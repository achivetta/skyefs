#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <utime.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/time.h>

#include <pvfs2.h>
#include <pvfs2-sysint.h>
#include <pvfs2-debug.h>

#define THREADS 10
#define FILES 5000

typedef struct {
	  PVFS_object_ref	ref;
	  PVFS_credentials	creds;
} pvfs_fuse_handle_t;

struct pvfs2fuse {
	  char	*fs_spec;
	  char	*scratch_dir;
	  PVFS_fs_id	fs_id;
	  struct PVFS_sys_mntent mntent;
};

pthread_mutex_t global_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_barrier_t global_barrier;

volatile int do_list = 0;

static struct pvfs2fuse pvfs2fuse;

#define PVFS_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))
#define THIS_PVFS_VERSION \
     PVFS_VERSION(PVFS2_VERSION_MAJOR, PVFS2_VERSION_MINOR, PVFS2_VERSION_SUB)

#if THIS_PVFS_VERSION > PVFS_VERSION(2,6,3)
#define PVFS_ERROR_TO_ERRNO_N(x) (-1)*PVFS_ERROR_TO_ERRNO(x)
#else
#define PVFS_ERROR_TO_ERRNO_N(x) PVFS_ERROR_TO_ERRNO(x)
#endif

static void pvfs_fuse_gen_credentials(
   PVFS_credentials *credentials)
{
   credentials->uid = 1000;
   credentials->gid = 1000;
}

static int get_path_components(const char *path, char *fileName, char *dirName)
{
	const char *p = path;
	if (!p || !fileName)
		return -1;

	if (strcmp(path, "/") == 0) {
		strcpy(fileName, "/");
		if (dirName)
			strcpy(dirName, "/");

		return 0;
	}

    int pathlen = strlen(path);

    p += pathlen;
	while ( (*p) != '/' && p != path)
		p--; // Come back till '/'

    // Copy after slash till end into filename
	strcpy(fileName, p+1); 
	if (dirName) {
		if (path == p)
			strncpy(dirName, "/", 2);
		else {
            // Copy rest into dirpath 
			strncpy(dirName, path, (int)(p - path )); 
			dirName[(int)(p - path)] = '\0';
		}
	}

	return 0;
}

static int lookup( const char *path, pvfs_fuse_handle_t *pfh, 
				   int32_t follow_link )
{
   PVFS_sysresp_lookup lk_response;
   int			ret;

   /* we don't have to do a PVFS_util_resolve
	* because FUSE resolves the path for us
	*/

   pvfs_fuse_gen_credentials(&pfh->creds);

   memset(&lk_response, 0, sizeof(lk_response));
   ret = PVFS_sys_lookup(pvfs2fuse.fs_id, 
						 (char *)path,
						 &pfh->creds, 
						 &lk_response, 
						 follow_link,
                         PVFS_HINT_NULL);
   if ( ret < 0 ) {
	  return ret;
   }

   pfh->ref.handle = lk_response.ref.handle;
   pfh->ref.fs_id  = pvfs2fuse.fs_id;

   return 0;
}

#if 0
static int pvfs_fuse_mkdir(const char *path, mode_t mode)
{
   int rc;
   PVFS_sys_attr attr;
   char parent[PVFS_NAME_MAX];
   char dirname[PVFS_SEGMENT_MAX];
   pvfs_fuse_handle_t	parent_pfh;

   PVFS_sysresp_mkdir resp_mkdir;

   get_path_components(path, dirname, parent);

   lookup( parent, &parent_pfh, PVFS2_LOOKUP_LINK_FOLLOW );

   /* Set attributes */
   memset(&attr, 0, sizeof(PVFS_sys_attr));
   attr.owner = parent_pfh.creds.uid;
   attr.group = parent_pfh.creds.gid;
   attr.perms = mode;
   attr.mask = PVFS_ATTR_SYS_ALL_SETABLE;

   rc = PVFS_sys_mkdir(dirname,
					   parent_pfh.ref,
					   attr,
					   &parent_pfh.creds,
					   &resp_mkdir,
                       PVFS_HINT_NULL);
   if (rc)
   {
	  return PVFS_ERROR_TO_ERRNO_N( rc );
   }

   return 0;
}
#endif

#if 0
static int pvfs_fuse_remove( const char *path )
{
   int rc;
   char parent[PVFS_NAME_MAX];
   char filename[PVFS_SEGMENT_MAX];
   pvfs_fuse_handle_t	parent_pfh;

   get_path_components(path, filename, parent);

   lookup( parent, &parent_pfh, PVFS2_LOOKUP_LINK_FOLLOW );

   rc = PVFS_sys_remove(filename, parent_pfh.ref, &parent_pfh.creds, PVFS_HINT_NULL);
   if (rc)
   {
	  return PVFS_ERROR_TO_ERRNO_N( rc );
   }

   return 0;
}
#endif

