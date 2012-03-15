#include "common/skye_rpc.h"
#include "common/defaults.h"
#include "common/connection.h"
#include "common/trace.h"
#include "cache.h"
#include "client.h"

#include <rpc/rpc.h>
#include <fuse_lowlevel.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pvfs2-util.h>
#include <assert.h>

#define min(x1,x2) ((x1) > (x2))? (x2):(x1)

/* FIXME: see pvfs:src/apps/admin/pvfs2-cp.c for how to do permissions correctly
 * TODO: should the structure we are storing in the fuse_file_info->fh also have
 * credentials? */

/* DANGER: depends on internals of PVFS struct
 * FIXME should this use PVFS_util_gen_credentials()? */
static void gen_credentials(PVFS_credentials *credentials, fuse_req_t req)
{
    credentials->uid = fuse_req_ctx(req)->uid;
    credentials->gid = fuse_req_ctx(req)->gid;
}

static PVFS_handle inode2handle(PVFS_credentials *credentials, fuse_ino_t ino)
{
    static PVFS_handle pvfs_root_handle;
    if (ino == FUSE_ROOT_ID) {
        if (pvfs_root_handle == 0){
            PVFS_sysresp_lookup lk_response;
            memset(&lk_response, 0, sizeof(lk_response));
            int ret = PVFS_sys_lookup(pvfs_fsid, (char *)"/", credentials, &lk_response,
                                      PVFS2_LOOKUP_LINK_NO_FOLLOW, PVFS_HINT_NULL);
            if ( ret < 0 )
                return -1 * pvfs2errno(ret);
            pvfs_root_handle = lk_response.ref.handle;
        }
        return pvfs_root_handle;
    } else {
        return ino;
    }
}

static int get_server_for_file(struct skye_directory *dir, const char *name)
{
    return giga_get_server_for_file(&dir->mapping, name);
}

//XXX: need a second parameter which is the bitmap returned in the RPC reply
static void update_client_mapping(struct skye_directory *dir, struct giga_mapping_t *mapping)
{
    giga_update_cache(&dir->mapping,mapping);
}

/** Updates parent_ref to point to the specified child */
static int lookup(PVFS_credentials *credentails, PVFS_object_ref* ref, char* pathname)
{
    int ret = 0, server_id;
	enum clnt_stat retval;
	skye_lookup result;

    if (strlen(pathname) >= MAX_FILENAME_LEN)
        return -ENAMETOOLONG;

    struct skye_directory *dir = cache_fetch(ref);
    if (!dir)
        return -EIO;
    
bitmap: 
    server_id = get_server_for_file(dir, pathname);
    CLIENT *rpc_client = get_connection(server_id);

	retval = skye_rpc_lookup_1(*credentails, *ref, pathname, &result, rpc_client);
	if (retval != RPC_SUCCESS) {
		clnt_perror (rpc_client, "RPC lookup failed");
        ret = -EIO;
        goto exit;
	}

    if (result.errnum == -EAGAIN){
        update_client_mapping(dir, &result.skye_lookup_u.bitmap);
        goto bitmap;
    } else if (result.errnum < 0){
        ret = result.errnum;
        goto exit;
    } else {
        ret = 0;
    }

    /* Giga+: Add a section here for reading out the bitmap */

    memcpy(ref, &result.skye_lookup_u.ref, sizeof(PVFS_object_ref));

 exit:
    cache_return(dir);

    return ret;
}

