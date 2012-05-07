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

#define FUSE_CACHE_TIME_S 10

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

struct direntbuf {
    char *buf;
    size_t size;
};

void skye_ll_opendir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi)
{
    struct direntbuf *db = calloc(1,sizeof(struct direntbuf));
    if (!db){
        fuse_reply_err(req, EIO);
        return;
    }
    fi->fh = (uintptr_t)db;

    PVFS_credentials credentials; gen_credentials(&credentials, req);
    PVFS_object_ref ref; ref.handle = inode2handle(&credentials, ino); ref.fs_id = pvfs_fsid;

    int add_to_buf(void *ctx, PVFS_dirent dirent){
        (void)ctx;
        /* FIXME double instead of realloc each time */
        struct stat stbuf; memset(&stbuf, 0, sizeof(stbuf));
        stbuf.st_ino = dirent.handle;

        size_t entrysize = fuse_add_direntry(req, NULL, 0, dirent.d_name, NULL, 0);

        char *newp = realloc(db->buf, db->size + entrysize);
        if (!newp) {
            return -1;
        } else { 
            db->buf = newp;
        }

        fuse_add_direntry(req, db->buf + db->size, entrysize, dirent.d_name, &stbuf,
                          db->size + entrysize);

        db->size = db->size + entrysize;

        return 0;
    }

    int recurse(void *ctx, PVFS_dirent dirent){
        /* TODO could we use this anywhere else? */
        if (dirent.d_name[0] != 'p')
            return 0; // Continue

        int (*callback)(void *, PVFS_dirent) = ctx;
        PVFS_object_ref ref; ref.fs_id = pvfs_fsid; ref.handle = dirent.handle;
        return pvfs_readdir(NULL, &credentials, &ref, callback);
    }

    pvfs_readdir(add_to_buf, &credentials, &ref, recurse);

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
    fuse_reply_err(req, 0);
}

void skye_ll_readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t offset,
                     struct fuse_file_info *fi)
{
    (void)ino;
    struct direntbuf *db = (void*)fi->fh;

    if (offset >= (signed long)db->size){
        fuse_reply_buf(req, NULL, 0);
        return;
    }

    fuse_reply_buf(req, db->buf + offset, min(db->size - offset, size));
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
        fuse_reply_err(req, ret);
    else
        fuse_reply_attr(req, &stbuf, 10);
}

int generate_fuse_entry(PVFS_credentials *credentials, struct fuse_entry_param *e, PVFS_object_ref *ref)
{
    memset(e, 0, sizeof(struct fuse_entry_param));
    e->ino = ref->handle;
    e->attr_timeout = FUSE_CACHE_TIME_S;
    e->entry_timeout = FUSE_CACHE_TIME_S;
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
    if (ret < 0){
        fuse_reply_err(req, ENOENT);
        return;
    }

    struct fuse_entry_param e;
    generate_fuse_entry(&credentials, &e, &ref);

    fuse_reply_entry(req, &e);
}

/**
 * Create file node
 *
 * Create a regular file, character device, block device, fifo or
 * socket node.
 *
 * Valid replies:
 *   fuse_reply_entry
 *   fuse_reply_err
 *
 * @param req request handle
 * @param parent inode number of the parent directory
 * @param name to create
 * @param mode file type and mode with which to create the new file
 * @param rdev the device number (only valid if created file is a device)
 */