static int pvfs_fuse_create(const char *path, mode_t mode)
{
   int rc;
   PVFS_sys_attr attr;
   char directory[PVFS_NAME_MAX];
   char filename[PVFS_SEGMENT_MAX];
   pvfs_fuse_handle_t	dir_pfh, *pfhp;

   PVFS_sysresp_create resp_create;

   get_path_components(path, filename, directory);

   /* this should fail */
   rc = lookup( path, &dir_pfh, PVFS2_LOOKUP_LINK_FOLLOW );
   if (rc == 0)
      return 0;

   rc = lookup( directory, &dir_pfh, PVFS2_LOOKUP_LINK_FOLLOW );
   if ( rc < 0 )
	  return PVFS_ERROR_TO_ERRNO_N( rc );

   /* Set attributes */
   memset(&attr, 0, sizeof(PVFS_sys_attr));
   attr.owner = dir_pfh.creds.uid;
   attr.group = dir_pfh.creds.gid;
   attr.perms = mode;
   attr.atime = time(NULL);
   attr.mtime = attr.atime;
   attr.mask = PVFS_ATTR_SYS_ALL_SETABLE;
   attr.dfile_count = 0;

   rc = PVFS_sys_create(filename,
						dir_pfh.ref,
						attr,
						&dir_pfh.creds,
						NULL,
						&resp_create, PVFS_SYS_LAYOUT_DEFAULT, PVFS_HINT_NULL);
   if (rc)
   {
      /* FIXME
       * the PVFS2 server code returns a ENOENT instead of an EACCES
       * because it does a ACL lookup for the system.posix_acl_access
       * which returns a ENOENT from the TROVE DBPF and that error is
       * just passed up in prelude_check_acls (server/prelude.c).  I'm
       * not sure that's the right thing to do.
       */
	  if ( rc == -PVFS_ENOENT ) 
	  {
		 return -EACCES;
	  }
	  return PVFS_ERROR_TO_ERRNO_N( rc );
   }

   pfhp = (pvfs_fuse_handle_t *)malloc( sizeof( pvfs_fuse_handle_t ) );
   if (pfhp == NULL)
   {
	  return -ENOMEM;
   }

   pfhp->ref = resp_create.ref;
   pfhp->creds = dir_pfh.creds;

   return 0;
}

volatile int curfile = 0;
volatile int lastfile = 0;

void alarmhandler(int signal){
    (void)signal;

    int now = curfile;
    printf("Created %d files in the last second.\n", now - lastfile);
    lastfile = now;
}

void *thread_list(void *arg)
{
    int ret;
    long locking = (long)arg;

    pthread_barrier_wait(&global_barrier);

    struct itimerval timer;
    timer.it_value.tv_sec = 1;
    timer.it_value.tv_usec = 0;
    timer.it_interval.tv_sec = 1;
    timer.it_interval.tv_usec = 0;
    setitimer(ITIMER_REAL, &timer, NULL);

    sleep(2);

    do {

        PVFS_sysresp_readdir rd_response;
        unsigned int pvfs_dirent_incount = 500; // reasonable chank size
        PVFS_ds_position token = 0;
        pvfs_fuse_handle_t	dir_pfh;

        ret = lookup( pvfs2fuse.scratch_dir, &dir_pfh, PVFS2_LOOKUP_LINK_FOLLOW );
        if (ret < 0){
            printf("couldn't lookup parent directory.\n");
            continue;
        }

        if (locking == 2)
            pthread_mutex_lock(&global_mutex);

        struct timeval start, end;
        gettimeofday(&start, NULL);
        do {
            memset(&rd_response, 0, sizeof(PVFS_sysresp_readdir));

            ret = PVFS_sys_readdir(dir_pfh.ref, (!token ? PVFS_READDIR_START : token),
                                   pvfs_dirent_incount, &dir_pfh.creds, &rd_response,
                                   PVFS_HINT_NULL);

            if (ret < 0){
                printf("couldn't list parent directory.\n");
                continue;
            }

            if (!token)
                token = rd_response.pvfs_dirent_outcount - 1;
            else
                token += rd_response.pvfs_dirent_outcount;

            if (rd_response.pvfs_dirent_outcount) {
                free(rd_response.dirent_array);
                rd_response.dirent_array = NULL;
            }

        } while(rd_response.pvfs_dirent_outcount == pvfs_dirent_incount);

        gettimeofday(&end, NULL);

        if (locking == 2)
            pthread_mutex_unlock(&global_mutex);

        printf("Listed directory in %ldms.\n",
               (end.tv_sec-start.tv_sec)*1000 + (end.tv_usec-start.tv_usec)/1000);

    } while (do_list);

    return NULL;
}