#if 0
/** Updates parent_ref to point to the bucket of specified child */
static int partition(PVFS_credentials *credentails, PVFS_object_ref* ref, char* pathname)
{
    int ret = 0, server_id;
	enum clnt_stat retval;
	skye_lookup result;

    if (strlen(pathname) >= MAX_FILENAME_LEN)
        return -ENAMETOOLONG;

    struct skye_directory *dir = cache_fetch(ref);
    if (!dir)
        return -EIO;
    
bitmap: 
    server_id = get_server_for_file(dir, pathname);
    CLIENT *rpc_client = get_connection(server_id);

	retval = skye_rpc_partition_1(*credentails, *ref, pathname, &result, rpc_client);
	if (retval != RPC_SUCCESS) {
		clnt_perror (rpc_client, "RPC lookup failed");
        ret = -EIO;
        goto exit;
	}

    if (result.errnum == -EAGAIN){
        update_client_mapping(dir, &result.skye_lookup_u.bitmap);
        goto bitmap;
    } else if (result.errnum < 0){
        ret = result.errnum;
        goto exit;
    } else {
        ret = 0;
    }

    /* Giga+: Add a section here for reading out the bitmap */

    memcpy(ref, &result.skye_lookup_u.ref, sizeof(PVFS_object_ref));

 exit:
    cache_return(dir);

    return ret;
}

static int resolve(PVFS_credentials *credentials, const char* pathname, PVFS_object_ref* ref)
{

    PVFS_sysresp_lookup lk_response;
    int ret;

    memset(&lk_response, 0, sizeof(lk_response));

    ret = PVFS_sys_lookup(pvfs_fsid, (char *)"/", credentials, &lk_response,
                          PVFS2_LOOKUP_LINK_NO_FOLLOW, PVFS_HINT_NULL);
    if ( ret < 0 )
        return pvfs2errno(ret);

    *ref = lk_response.ref;

    char *saveptr, *component;
    char *pathname_copy = strdup(pathname); /* since fuse passes us a const */
    component = strtok_r(pathname_copy, "/", &saveptr);
    while (component){
        ret = lookup(credentials, ref, component);
        if (ret < 0){
            free(pathname_copy);
            return ret; 
        }

        component = strtok_r(NULL, "/", &saveptr);
    }


    return 0;
}
#endif

struct direntbuf {
    char *buf;
    size_t size;
};

void skye_ll_opendir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
    (void)ino;
    fi->fh = (uintptr_t)calloc(1,sizeof(struct direntbuf));
    fuse_reply_open(req, fi);
}
void skye_ll_releasedir (fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
    (void)req;
    (void)ino;
    if (fi->fh && ((struct direntbuf*)fi->fh)->buf)
        free(((struct direntbuf*)fi->fh)->buf);
    if (fi->fh)
        free((void*)fi->fh);
}

void skye_ll_readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t offset,
                     struct fuse_file_info *fi)
{
    struct direntbuf *db = (struct direntbuf*)fi->fh;

    if (db->buf && offset > 0 && (unsigned long)offset < db->size)
    {
        fuse_reply_buf(req, db->buf, min(db->size - offset, size));
        return;
    } else if (db->buf && offset > 0 && (unsigned long)offset >= db->size) {
        fuse_reply_buf(req, NULL, 0);
        memset(db, 0, sizeof(struct direntbuf));
        return;
    }

    PVFS_credentials credentials; gen_credentials(&credentials, req);
    PVFS_object_ref ref; ref.handle = inode2handle(&credentials, ino); ref.fs_id = pvfs_fsid;

    int add_to_buf(void *ctx, PVFS_dirent dirent){
        (void)ctx;
        /* FIXME double instead of realloc each time */
        struct stat stbuf; memset(&stbuf, 0, sizeof(stbuf));
        size_t oldsize = db->size;
        db->size += fuse_add_direntry(req, NULL, 0, dirent.d_name, NULL, 0);
        char *newp = realloc(db->buf, db->size);
        if (!newp) {
            return -1;
        }
        db->buf = newp;
        stbuf.st_ino = dirent.handle;
        fuse_add_direntry(req, db->buf + oldsize, db->size - oldsize, dirent.d_name, &stbuf,
                          db->size);
        return 0;
    }

