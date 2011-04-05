#include "common/trace.h"
#include "common/skye_rpc.h"
#include "common/defaults.h"
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

    int index = giga_get_index_for_file(&dir->mapping, (const char*)path);

    cache_return(dir);

    char physical_path[MAX_LEN];
    snprintf(physical_path, MAX_LEN, "p%05d/%s", index, (const char*)path);

    dbg_msg(log_fp, "[%s] doing PVFS lookup for %s", __func__, physical_path);

    PVFS_sysresp_lookup lk_response;
    int ret;

    memset(&lk_response, 0, sizeof(lk_response));
    ret = PVFS_sys_ref_lookup(srv_settings.fs_id, physical_path, parent,
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

static int enter_bucket(PVFS_credentials *creds, PVFS_object_ref *handle, const char *name)
{
    struct skye_directory *dir = cache_fetch(handle);
    if (!dir)
        return -EIO;

    int index = giga_get_index_for_file(&dir->mapping, name);

    cache_return(dir);

    char physical_path[MAX_LEN];
    snprintf(physical_path, MAX_LEN, "p%05d", index);

    dbg_msg(log_fp, "[%s] descending into %s", __func__, physical_path);

    PVFS_sysresp_lookup lk_response;
    int ret;

    memset(&lk_response, 0, sizeof(lk_response));
    ret = PVFS_sys_ref_lookup(srv_settings.fs_id, physical_path, *handle,
                              creds, &lk_response, PVFS2_LOOKUP_LINK_NO_FOLLOW,
                              PVFS_HINT_NULL);
    if ( ret < 0 )
        return -1 * PVFS_get_errno_mapping(ret);

    *handle = lk_response.ref;

    return 0;
}

bool_t skye_rpc_create_1_svc(PVFS_credentials creds, PVFS_object_ref parent,
                             skye_pathname filename, mode_t mode, 
                             skye_lookup *result, struct svc_req *rqstp)
{
    (void)rqstp;
    int rc;

    if ((rc = enter_bucket(&creds, &parent, (char*)filename)) < 0){
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

    result->errnum = 0;
    result->skye_lookup_u.ref = resp_create.ref;

    return true;
}

bool_t skye_rpc_mkdir_1_svc(PVFS_credentials creds, PVFS_object_ref parent,
                            skye_pathname dirname, mode_t mode, 
                            skye_result *result, struct svc_req *rqstp)
{
    (void)rqstp;
    int rc;

    if ((rc = enter_bucket(&creds, &parent, (char*)dirname)) < 0){
        result->errnum = rc;
        return true;
    }

    PVFS_sysresp_mkdir resp_mkdir;

    /* Set attributes */
    PVFS_sys_attr attr;
    memset(&attr, 0, sizeof(PVFS_sys_attr));
    attr.owner = creds.uid;
    attr.group = creds.gid;
    attr.perms = mode;
    attr.mask = PVFS_ATTR_SYS_ALL_SETABLE;

    rc = PVFS_sys_mkdir(dirname, parent, attr, &creds, &resp_mkdir,
                            PVFS_HINT_NULL);
    if (rc != 0)
        result->errnum = -1 * PVFS_get_errno_mapping(rc);
    else
        result->errnum = 0;

    /* FIXME: handle cleanup in the case that this fails */
    parent = resp_mkdir.ref;
    dirname = "p00000";
    rc = PVFS_sys_mkdir(dirname, parent, attr, &creds, &resp_mkdir,
                            PVFS_HINT_NULL);
    if (rc != 0)
        result->errnum = -1 * PVFS_get_errno_mapping(rc);
    else
        result->errnum = 0;

	return true;
}

int isdir(PVFS_credentials *creds, PVFS_object_ref *handle){
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

bool_t skye_rpc_remove_1_svc(PVFS_credentials creds, PVFS_object_ref parent,
                            skye_pathname filename, skye_result *result, 
                            struct svc_req *rqstp)
{
    (void)rqstp;
    int rc;

    if ((rc = enter_bucket(&creds, &parent, (char*)filename)) < 0){
        result->errnum = rc;
        return true;
    }

    PVFS_sysresp_lookup lk_response;

    memset(&lk_response, 0, sizeof(lk_response));
    rc = PVFS_sys_ref_lookup(srv_settings.fs_id, filename, parent, &creds,
                              &lk_response, PVFS2_LOOKUP_LINK_NO_FOLLOW,
                              PVFS_HINT_NULL);
    if ( rc < 0 )
        return -1 * PVFS_get_errno_mapping(rc);

    if (isdir(&creds, &(lk_response.ref))){
        rc = PVFS_sys_remove("p00000", lk_response.ref, &creds, PVFS_HINT_NULL);
        if ( rc < 0 )
            return -1 * PVFS_get_errno_mapping(rc);
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

    if ((rc = enter_bucket(&creds, &src_parent, (char*)src_name)) < 0){
        result->errnum = rc;
        return true;
    }

    if ((rc = enter_bucket(&creds, &dst_parent, (char*)dst_name)) < 0){
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