void *thread_main(void *arg)
{
    long locking = (long)arg;

    pthread_t tid = pthread_self();

    pthread_barrier_wait(&global_barrier); 

    struct timeval start, end;

    gettimeofday(&start, NULL);

    int i = 0;
    do{
        char filename[200]; sprintf(filename, "%s/t%lu-f%d", pvfs2fuse.scratch_dir, (unsigned long)tid, i++);

retry: 
        if (locking)
            pthread_mutex_lock(&global_mutex);

        int rc = pvfs_fuse_create(filename, 0666);

        if (locking)
            pthread_mutex_unlock(&global_mutex);

        if (rc < 0){
            printf("failed to create file (%d).\n", rc);
            i--;
            goto retry;
        }
    } while (__sync_fetch_and_add(&curfile,1) < FILES);

    gettimeofday(&end, NULL);

    printf("Thread %lu completed %d create()s in %ldms with locking set to %ld.\n",
           (unsigned long)tid, i,
           (end.tv_sec-start.tv_sec)*1000 + (end.tv_usec-start.tv_usec)/1000, 
           locking);

    do_list = 0;
    return NULL;
}

static void usage(const char *progname)
{
   fprintf(stderr, "usage: %s fs_spec scratch_dir global_locks\n", progname);
}

static void do_test(long locks)
{
  int i;
  pthread_t tids[THREADS];

  pthread_barrier_init(&global_barrier, NULL, THREADS + 1);


  printf("Running test with global lock = %ld.\n", locks);
  for (i = 0; i < THREADS; i ++)
      pthread_create(&tids[i], NULL, thread_main, (void*)locks);

  do_list = 1;
  pthread_t list_tid;
  pthread_create(&list_tid, NULL, thread_list, (void*)locks);

  for (i = 0; i < THREADS; i ++)
      pthread_join(tids[i], NULL);

  pthread_join(list_tid, NULL);

  printf("All done.\n");
}

int main(int argc, char *argv[])
{
   int ret;

   if (argc != 4){
       usage(argv[0]);
       exit(1);
   }

  pvfs2fuse.fs_spec = argv[1];
  pvfs2fuse.scratch_dir = argv[2];

  struct PVFS_sys_mntent *me = &pvfs2fuse.mntent;
  char *cp;
  int cur_server;

  /* the following is copied from PVFS_util_init_defaults()
     in fuse/lib/pvfs2-util.c */

  /* initialize pvfs system interface */
  ret = PVFS_sys_initialize(GOSSIP_NO_DEBUG);
  if (ret < 0)
  {
     return(ret);
  }

  /* the following is copied from PVFS_util_parse_pvfstab()
     in fuse/lib/pvfs2-util.c */
  memset( me, 0, sizeof(pvfs2fuse.mntent) );

  /* Enable integrity checks by default */
  me->integrity_check = 1;
  /* comma-separated list of ways to contact a config server */
  me->num_pvfs_config_servers = 1;

  for (cp=pvfs2fuse.fs_spec; *cp; cp++)
     if (*cp == ',')
        ++me->num_pvfs_config_servers;

  /* allocate room for our copies of the strings */
  me->pvfs_config_servers =
     malloc(me->num_pvfs_config_servers *
            sizeof(*me->pvfs_config_servers));
  if (!me->pvfs_config_servers)
     exit(-1);
  memset(me->pvfs_config_servers, 0,
         me->num_pvfs_config_servers * sizeof(*me->pvfs_config_servers));

  me->mnt_dir = NULL;
  me->mnt_opts = NULL;
  me->encoding = PVFS2_ENCODING_DEFAULT;

  cp = pvfs2fuse.fs_spec;
  cur_server = 0;
  for (;;) {
     char *tok;
     int slashcount;
     char *slash;
     char *last_slash;

     tok = strsep(&cp, ",");
     if (!tok) break;

     slash = tok;
     slashcount = 0;
     while ((slash = index(slash, '/')))
     {
        slash++;
        slashcount++;
     }
     if (slashcount != 3)
     {
        fprintf(stderr,"Error: invalid FS spec: %s\n",
                pvfs2fuse.fs_spec);
        exit(-1);
     }

     /* find a reference point in the string */
     last_slash = rindex(tok, '/');
     *last_slash = '\0';

     /* config server and fs name are a special case, take one 
      * string and split it in half on "/" delimiter
      */
     me->pvfs_config_servers[cur_server] = strdup(tok);
     if (!me->pvfs_config_servers[cur_server])
        exit(-1);

     ++last_slash;

     if (cur_server == 0) {
        me->pvfs_fs_name = strdup(last_slash);
        if (!me->pvfs_fs_name)
           exit(-1);
     } else {
        if (strcmp(last_slash, me->pvfs_fs_name) != 0) {
           fprintf(stderr,
                   "Error: different fs names in server addresses: %s\n",
                   pvfs2fuse.fs_spec);
           exit(-1);
        }
     }
     ++cur_server;
  }

  me->flowproto = FLOWPROTO_DEFAULT;

  ret = PVFS_sys_fs_add(me);
  if( ret < 0 )
  {
      PVFS_perror("Could not add mnt entry", ret);
      return(-1);
  }
  pvfs2fuse.fs_id = me->fs_id;

  signal(SIGALRM, alarmhandler);
  do_test(atol(argv[3]));

  return 0;
}