    int recurse(void *ctx, PVFS_dirent dirent){
        /* TODO could we use this anywhere else? */
        if (dirent.d_name[0] != 'p')
            return 0;

        int (*callback)(void *, PVFS_dirent) = ctx;
        PVFS_object_ref ref; ref.fs_id = pvfs_fsid; ref.handle = dirent.handle;
        return pvfs_readdir(NULL, &credentials, &ref, callback);
    }

    pvfs_readdir(add_to_buf, &credentials, &ref, recurse);

    fuse_reply_buf(req, db->buf, min(db->size, size));
}

/* function body taken from pvfs2fuse.c */
static int pvfs_getattr(PVFS_credentials *credentials, PVFS_object_ref *ref, struct stat *stbuf)
{
    PVFS_sysresp_getattr getattr_response;
    PVFS_sys_attr*	attrs;

    int			ret;
    int			perm_mode = 0;

    memset(&getattr_response,0, sizeof(PVFS_sysresp_getattr));

    ret = PVFS_sys_getattr(*ref, PVFS_ATTR_SYS_ALL_NOHINT, credentials,
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
            break;
        case PVFS_TYPE_SYMLINK:
            stbuf->st_mode |= S_IFLNK;
            break;
        default:
            break;
    }

    /* NOTE: we have no good way to keep nlink consistent for 
     * directories across clients; keep constant at 1.  Why 1?  If
     * we go with 2, then find(1) gets confused and won't work
     * properly withouth the -noleaf option */
    stbuf->st_nlink = 1;

    stbuf->st_dev = ref->fs_id;
    stbuf->st_ino = ref->handle;

    stbuf->st_rdev = 0;
    stbuf->st_blksize = 4096;

    PVFS_util_release_sys_attr(attrs);

    return 0;
}

void skye_ll_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
    PVFS_credentials credentials; gen_credentials(&credentials, req);
    PVFS_object_ref ref; ref.handle = inode2handle(&credentials, ino); ref.fs_id = pvfs_fsid;
    struct stat stbuf; memset(&stbuf, 0, sizeof(stbuf));
    int ret;
    (void)fi;

    if ((ret = pvfs_getattr(&credentials, &ref, &stbuf)) < 0)
        fuse_reply_err(req, ENOENT);
    else
        fuse_reply_attr(req, &stbuf, 10);
}

int generate_fuse_entry(PVFS_credentials *credentials, struct fuse_entry_param *e, PVFS_object_ref *ref)
{
    memset(e, 0, sizeof(struct fuse_entry_param));
    e->ino = ref->handle;
    e->attr_timeout = 1.0;
    e->entry_timeout = 1.0;
    return pvfs_getattr(credentials, ref, &(e->attr));
}

/**
 * Look up a directory entry by name and get its attributes.
 *
 * Valid replies:
 *   fuse_reply_entry
 *   fuse_reply_err
 *
 * @param req request handle
 * @param parent inode number of the parent directory
 * @param name the name to look up
 */
/* FIXME: isn't there a way to both lookup and stat with PVFS? */
void skye_ll_lookup(fuse_req_t req, fuse_ino_t parent, const char *name)
{
    PVFS_credentials credentials; gen_credentials(&credentials, req);
    PVFS_object_ref ref; ref.handle = inode2handle(&credentials, parent); ref.fs_id = pvfs_fsid;

    int ret = lookup(&credentials, &ref, (char*)name);
    if (ret < 0)
        fuse_reply_err(req, ENOENT);

    struct fuse_entry_param e;
    generate_fuse_entry(&credentials, &e, &ref);

    fuse_reply_entry(req, &e);
}

/**
 * Create and open a file
 *
 * Open flags (with the exception of O_NOCTTY) are available in
 * fi->flags.
 *
 * Valid replies:
 *   fuse_reply_create
 *   fuse_reply_err
 *
 * @param req request handle
 * @param parent inode number of the parent directory
 * @param name to create
 * @param mode file type and mode with which to create the new file
 * @param fi file information
 */
