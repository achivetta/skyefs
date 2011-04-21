#include "common/defaults.h"
#include "common/trace.h"
#include "common/skye_rpc.h"
#include "common/options.h"
#include "common/connection.h"
#include "server.h"
#include "cache.h"

#include <assert.h>
#include <stdbool.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#include <pvfs2-util.h>
#include <pvfs2-sysint.h>
#include <pvfs2-mgmt.h>

static int enter_bucket(PVFS_credentials *creds, PVFS_object_ref *handle, const char *name, skye_bitmap *bitmap)
{
    struct skye_directory *dir = cache_fetch(handle);
    if (!dir)
        return -EIO;

    /* don't do locality check if not provided bitmap to update */
    if (bitmap){
        int server = giga_get_server_for_file(&(dir->mapping), name);

        if (server != skye_options.servernum){
            memcpy(bitmap, &dir->mapping, sizeof(dir->mapping));
            return -EAGAIN;
        }
    }

    int index = giga_get_index_for_file(&(dir->mapping), name);

    cache_return(dir);

    char physical_path[MAX_LEN];
    snprintf(physical_path, MAX_LEN, "p%05d", index);

    PVFS_sysresp_lookup lk_response;
    int ret;

    memset(&lk_response, 0, sizeof(lk_response));
    ret = PVFS_sys_ref_lookup(pvfs_fsid, physical_path, *handle,
                              creds, &lk_response, PVFS2_LOOKUP_LINK_NO_FOLLOW,
                              PVFS_HINT_NULL);
    if ( ret < 0 )
        return -1 * PVFS_get_errno_mapping(ret);

    *handle = lk_response.ref;

    return 0;
}

static int isdir(PVFS_credentials *creds, PVFS_object_ref *handle)
{
    PVFS_sysresp_getattr getattr_response;
    PVFS_sys_attr*	attrs;

    int			ret;

    memset(&getattr_response,0, sizeof(PVFS_sysresp_getattr));

    ret = PVFS_sys_getattr(*handle, PVFS_ATTR_SYS_ALL_NOHINT, creds,
                           &getattr_response, PVFS_HINT_NULL);
    if ( ret < 0 )
        return 0;

    attrs = &getattr_response.attr;

    if (attrs->objtype == PVFS_TYPE_DIRECTORY)
        return 1;
    return 0;
} 

static int isdir_overflow(PVFS_credentials *creds, PVFS_object_ref *handle)
{
    int	ret;

    PVFS_sysresp_getattr getattr_response;
    PVFS_sys_attr*	attrs;
    memset(&getattr_response,0, sizeof(PVFS_sysresp_getattr));

    ret = PVFS_sys_getattr(*handle, PVFS_ATTR_SYS_ALL_NOHINT, creds,
                           &getattr_response, PVFS_HINT_NULL);
    if (ret < 0 )
        return -1;

    attrs = &getattr_response.attr;

    //XXX: fix the size for now
    if (attrs->dirent_count > SPLIT_THRESHOLD)
        return 1;
    return 0;
} 

bool_t skye_rpc_init_1_svc(bool_t *result, struct svc_req *rqstp)
{
    (void)rqstp;
    assert(result);

    dbg_msg(log_fp, "[%s] recv:init()", __func__);

    *result = true;

    return true;
}

bool_t skye_rpc_lookup_1_svc(PVFS_credentials creds, PVFS_object_ref parent,
                             skye_pathname path, skye_lookup *result, 
                             struct svc_req *rqstp)
{
    (void)rqstp;

    struct skye_directory *dir = cache_fetch(&parent);
    if (!dir){
        result->errnum = -EIO;
        return true;
    }

    int server = giga_get_server_for_file(&dir->mapping, (const char*)path);

    if (server != skye_options.servernum){
        result->errnum = -EAGAIN;
        memcpy(&(result->skye_lookup_u.bitmap), &dir->mapping, sizeof(dir->mapping));
        return true;
    }

    int index = giga_get_index_for_file(&dir->mapping, (const char*)path);

    cache_return(dir);

    char physical_path[MAX_LEN];
    snprintf(physical_path, MAX_LEN, "p%05d/%s", index, (const char*)path);

    PVFS_sysresp_lookup lk_response;
    int ret;

    memset(&lk_response, 0, sizeof(lk_response));
    ret = PVFS_sys_ref_lookup(pvfs_fsid, physical_path, parent,
                              &creds, &lk_response, PVFS2_LOOKUP_LINK_NO_FOLLOW,
                              PVFS_HINT_NULL);
    if ( ret < 0 ) {
        result->errnum = -1 * PVFS_get_errno_mapping(ret);
        return true;
    }

    result->skye_lookup_u.ref = lk_response.ref;
    result->errnum = 0;

    return true;;
}

