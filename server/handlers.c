#include "common/trace.h"
#include "common/skye_rpc.h"
#include "server.h"
#include "common/defaults.h"

#include <assert.h>
#include <stdbool.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>

#include <pvfs2-util.h>
#include <pvfs2-sysint.h>

typedef struct {
	  PVFS_object_ref	ref;
	  PVFS_credentials	creds;
} pvfs_handle_t;

/* FIXME */
static void pvfs_gen_credentials(
   PVFS_credentials *credentials)
{
   //credentials->uid = fuse_get_context()->uid;
   //credentials->gid = fuse_get_context()->gid;
   credentials->uid = 1000;
   credentials->gid = 1000;
}

/* FIXME: this will segfault if "./" is specified as the path */
static int pvfs_lookup( const char *path, pvfs_handle_t *pfh, 
				   int32_t follow_link )
{
   PVFS_sysresp_lookup lk_response;
   int ret;

   /* we don't have to do a PVFS_util_resolve
	* because FUSE resolves the path for us
	*/

   pvfs_gen_credentials(&pfh->creds);

   memset(&lk_response, 0, sizeof(lk_response));
   ret = PVFS_sys_lookup(srv_settings.fs_id, (char *)path, &pfh->creds,
                         &lk_response, follow_link, PVFS_HINT_NULL);
   if ( ret < 0 ) {
	  return ret;
   }

   pfh->ref.handle = lk_response.ref.handle;
   pfh->ref.fs_id  = srv_settings.fs_id;

   return 0;
}

static int pvfs_stat(pvfs_handle_t *pfhp, struct stat *stbuf) {
   PVFS_sysresp_getattr getattr_response;
   PVFS_sys_attr*	attrs;
   int			ret;
   int			perm_mode = 0;

   memset(&getattr_response,0, sizeof(PVFS_sysresp_getattr));
    
   ret = PVFS_sys_getattr(pfhp->ref, 
                          PVFS_ATTR_SYS_ALL_NOHINT,
                          (PVFS_credentials *) &pfhp->creds, 
                          &getattr_response,PVFS_HINT_NULL);
   if ( ret < 0 )
	  return -ret;
   
   memset(stbuf, 0, sizeof(struct stat));

   /* Code copied from kernel/linux-2.x/pvfs2-utils.c */

   /*
	 arbitrarily set the inode block size; FIXME: we need to
	 resolve the difference between the reported inode blocksize
	 and the PAGE_CACHE_SIZE, since our block count will always
	 be wrong.

	 For now, we're setting the block count to be the proper
	 number assuming the block size is 512 bytes, and the size is
	 rounded up to the nearest 4K.  This is apparently required
	 to get proper size reports from the 'du' shell utility.

   */

   attrs = &getattr_response.attr;
   
   if (attrs->objtype == PVFS_TYPE_METAFILE)
   {
	  if (attrs->mask & PVFS_ATTR_SYS_SIZE)
	  {
		 size_t inode_size = attrs->size;
		 size_t rounded_up_size = (inode_size + (4096 - (inode_size % 4096)));

		 stbuf->st_size = inode_size;
		 stbuf->st_blocks = (unsigned long)(rounded_up_size / 512);
	  }
   }
   else if ((attrs->objtype == PVFS_TYPE_SYMLINK) &&
			(attrs->link_target != NULL))
   {
	  stbuf->st_size = strlen(attrs->link_target);
   }
   else
   {
      /* what should this be??? */
	  unsigned long PAGE_CACHE_SIZE = 4096;
	  stbuf->st_blocks = (unsigned long)(PAGE_CACHE_SIZE / 512);
	  stbuf->st_size = PAGE_CACHE_SIZE;
   }

   stbuf->st_uid = attrs->owner;
   stbuf->st_gid = attrs->group;

   stbuf->st_atime = (time_t)attrs->atime;
   stbuf->st_mtime = (time_t)attrs->mtime;
   stbuf->st_ctime = (time_t)attrs->ctime;

   stbuf->st_mode = 0;
   if (attrs->perms & PVFS_O_EXECUTE)
	  perm_mode |= S_IXOTH;
   if (attrs->perms & PVFS_O_WRITE)
	  perm_mode |= S_IWOTH;
   if (attrs->perms & PVFS_O_READ)
	  perm_mode |= S_IROTH;

   if (attrs->perms & PVFS_G_EXECUTE)
	  perm_mode |= S_IXGRP;
   if (attrs->perms & PVFS_G_WRITE)
	  perm_mode |= S_IWGRP;
   if (attrs->perms & PVFS_G_READ)
	  perm_mode |= S_IRGRP;

   if (attrs->perms & PVFS_U_EXECUTE)
	  perm_mode |= S_IXUSR;
   if (attrs->perms & PVFS_U_WRITE)
	  perm_mode |= S_IWUSR;
   if (attrs->perms & PVFS_U_READ)
	  perm_mode |= S_IRUSR;

   if (attrs->perms & PVFS_G_SGID)
      perm_mode |= S_ISGID;

   /* Should we honor the suid bit of the file? */
   /* FIXME should we check the file system suid flag */
   if ( /* get_suid_flag(inode) == 1 && */ (attrs->perms & PVFS_U_SUID))
	  perm_mode |= S_ISUID;

   stbuf->st_mode |= perm_mode;

   /* FIXME special case: mark the root inode as sticky
	  if (is_root_handle(inode))
	  {
	  inode->i_mode |= S_ISVTX;
	  }
   */
   switch (attrs->objtype)
   {
	  case PVFS_TYPE_METAFILE:
		 stbuf->st_mode |= S_IFREG;
		 break;
	  case PVFS_TYPE_DIRECTORY:
		 stbuf->st_mode |= S_IFDIR;
		 /* NOTE: we have no good way to keep nlink consistent for 
		  * directories across clients; keep constant at 1.  Why 1?  If
		  * we go with 2, then find(1) gets confused and won't work
		  * properly withouth the -noleaf option */
		 stbuf->st_nlink = 1;
		 break;
	  case PVFS_TYPE_SYMLINK:
		 stbuf->st_mode |= S_IFLNK;
		 break;
	  default:
		 break;
   }

   stbuf->st_dev = pfhp->ref.fs_id;
   stbuf->st_ino = pfhp->ref.handle;

   stbuf->st_rdev = 0;
   stbuf->st_blksize = 4096;

   PVFS_util_release_sys_attr(attrs);
    
   return 0;
}