void skye_ll_create(fuse_req_t req, fuse_ino_t parent, const char *filename, 
                     mode_t mode, struct fuse_file_info *fi)
{
    PVFS_credentials credentials; gen_credentials(&credentials, req);
    PVFS_object_ref ref; ref.handle = inode2handle(&credentials, parent); ref.fs_id = pvfs_fsid;
    
    int server_id;
    struct skye_directory *dir = cache_fetch(&ref);
    if (!dir){
        fuse_reply_err(req,EIO);
    }
    
bitmap: 
    server_id = get_server_for_file(dir, filename);
    CLIENT *rpc_client = get_connection(server_id);
    skye_lookup result;

    enum clnt_stat retval = skye_rpc_create_1(credentials, ref, (char*)filename, mode, &result, rpc_client);
    if (retval != RPC_SUCCESS) {
		clnt_perror (rpc_client, "RPC create failed");
        fuse_reply_err(req,EIO);
        goto exit;
	}

    if (result.errnum == -EAGAIN){
        update_client_mapping(dir, &result.skye_lookup_u.bitmap);
        goto bitmap;
    } else if (result.errnum < 0){
        fuse_reply_err(req,-1 * result.errnum);
        goto exit;
    }

    fi->fh = result.skye_lookup_u.ref.handle;

    struct fuse_entry_param e;
    generate_fuse_entry(&credentials, &e, &result.skye_lookup_u.ref);

    fuse_reply_create(req, &e, fi);

exit:
    cache_return(dir);
}

/**
 * Create a directory
 *
 * Valid replies:
 *   fuse_reply_entry
 *   fuse_reply_err
 *
 * @param req request handle
 * @param parent inode number of the parent directory
 * @param name to create
 * @param mode with which to create the new file
 */
void skye_ll_mkdir(fuse_req_t req, fuse_ino_t parent, const char *filename,
               mode_t mode)
{
    PVFS_credentials credentials; gen_credentials(&credentials, req);
    PVFS_object_ref ref; ref.handle = inode2handle(&credentials, parent); ref.fs_id = pvfs_fsid;

    int server_id;
    struct skye_directory *dir = cache_fetch(&ref);
    if (!dir){
        fuse_reply_err(req,EIO);
    }
    
bitmap: 
    server_id = get_server_for_file(dir, filename);
    CLIENT *rpc_client = get_connection(server_id);
    skye_lookup result;

    enum clnt_stat retval = skye_rpc_mkdir_1(credentials, ref, (char*)filename, mode, &result, rpc_client);
    if (retval != RPC_SUCCESS) {
		clnt_perror (rpc_client, "RPC mkdir failed");
        fuse_reply_err(req,EIO);
        goto exit;
	}

    if (result.errnum == -EAGAIN){
        update_client_mapping(dir, &result.skye_lookup_u.bitmap);
        goto bitmap;
    } else if (result.errnum < 0){
        fuse_reply_err(req,-1 * result.errnum);
        goto exit;
    }

    struct fuse_entry_param e;
    generate_fuse_entry(&credentials, &e, &result.skye_lookup_u.ref);

    fuse_reply_entry(req, &e);

exit:
    cache_return(dir);
}