bool_t skye_rpc_create_1_svc(PVFS_credentials creds, PVFS_object_ref parent,
                             skye_pathname filename, mode_t mode, 
                             skye_lookup *result, struct svc_req *rqstp)
{
    (void)rqstp;
    int rc;

    if ((rc = enter_bucket(&creds, &parent, (char*)filename, &(result->skye_lookup_u.bitmap))) < 0){
        result->errnum = rc;
        return true;
    }

    PVFS_sysresp_create resp_create;

    /* Set attributes */
    PVFS_sys_attr attr;
    memset(&attr, 0, sizeof(PVFS_sys_attr));
    attr.owner = creds.uid;
    attr.group = creds.gid;
    attr.perms = mode;
    attr.atime = time(NULL);
    attr.mtime = attr.atime;
    attr.mask = PVFS_ATTR_SYS_ALL_SETABLE;
    attr.dfile_count = 0;

    rc = PVFS_sys_create(filename, parent, attr, &creds, NULL, &resp_create,
                             PVFS_SYS_LAYOUT_DEFAULT, PVFS_HINT_NULL);
    if (rc != 0){
        if ( rc == -PVFS_ENOENT )
            result->errnum = -EACCES;
        else
            result->errnum = -1 * PVFS_get_errno_mapping(rc);
        return true;
    }

    //TODO: splitting logic
    //
    //(1) check the number of entries in the directory
    //
    //(2) if partition doesn't overflow, exit ... else
    //--- split begins: take appropriate flags
    //(3) create a new partition (use giga to find the new index)
    //    --- when you create the new partition check if it created on the
    //        correct server; if not, delete and re-create repeatedly
    //(4) readdir() old partition, rename the files that will move
    //(5) after all entries have been moved, send RPC to server
    //--- reset all split flags after successful response from servers
   
    if (isdir_overflow(&creds, &parent) == 1) {
        // the partition (directory) is overflowing, let's split
    }

    result->errnum = 0;
    result->skye_lookup_u.ref = resp_create.ref;

    return true;
}

static int pvfs_mkdir_server(PVFS_credentials *creds, PVFS_object_ref *parent, 
                        char *dirname, PVFS_sys_attr *attr, int server)
{
    (void)server;
    PVFS_sysresp_mkdir resp_mkdir;

    int created_on = -1;
    PVFS_error rc = 0;
    
    while (1){
        rc = PVFS_sys_mkdir(dirname, *parent, *attr, creds, &resp_mkdir,
                            PVFS_HINT_NULL);
        if (rc != 0)
            return -1 * PVFS_get_errno_mapping(rc);

        created_on = pvfs_get_mds(&resp_mkdir.ref);

        if (created_on == server)
            break;

        rc = PVFS_sys_remove(dirname, *parent, creds, PVFS_HINT_NULL);
        if (rc != 0)
            return -1 * PVFS_get_errno_mapping(rc);
    }
    return 0;
}

bool_t skye_rpc_mkdir_1_svc(PVFS_credentials creds, PVFS_object_ref parent,
                            skye_pathname dirname, mode_t mode, 
                            skye_result *result, struct svc_req *rqstp)
{
    (void)rqstp;
    int rc;

    if ((rc = enter_bucket(&creds, &parent, (char*)dirname, &(result->skye_result_u.bitmap))) < 0){
        result->errnum = rc;
        return true;
    }


    /* Set attributes */
    PVFS_sys_attr attr;
    memset(&attr, 0, sizeof(PVFS_sys_attr));
    attr.owner = creds.uid;
    attr.group = creds.gid;
    attr.perms = mode;
    attr.mask = PVFS_ATTR_SYS_ALL_SETABLE;

    PVFS_sysresp_mkdir resp_mkdir;
    rc = PVFS_sys_mkdir(dirname, parent, attr, &creds, &resp_mkdir,
                            PVFS_HINT_NULL);

    if (rc != 0){
        result->errnum = -1 * PVFS_get_errno_mapping(rc);
        return true;
    }

    /* FIXME: handle cleanup in the case that this fails */

    parent = resp_mkdir.ref;

    int server = pvfs_get_mds(&parent);

    dirname = "p00000";
    rc = pvfs_mkdir_server(&creds, &parent, dirname, &attr, server);
    result->errnum = rc;

	return true;
}

