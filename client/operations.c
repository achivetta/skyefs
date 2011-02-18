#include "common/skye_rpc.h"
#include "common/defaults.h"
#include "client.h"

#include <rpc/rpc.h>
#include <fuse.h>
#include <arpa/inet.h>
#include <errno.h>

/* FIXME: a lot of the paths passed to the skye_* functions should be const.  I
 * wonder if this could cause FUSE to have problems if we modify them. */

#define pvfs2errno(n) (-1)*(PVFS_get_errno_mapping(n))

/* TODO: should the structure we are storing in the fuse_file_info->fh also have
 * credentials? */

static PVFS_credentials credentials = {
    .uid = 1000,
    .gid = 1000
};

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

	while ( (*p) != '\0')
		p++; // Go to end of string
	while ( (*p) != '/' && p != path)
		p--; // Come back till '/'

    // Copy after slash till end into filename
	strncpy(fileName, p+1, MAX_FILENAME_LEN); 
	if (dirName) {
		if (path == p)
			strncpy(dirName, "/", 1);
		else {
            // Copy rest into dirpath 
			strncpy(dirName, path, (int)(p - path )); 
			dirName[(int)(p - path)] = '\0';
		}
	}
	return 0;
}

/** Updates parent_ref to point to the specified child */
static int lookup(PVFS_object_ref* ref, char* pathname)
{
	enum clnt_stat retval;
	skye_lookup result;

	retval = skye_rpc_lookup_1(*ref, pathname, &result, rpc_client);
	if (retval != RPC_SUCCESS) {
		clnt_perror (rpc_client, "RPC lookup failed");
        return -EIO;
	}

    if (result.errnum < 0)
        return result.errnum;

    /* Giga+: Add a section here for reading out the bitmap */

    *ref = result.skye_lookup_u.ref;

    return 0;
}

static int resolve(char* pathname, PVFS_object_ref* ref)
{

    PVFS_sysresp_lookup lk_response;
    int ret;

    memset(&lk_response, 0, sizeof(lk_response));

    ret = PVFS_sys_lookup(pvfs_fsid, (char *)"/", &credentials, &lk_response,
                          PVFS2_LOOKUP_LINK_NO_FOLLOW, PVFS_HINT_NULL);
    if ( ret < 0 )
        return pvfs2errno(ret);

    *ref = lk_response.ref;

    char *saveptr, *component;
    component = strtok_r(pathname, "/", &saveptr);
    while (component){
        ret = lookup(ref, component);
        if (ret < 0)
            return ret; 

        component = strtok_r(NULL, "/", &saveptr);
    }

    return 0;
}

static int pvfs_readdir(PVFS_object_ref *ref, void *buf, fuse_fill_dir_t filler)
{
    int ret;
    PVFS_sysresp_readdir rd_response;
    unsigned int pvfs_dirent_incount = 32; // reasonable chank size
    PVFS_ds_position token = 0;

    do {
        char *cur_file = NULL;
        unsigned int i;

        memset(&rd_response, 0, sizeof(PVFS_sysresp_readdir));
        ret = PVFS_sys_readdir(*ref, (!token ? PVFS_READDIR_START : token),
                                pvfs_dirent_incount, &credentials, &rd_response,
                                PVFS_HINT_NULL);
        if (ret < 0)
            return pvfs2errno(ret);

        for (i = 0; i < rd_response.pvfs_dirent_outcount; i++) {
            cur_file = rd_response.dirent_array[i].d_name;

            if (filler(buf, cur_file, NULL, 0))
                break;
        }
        token += rd_response.pvfs_dirent_outcount;

        if (rd_response.pvfs_dirent_outcount) {
            free(rd_response.dirent_array);
            rd_response.dirent_array = NULL;
        }

    } while(rd_response.pvfs_dirent_outcount == pvfs_dirent_incount);

    return 0;
}