#if 0
int skye_rename(const char *src_path, const char *dst_path)
{
    PVFS_credentials credentials; gen_credentials(&credentials);

    PVFS_object_ref src_ref;
    char src_name[MAX_FILENAME_LEN] = {0};
    char src_dir[MAX_PATHNAME_LEN] = {0};
    int ret, server_id, retry_count = 3;

    if ((ret = get_path_components(src_path, src_name, src_dir)) < 0)
        return ret;

    PVFS_object_ref dst_ref;
    char dst_name[MAX_FILENAME_LEN] = {0};
    char dst_dir[MAX_PATHNAME_LEN] = {0};
    if ((ret = get_path_components(dst_path, dst_name, dst_dir)) < 0)
        return ret;

retry:

    if ((ret = resolve(&credentials, src_dir, &src_ref)) < 0)
        return ret;

    if ((ret = partition(&credentials, &src_ref, src_name)) < 0)
        return ret;

    if ((ret = resolve(&credentials, dst_dir, &dst_ref)) < 0)
        return ret;
    
    struct skye_directory *dir = cache_fetch(&dst_ref);
    if (!dir){
        return -EIO;
    }
    
bitmap: 
    server_id = get_server_for_file(dir, dst_name);

    enum clnt_stat retval;
    skye_result result;
    
    server_id = get_server_for_file(dir, dst_name);
    CLIENT *rpc_client = get_connection(server_id);

    retval = skye_rpc_rename_1(credentials, src_name, src_ref, dst_name, dst_ref, &result, rpc_client);
    if (retval != RPC_SUCCESS) {
		clnt_perror (rpc_client, "RPC create failed");
        ret =  -EIO;
        goto exit;
	}

    ret = result.errnum;
    if (ret == -EAGAIN){
        update_client_mapping(dir, &result.skye_result_u.bitmap);
        goto bitmap;
    } 
    if (ret == -ENOENT){
        if (retry_count){
            err_msg("rename() encountered an ENOENT, retrying (%d more times).\n", retry_count);
            retry_count--;
            goto retry;
        }
        err_msg("rename() encountered an ENOENT too many times, giving up..\n", retry_count);
    }

exit:
    return ret;
}

/* FIXME: in a lot of ways! */
int skye_remove(const char *path)
{
    PVFS_object_ref ref;
    PVFS_credentials credentials; gen_credentials(&credentials);
    char parent[MAX_PATHNAME_LEN], filename[MAX_FILENAME_LEN];
    int ret = 0, server_id;


    if ((ret = get_path_components(path, filename, parent)) < 0)
        return ret;

    if ((ret = resolve(&credentials, parent, &ref)) < 0)
        return ret;

    skye_result result;
    enum clnt_stat retval;

    struct skye_directory *dir = cache_fetch(&ref);
    if (!dir){
        return -EIO;
    }
    
bitmap:
    server_id = get_server_for_file(dir, filename);
    CLIENT *rpc_client = get_connection(server_id);

    retval = skye_rpc_remove_1(credentials, ref, filename, &result, rpc_client);
	if (retval != RPC_SUCCESS) {
		clnt_perror (rpc_client, "RPC remove failed");
        ret = -EIO;
        goto exit;
	}

    if (result.errnum == -EAGAIN){
        update_client_mapping(dir, &result.skye_result_u.bitmap);
        goto bitmap;
    } else if (result.errnum < 0){
        ret = result.errnum;
        goto exit;
    } else {
        ret = result.errnum;
    }

exit:
    cache_return(dir);
    return ret;
}

int skye_unlink(const char *path)
{
    return skye_remove(path);
}

int skye_rmdir(const char *path)
{
    return skye_remove(path);
}

int skye_chmod(const char *path, mode_t mode)
{
    PVFS_object_ref ref;
    PVFS_sys_attr new_attr;
    PVFS_credentials credentials; gen_credentials(&credentials);
    int ret;

    if ((ret = resolve(&credentials, path, &ref)) < 0)
        return ret;

    // FIXME: should be PVFS_util_translate_mode()?
    new_attr.perms = mode & PVFS_PERM_VALID;
    new_attr.mask = PVFS_ATTR_SYS_PERM;

    ret = PVFS_sys_setattr(ref, new_attr, &credentials, PVFS_HINT_NULL);
    if (ret < 0)
        return pvfs2errno(ret);

    return 0;
}

int skye_chown(const char *path, uid_t uid, gid_t gid)
{
    PVFS_object_ref ref;
    PVFS_sys_attr new_attr;
    PVFS_credentials credentials; gen_credentials(&credentials);
    int ret;

    if ((ret = resolve(&credentials, path, &ref)) < 0)
        return ret;

    new_attr.owner = uid;
    new_attr.group = gid;
    new_attr.mask = PVFS_ATTR_SYS_UID | PVFS_ATTR_SYS_GID;

    ret = PVFS_sys_setattr(ref, new_attr, &credentials, PVFS_HINT_NULL);
    if (ret < 0)
        return pvfs2errno(ret);

    return 0;
}