bool_t skye_rpc_init_1_svc(bool_t *result, struct svc_req *rqstp)
{
    assert(result);

    dbg_msg(log_fp, "[%s] recv:init()", __func__);

    *result = true;

    return true;
}

#define MAX_NUM_DIRENTS 25

bool_t skye_rpc_readdir_1_svc(skye_pathname path, skye_dirlist *result,  
                              struct svc_req *rqstp)
{
    dbg_msg(log_fp, "[%s] recv:readdir(%s)", __func__, path);
    assert(result);

    int ret;
    pvfs_handle_t pfh;

    ret = pvfs_lookup( path, &pfh, PVFS2_LOOKUP_LINK_FOLLOW );
    if ( ret < 0 ){
        result->errnum = -ret;
        return true;
    }

    result->errnum = 0;
    result->skye_dirlist_u.dlist = NULL;

    /* while we keep maxing out our response size... */
    int pvfs_dirent_incount = MAX_NUM_DIRENTS;
    PVFS_ds_position token = 0;
    PVFS_sysresp_readdir rd_response;
    do {
        memset(&rd_response, 0, sizeof(PVFS_sysresp_readdir));

        ret = PVFS_sys_readdir(pfh.ref, (!token ? PVFS_READDIR_START : token),
                               pvfs_dirent_incount, &pfh.creds, &rd_response,
                               PVFS_HINT_NULL);
        if(ret < 0){
            result->errnum = -ret;
            return true;
        }

        /* for each returned file */
        for(int i = 0; i < rd_response.pvfs_dirent_outcount; i++) {
            /* create a new dnode */
            skye_dnode *dnode = calloc(sizeof(skye_dnode),1);

            char *cur_file = rd_response.dirent_array[i].d_name;

            /* populate filename */
            dnode->name = strdup(cur_file);
            if (dnode->name == NULL){
                dbg_msg(log_fp, "[%s] Unable to duplicate string \"%s\". ",
                        __func__, cur_file);
                free(dnode);
                continue;
            }

            /* insert at front of list */
            dnode->next = result->skye_dirlist_u.dlist;
            result->skye_dirlist_u.dlist = dnode;
        }

        token += rd_response.pvfs_dirent_outcount;

        if (rd_response.pvfs_dirent_outcount) {
            free(rd_response.dirent_array);
            rd_response.dirent_array = NULL;
        }
    } while (rd_response.pvfs_dirent_outcount == pvfs_dirent_incount);

	return true;
}

bool_t skye_rpc_getattr_1_svc(skye_pathname path, skye_stat *result,  
                              struct svc_req *rqstp)
{
    dbg_msg(log_fp, "[%s] recv:getattr(%s)", __func__, path);
    assert(result);

    int ret;
    pvfs_handle_t pfh;

    ret = pvfs_lookup(path, &pfh, PVFS2_LOOKUP_LINK_NO_FOLLOW);
    if (ret < 0){
        result->errnum = -ret;
        return true;
    }

    ret = pvfs_stat(&pfh, &result->skye_stat_u.stbuf);
    if (ret < 0){
        result->errnum = ret;
        return true;
    }

    result->errnum = 0;
    return true;
}

int skye_rpc_prog_1_freeresult (SVCXPRT *transp, xdrproc_t xdr_result, 
                                caddr_t result)
{
    xdr_free (xdr_result, result);

    /*
     * Insert additional freeing code here, if needed
     */

    return 1;
}