int skye_readdir(const char * path, void * buf, fuse_fill_dir_t filler, off_t offset,
                 struct fuse_file_info *fi){
    (void)offset;
    (void)fi;

    PVFS_object_ref ref;
    int ret;

    if ( (ret = resolve(path, &ref)) < 0 )
        return ret;

    return pvfs_readdir(&ref, buf, filler);

    /* Giga+ version lies below 
    unsigned int pvfs_dirent_incount = 32; // reasonable chank size
    PVFS_ds_position token = 0;
    PVFS_sysresp_readdir rd_response;
    do {
        unsigned int i;

        memset(&rd_response, 0, sizeof(PVFS_sysresp_readdir));
        ret = PVFS_sys_readdir(ref, (!token ? PVFS_READDIR_START : token),
                               pvfs_dirent_incount, &credentials, &rd_response,
                               PVFS_HINT_NULL);
        if (ret < 0)
            return pvfs2errno(ret);

        for (i = 0; i < rd_response.pvfs_dirent_outcount; i++) {
            ref.handle = rd_response.dirent_array[i].handle; 
            // FIXME: error handleing
            pvfs_readdir(&ref, buf, filler);
        }
        token += rd_response.pvfs_dirent_outcount;

        if (rd_response.pvfs_dirent_outcount) {
            free(rd_response.dirent_array);
            rd_response.dirent_array = NULL;
        }

    } while (rd_response.pvfs_dirent_outcount == pvfs_dirent_incount);
    */
}