int skye_truncate(const char *path, off_t size)
{
    PVFS_object_ref ref;
    PVFS_credentials credentials; gen_credentials(&credentials);
    int ret;

    if ((ret = resolve(&credentials, path, &ref)) < 0)
        return ret;

    ret = PVFS_sys_truncate(ref, size, &credentials, PVFS_HINT_NULL);
    if (ret < 0)
        return pvfs2errno(ret);

    return 0;
}

int skye_utime(const char *path, struct utimbuf *timbuf)
{
    PVFS_object_ref ref;
    PVFS_sys_attr new_attr;
    PVFS_credentials credentials; gen_credentials(&credentials);
    int ret;

    if ((ret = resolve(&credentials, path, &ref)) < 0)
        return ret;

    new_attr.atime = (PVFS_time)timbuf->actime;
    new_attr.mtime = (PVFS_time)timbuf->modtime;
    new_attr.mask = PVFS_ATTR_SYS_ATIME | PVFS_ATTR_SYS_MTIME;

    ret = PVFS_sys_setattr(ref, new_attr, &credentials, PVFS_HINT_NULL);
    if (ret < 0)
        return pvfs2errno(ret);

    return 0;
}
#endif

/* FIXME: we should verify that the file exists and such here */
void skye_ll_open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
    PVFS_credentials credentials; gen_credentials(&credentials, req);
    PVFS_object_ref ref; ref.handle = inode2handle(&credentials, ino); ref.fs_id = pvfs_fsid;
    fi->fh = ref.handle;
    fuse_reply_open(req, fi);
}

void skye_ll_read(fuse_req_t req, fuse_ino_t ino, size_t size,
			  off_t offset, struct fuse_file_info *fi)
{
    PVFS_credentials credentials; gen_credentials(&credentials, req);
    PVFS_object_ref ref; ref.handle = inode2handle(&credentials, ino); ref.fs_id = pvfs_fsid;
    PVFS_Request mem_req, file_req;
    PVFS_sysresp_io resp_io;
    int ret;
    char buf[size + 1];
    (void)fi;

    file_req = PVFS_BYTE;
    ret = PVFS_Request_contiguous(size, PVFS_BYTE, &mem_req);
    if (ret < 0)
        fuse_reply_err(req, pvfs2errno(ret));

    ret = PVFS_sys_read(ref, file_req, offset, buf, mem_req, &credentials,
                        &resp_io, PVFS_HINT_NULL);
    if (ret < 0)
        fuse_reply_err(req, pvfs2errno(ret));

    PVFS_Request_free(&mem_req);

    fuse_reply_buf(req, buf, resp_io.total_completed);
}

#if 0
int skye_write(const char* path, const char *buf, size_t size, off_t offset, 
              struct fuse_file_info *fi)
{
    (void)path;
    assert(fi);
    assert(fi->fh);

    PVFS_object_ref *ref = (PVFS_object_ref*)fi->fh;
    PVFS_credentials credentials; gen_credentials(&credentials);
    PVFS_Request mem_req, file_req;
    PVFS_sysresp_io resp_io;
    int ret;

    file_req = PVFS_BYTE;
    ret = PVFS_Request_contiguous(size, PVFS_BYTE, &mem_req);
    if (ret < 0)
        return pvfs2errno(ret);

    ret = PVFS_sys_write(*ref, file_req, offset, (void*)buf, mem_req,
                         &credentials, &resp_io, PVFS_HINT_NULL);
    if (ret < 0)
        return pvfs2errno(ret);

    PVFS_Request_free(&mem_req);
    return resp_io.total_completed;
}
#endif
