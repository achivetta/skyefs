#include "common/trace.h"
#include "common/skye_rpc.h"
#include "server.h"
#include "common/defaults.h"

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

    PVFS_sysresp_lookup lk_response;
    int ret;

    memset(&lk_response, 0, sizeof(lk_response));
    ret = PVFS_sys_ref_lookup(srv_settings.fs_id, (char *)path, parent,
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

    int rc = PVFS_sys_create(filename, parent, attr, &creds, NULL, &resp_create,
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

    PVFS_sysresp_mkdir resp_mkdir;

    /* Set attributes */
    PVFS_sys_attr attr;
    memset(&attr, 0, sizeof(PVFS_sys_attr));
    attr.owner = creds.uid;
    attr.group = creds.gid;
    attr.perms = mode;
    attr.mask = PVFS_ATTR_SYS_ALL_SETABLE;

    int rc = PVFS_sys_mkdir(dirname, parent, attr, &creds, &resp_mkdir,
                            PVFS_HINT_NULL);
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

    int rc = PVFS_sys_rename(src_name, src_parent, dst_name, dst_parent,
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