/* except the 0th bucket */
static int remove_all_buckets(PVFS_credentials *creds, struct skye_directory *dir)
{
    PVFS_sysresp_readdir rd_response;
    unsigned int pvfs_dirent_incount = 32; // reasonable chank size
    PVFS_ds_position token = 0;

    PVFS_object_ref *parent = &dir->handle;

    do {
        PVFS_dirent *cur_file = NULL;
        unsigned int i;

        memset(&rd_response, 0, sizeof(PVFS_sysresp_readdir));
        int ret = PVFS_sys_readdir(*parent, (!token ? PVFS_READDIR_START : token),
                                pvfs_dirent_incount, creds, &rd_response,
                                PVFS_HINT_NULL);
        if (ret < 0)
            return -1 * PVFS_get_errno_mapping(ret);

        // FIXME: just abort if any partition is splitting
        for (i = 0; i < rd_response.pvfs_dirent_outcount; i++) {
            cur_file = &(rd_response.dirent_array[i]);

            index_t index;
            sscanf(cur_file->d_name, "p%u", &index);

            /* we don't remove the 0th bucket until the end */
            if (index == 0)
                continue;

            CLIENT *client = get_connection(giga_get_server_for_index(&dir->mapping, index));

            enum clnt_stat retval;
            retval = skye_rpc_bucket_remove_1(*creds, *parent, index, &ret, client);
            if (retval != RPC_SUCCESS){
                clnt_perror(client, "RPC bucket remove failed");
                return -EIO;
            }

            if (ret != 0)
                return ret;
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

    return 0;
}

bool_t skye_rpc_remove_1_svc(PVFS_credentials creds, PVFS_object_ref parent,
                            skye_pathname filename, skye_result *result, 
                            struct svc_req *rqstp)
{
    (void)rqstp;
    int rc;

    if ((rc = enter_bucket(&creds, &parent, (char*)filename, &(result->skye_result_u.bitmap))) < 0){
        result->errnum = rc;
        return true;
    }

    PVFS_sysresp_lookup lk_response;

    memset(&lk_response, 0, sizeof(lk_response));
    rc = PVFS_sys_ref_lookup(pvfs_fsid, filename, parent, &creds,
                              &lk_response, PVFS2_LOOKUP_LINK_NO_FOLLOW,
                              PVFS_HINT_NULL);
    if ( rc < 0 ){
        result->errnum =  -1 * PVFS_get_errno_mapping(rc);
        return true;
    }

    if (isdir(&creds, &lk_response.ref)){
        struct skye_directory *dir = cache_fetch(&lk_response.ref);
        rc = remove_all_buckets(&creds, dir);
        if ( rc < 0 ){
            result->errnum = rc;
            return true;
        }

        rc = PVFS_sys_remove("p00000", dir->handle, &creds, PVFS_HINT_NULL);
        if ( rc < 0 ){
            result->errnum = -1 * PVFS_get_errno_mapping(rc);
            return true;
        }

        /* FIXME: lock out access to the directory here */
        rc = PVFS_sys_remove(filename, parent, &creds, PVFS_HINT_NULL);
        if ( rc < 0 ){
            result->errnum = -1 * PVFS_get_errno_mapping(rc);
            return true;
        }

        cache_destroy(dir);
        dir = NULL;

        return true;
    }

    rc = PVFS_sys_remove(filename, parent, &creds, PVFS_HINT_NULL);

    if (rc != 0)
        result->errnum = -1 * PVFS_get_errno_mapping(rc);
    else
        result->errnum = 0;

    
	return true;
}

bool_t skye_rpc_rename_1_svc(PVFS_credentials creds, 
                             skye_pathname src_name, PVFS_object_ref src_parent,
                             skye_pathname dst_name, PVFS_object_ref dst_parent,
                             skye_result *result,  struct svc_req *rqstp)
{
    (void)rqstp;
    int rc;

    if ((rc = enter_bucket(&creds, &src_parent, (char*)src_name, NULL)) < 0){
        result->errnum = rc;
        return true;
    }

    if ((rc = enter_bucket(&creds, &dst_parent, (char*)dst_name, &(result->skye_result_u.bitmap))) < 0){
        result->errnum = rc;
        return true;
    }

    rc = PVFS_sys_rename(src_name, src_parent, dst_name, dst_parent,
                             &creds, PVFS_HINT_NULL);
    if (rc != 0)
        result->errnum = -1 * PVFS_get_errno_mapping(rc);
    else
        result->errnum = 0;

	return true;
}

bool_t skye_rpc_bucket_add_1_svc(PVFS_object_ref handle, int index, int *result,
                                 struct svc_req *rqstp) 
{
    bool_t retval = true;
    (void)result;
    (void)rqstp;

    struct skye_directory *dir = cache_fetch(&handle);

    giga_update_mapping(&dir->mapping, index);

    cache_return(dir);

    *result = 0;

    return retval;
}

bool_t skye_rpc_bucket_remove_1_svc(PVFS_credentials creds, 
                                    PVFS_object_ref handle, 
                                    int index, int *result, 
                                    struct svc_req *rqstp)
{
    bool_t retval = true;
    (void)rqstp;


    struct skye_directory *dir = cache_fetch(&handle);

    if (!dir) {
        *result = -ENOMEM;
        return true;
    }

    char physical_path[MAX_LEN];
    snprintf(physical_path, MAX_LEN, "p%05d", index);

    int rc = PVFS_sys_remove(physical_path, handle, &creds, PVFS_HINT_NULL);
    if ( rc < 0 ){
        *result = -1 * PVFS_get_errno_mapping(rc);
        return true;
    }

    giga_update_mapping_remove(&dir->mapping, index);

    cache_return(dir);

    *result = 0;

    return retval;
}

/* TODO: What exactly am I supposed to do here? */
int skye_rpc_prog_1_freeresult (SVCXPRT *transp, xdrproc_t xdr_result, 
                                caddr_t result)
{
    (void)transp;

    xdr_free (xdr_result, result);

    /*
     * Insert additional freeing code here, if needed
     */

    return 1;
}