void skye_ll_mknod (fuse_req_t req, fuse_ino_t parent, const char *filename, 
                    mode_t mode, dev_t rdev)
{
    (void)rdev;

    if (! (mode & S_IFREG) ){
        fuse_reply_err(req,ENOSYS);
        return;
    }

    PVFS_credentials credentials; gen_credentials(&credentials, req);
    PVFS_object_ref ref; ref.handle = inode2handle(&credentials, parent); ref.fs_id = pvfs_fsid;
    
    int server_id;
    struct skye_directory *dir = cache_fetch(&ref);
    if (!dir){
        fuse_reply_err(req,EIO);
        return;
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

    struct fuse_entry_param e;
    generate_fuse_entry(&credentials, &e, &result.skye_lookup_u.ref);

    fuse_reply_entry(req, &e);

exit:
    cache_return(dir);
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
        return;
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
        return;;
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

void skye_ll_rename (fuse_req_t req, fuse_ino_t src_parent, const char *src_name,
                     fuse_ino_t dst_parent, const char *dst_name)
{
    PVFS_credentials credentials; gen_credentials(&credentials, req);
    PVFS_object_ref src_ref; src_ref.fs_id = pvfs_fsid;
    PVFS_object_ref dst_ref; dst_ref.fs_id = pvfs_fsid;

    int ret, server_id, retry_count = 3;

retry:
    src_ref.handle = inode2handle(&credentials, src_parent); 
    dst_ref.handle = inode2handle(&credentials, dst_parent); 

    if ((ret = partition(&credentials, &src_ref, (char*)src_name)) < 0){
        fuse_reply_err(req, ret);
        return;
    }

    struct skye_directory *dir = cache_fetch(&dst_ref);
    if (!dir){
        fuse_reply_err(req, EIO);
        return;
    }
    
bitmap: 
    server_id = get_server_for_file(dir, dst_name);

    enum clnt_stat retval;
    skye_result result;
    
    server_id = get_server_for_file(dir, dst_name);
    CLIENT *rpc_client = get_connection(server_id);

    retval = skye_rpc_rename_1(credentials, (char*)src_name, src_ref, (char*)dst_name, dst_ref, &result, rpc_client);
    if (retval != RPC_SUCCESS) {
		clnt_perror (rpc_client, "RPC create failed");
        fuse_reply_err(req, EIO);
	}

    ret = result.errnum;
    if (ret == -EAGAIN){
        update_client_mapping(dir, &result.skye_result_u.bitmap);
        goto bitmap;
    } else if (ret == -ENOENT){
        if (retry_count){
            err_msg("rename() encountered an ENOENT, retrying (%d more times).\n", retry_count);
            retry_count--;
            goto retry;
        }
        err_msg("rename() encountered an ENOENT too many times, giving up..\n", retry_count);
        fuse_reply_err(req, EIO);
    } else {
        fuse_reply_err(req, 0);
    }
}

/* FIXME: in a lot of ways! */
static int skye_remove(fuse_req_t req, fuse_ino_t parent, const char *filename)
{
    PVFS_credentials credentials; gen_credentials(&credentials, req);
    PVFS_object_ref ref; ref.handle = inode2handle(&credentials, parent); ref.fs_id = pvfs_fsid;
    int ret = 0, server_id;

    skye_result result;
    enum clnt_stat retval;

    struct skye_directory *dir = cache_fetch(&ref);
    if (!dir){
        return EIO;
    }
    
bitmap:
    server_id = get_server_for_file(dir, filename);
    CLIENT *rpc_client = get_connection(server_id);

    retval = skye_rpc_remove_1(credentials, ref, (void*)filename, &result, rpc_client);
	if (retval != RPC_SUCCESS) {
		clnt_perror (rpc_client, "RPC remove failed");
        ret = EIO;
        goto exit;
	}

    if (result.errnum == -EAGAIN){
        update_client_mapping(dir, &result.skye_result_u.bitmap);
        goto bitmap;
    } else if (result.errnum < 0){
        ret = -1 * result.errnum;
    } else {
        ret = 0;
    }

exit:
    cache_return(dir);
    return ret;
}

void skye_ll_unlink(fuse_req_t req, fuse_ino_t parent, const char *name)
{
    fuse_reply_err(req,skye_remove(req,parent,name));
}

void skye_ll_rmdir(fuse_req_t req, fuse_ino_t parent, const char *name)
{
    fuse_reply_err(req,skye_remove(req,parent,name));
}

/**
 * Set file attributes
 *
 * In the 'attr' argument only members indicated by the 'to_set'
 * bitmask contain valid values.  Other members contain undefined
 * values.
 *
 * Valid replies:
 *   fuse_reply_attr
 *   fuse_reply_err
 *
 * @param req request handle
 * @param ino the inode number
 * @param attr the attributes
 * @param to_set bit mask of attributes which should be set
 * @param fi file information, or NULL
 */
void skye_ll_setattr(fuse_req_t req, fuse_ino_t ino, struct stat *attr,
                     int to_set, struct fuse_file_info *fi)
{
    (void)fi;
    PVFS_credentials credentials; gen_credentials(&credentials, req);
    PVFS_object_ref ref; ref.handle = inode2handle(&credentials, ino); ref.fs_id = pvfs_fsid;

    PVFS_sys_attr new_attr; memset(&new_attr, 0, sizeof(new_attr));
    if (to_set & FUSE_SET_ATTR_MODE){
        new_attr.perms = attr->st_mode & PVFS_PERM_VALID;
        new_attr.mask |= PVFS_ATTR_SYS_PERM;
    }
    if (to_set & FUSE_SET_ATTR_UID ){
        new_attr.owner = attr->st_uid;
        new_attr.mask |= PVFS_ATTR_SYS_UID;
    }
    if (to_set & FUSE_SET_ATTR_GID ){
        new_attr.group = attr->st_gid;
        new_attr.mask |= PVFS_ATTR_SYS_GID;
    }
    if (to_set & FUSE_SET_ATTR_ATIME){
        new_attr.atime = attr->st_atime;
        new_attr.mask |= PVFS_ATTR_SYS_ATIME;
    }
    if (to_set & FUSE_SET_ATTR_MTIME){
        new_attr.mtime = attr->st_mtime;
        new_attr.mask |= PVFS_ATTR_SYS_MTIME;
    }
    if (to_set & FUSE_SET_ATTR_ATIME_NOW){
        new_attr.atime = time(NULL);
        new_attr.mask |= PVFS_ATTR_SYS_ATIME;
    }
    if (to_set & FUSE_SET_ATTR_MTIME_NOW){
        new_attr.mtime = time(NULL);
        new_attr.mask |= PVFS_ATTR_SYS_MTIME;
    }

    int ret = PVFS_sys_setattr(ref, new_attr, &credentials, PVFS_HINT_NULL);
    if (ret < 0){
        fuse_reply_err(req,pvfs2errno(ret));
        return;
    }

    /* truncate is its own beast */
    if (to_set & FUSE_SET_ATTR_SIZE){
        ret = PVFS_sys_truncate(ref, attr->st_size, &credentials, PVFS_HINT_NULL);
        if (ret < 0){
            fuse_reply_err(req,pvfs2errno(ret));
            return;
        }
    }

    struct stat stbuf; memset(&stbuf, 0, sizeof(stbuf));
    if ((ret = pvfs_getattr(&credentials, &ref, &stbuf)) < 0)
        fuse_reply_err(req, ret);
    else
        fuse_reply_attr(req, &stbuf, FUSE_CACHE_TIME_S);
}

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
    if (ret < 0){
        fuse_reply_err(req, pvfs2errno(ret));
        return;
    }

    ret = PVFS_sys_read(ref, file_req, offset, buf, mem_req, &credentials,
                        &resp_io, PVFS_HINT_NULL);
    if (ret < 0){
        fuse_reply_err(req, pvfs2errno(ret));
        return;
    }

    PVFS_Request_free(&mem_req);

    fuse_reply_buf(req, buf, resp_io.total_completed);
}

/**
 * Write data
 *
 * Write should return exactly the number of bytes requested
 * except on error.  An exception to this is when the file has
 * been opened in 'direct_io' mode, in which case the return value
 * of the write system call will reflect the return value of this
 * operation.
 *
 * fi->fh will contain the value set by the open method, or will
 * be undefined if the open method didn't set any value.
 *
 * Valid replies:
 *   fuse_reply_write
 *   fuse_reply_err
 *
 * @param req request handle
 * @param ino the inode number
 * @param buf data to write
 * @param size number of bytes to write
 * @param off offset to write to
 * @param fi file information
 */
void skye_ll_write (fuse_req_t req, fuse_ino_t ino, const char *buf,
                    size_t size, off_t offset, struct fuse_file_info *fi)
{
    (void)fi;
    PVFS_credentials credentials; gen_credentials(&credentials, req);
    PVFS_object_ref ref; ref.handle = inode2handle(&credentials, ino); ref.fs_id = pvfs_fsid;
    PVFS_Request mem_req, file_req;
    PVFS_sysresp_io resp_io;
    int ret;

    file_req = PVFS_BYTE;
    ret = PVFS_Request_contiguous(size, PVFS_BYTE, &mem_req);
    if (ret < 0){
        fuse_reply_err(req, pvfs2errno(ret));
        return;
    }

    ret = PVFS_sys_write(ref, file_req, offset, (void*)buf, mem_req, &credentials,
                         &resp_io, PVFS_HINT_NULL);
    if (ret < 0){
        fuse_reply_err(req, pvfs2errno(ret));
        return;
    }

    PVFS_Request_free(&mem_req);
    fuse_reply_write(req,resp_io.total_completed);
}