/* function body taken from pvfs2fuse.c */
static int pvfs_getattr(PVFS_object_ref *ref, struct stat *stbuf)
{
    PVFS_sysresp_getattr getattr_response;
    PVFS_sys_attr*	attrs;

    int			ret;
    int			perm_mode = 0;

    memset(&getattr_response,0, sizeof(PVFS_sysresp_getattr));

    ret = PVFS_sys_getattr(*ref, PVFS_ATTR_SYS_ALL_NOHINT, &credentials,
                           &getattr_response, PVFS_HINT_NULL);
    if ( ret < 0 )
        return pvfs2errno(ret);

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

    if (attrs->objtype == PVFS_TYPE_METAFILE) {
        if (attrs->mask & PVFS_ATTR_SYS_SIZE) {
            size_t inode_size = attrs->size;
            size_t rounded_up_size = (inode_size + (4096 - (inode_size % 4096)));

            stbuf->st_size = inode_size;
            stbuf->st_blocks = (unsigned long)(rounded_up_size / 512);
        }
    } else if ((attrs->objtype == PVFS_TYPE_SYMLINK) &&
             (attrs->link_target != NULL)) {
        stbuf->st_size = strlen(attrs->link_target);
    } else {
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
       if (is_root_handle(inode)) {
           inode->i_mode |= S_ISVTX;
       }
    */
    switch (attrs->objtype) {
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

    stbuf->st_dev = ref->fs_id;
    stbuf->st_ino = ref->handle;

    stbuf->st_rdev = 0;
    stbuf->st_blksize = 4096;

    PVFS_util_release_sys_attr(attrs);

    return 0;
}

int skye_getattr(const char *path, struct stat *stbuf)
{
    PVFS_object_ref ref;
    int ret;

    if ((ret = resolve(path, &ref)) < 0)
        return ret;

    if ((ret = pvfs_getattr(&ref, stbuf)) < 0)
        return ret;

    return 0;
}

int skye_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    PVFS_object_ref *ref = malloc(sizeof(PVFS_object_ref));

    char filename[MAX_FILENAME_LEN] = {0};
    char pathname[MAX_PATHNAME_LEN] = {0};

    get_path_components(path, filename, pathname);

    int ret;
    if ((ret = resolve(pathname, ref)) < 0)
        return ret;

    enum clnt_stat retval;
    skye_lookup result;

    retval = skye_rpc_create_1(*ref, filename, mode, &result, rpc_client);
    if (retval != RPC_SUCCESS) {
		clnt_perror (rpc_client, "RPC create failed");
        return -EIO;
	}

    if (result.errnum != 0)
        return result.errnum;

    fi->fh = (intptr_t) ref;

    return 0;
}

int skye_mkdir(const char * path, mode_t mode)
{
    PVFS_object_ref ref;

    char filename[MAX_FILENAME_LEN] = {0};
    char pathname[MAX_PATHNAME_LEN] = {0};

    get_path_components(path, filename, pathname);

    int ret;
    if ((ret = resolve(pathname, &ref)) < 0)
        return ret;

    enum clnt_stat retval;
    skye_result result;

    retval = skye_rpc_mkdir_1(ref, filename, mode, &result, rpc_client);
    if (retval != RPC_SUCCESS) {
		clnt_perror (rpc_client, "RPC create failed");
        return -EIO;
	}

    return result.errnum;
}

int skye_rename(const char *src_path, const char *dst_path)
{
    PVFS_object_ref src_ref;
    char src_name[MAX_FILENAME_LEN] = {0};
    char src_dir[MAX_PATHNAME_LEN] = {0};
    get_path_components(src_path, src_name, src_dir);

    PVFS_object_ref dst_ref;
    char dst_name[MAX_FILENAME_LEN] = {0};
    char dst_dir[MAX_PATHNAME_LEN] = {0};
    get_path_components(dst_path, dst_name, dst_dir);

    int ret;
    if ((ret = resolve(src_dir, &src_ref)) < 0)
        return ret;
    if ((ret = resolve(dst_dir, &dst_ref)) < 0)
        return ret;

    enum clnt_stat retval;
    skye_result result;
    retval = skye_rpc_rename_1(src_name, src_ref, dst_name, dst_ref, &result, rpc_client);
    if (retval != RPC_SUCCESS) {
		clnt_perror (rpc_client, "RPC create failed");
        return -EIO;
	}

    return result.errnum;
}

int skye_remove(const char *path){
    PVFS_object_ref ref;
    char parent[MAX_PATHNAME_LEN], filename[MAX_FILENAME_LEN];
    int ret;

    get_path_components(path, filename, parent);

    if ((ret = resolve(parent, &ref)) < 0)
        return ret;

    ret = PVFS_sys_remove(filename, ref, &credentials, PVFS_HINT_NULL);
    if (ret < 0)
        return pvfs2errno(ret);

    return 0;
}

int skye_unlink(const char *path){
    return skye_remove(path);
}

int skye_rmdir(const char *path){
    return skye_remove(path);
}

int skye_chmod(const char *path, mode_t mode){
    PVFS_object_ref ref;
    PVFS_sys_attr new_attr;
    int ret;

    if ((ret = resolve(path, &ref)) < 0)
        return ret;

    new_attr.perms = mode;
    new_attr.mask = PVFS_ATTR_SYS_PERM;

    ret = PVFS_sys_setattr(ref, new_attr, &credentials, PVFS_HINT_NULL);
    if (ret < 0)
        return pvfs2errno(ret);

    return 0;
}

int skye_chown(const char *path, uid_t uid, gid_t gid){
    PVFS_object_ref ref;
    PVFS_sys_attr new_attr;
    int ret;

    if ((ret = resolve(path, &ref)) < 0)
        return ret;

    new_attr.owner = uid;
    new_attr.group = gid;
    new_attr.mask = PVFS_ATTR_SYS_UID | PVFS_ATTR_SYS_GID;

    ret = PVFS_sys_setattr(ref, new_attr, &credentials, PVFS_HINT_NULL);
    if (ret < 0)
        return pvfs2errno(ret);

    return 0;
}

int skye_truncate(const char *path, off_t size){
    PVFS_object_ref ref;
    int ret;

    if ((ret = resolve(path, &ref)) < 0)
        return ret;

    ret = PVFS_sys_truncate(ref, size, &credentials, PVFS_HINT_NULL);
    if (ret < 0)
        return pvfs2errno(ret);

    return 0;
}

int skye_utime(const char *path, struct utimbuf *timbuf){
    PVFS_object_ref ref;
    PVFS_sys_attr new_attr;
    int ret;

    if ((ret = resolve(path, &ref)) < 0)
        return ret;

    new_attr.atime = (PVFS_time)timbuf->actime;
    new_attr.mtime = (PVFS_time)timbuf->modtime;
    new_attr.mask = PVFS_ATTR_SYS_ATIME | PVFS_ATTR_SYS_MTIME;

    ret = PVFS_sys_setattr(ref, new_attr, &credentials, PVFS_HINT_NULL);
    if (ret < 0)
        return pvfs2errno(ret);

    return 0;
}
