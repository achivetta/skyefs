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

/* FIXME */
static void pvfs_gen_credentials(PVFS_credentials *credentials)
{
   //credentials->uid = fuse_get_context()->uid;
   //credentials->gid = fuse_get_context()->gid;
   credentials->uid = 1000;
   credentials->gid = 1000;
}

bool_t skye_rpc_lookup_1_svc(PVFS_object_ref parent, skye_pathname path,
                             skye_lookup *result, struct svc_req *rqstp)
{
    (void)rqstp;

    PVFS_sysresp_lookup lk_response;
    PVFS_credentials	creds;
    int ret;

    pvfs_gen_credentials(&creds);

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

bool_t skye_rpc_init_1_svc(bool_t *result, struct svc_req *rqstp)
{
    (void)rqstp;
    assert(result);

    dbg_msg(log_fp, "[%s] recv:init()", __func__);

    *result = true;

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

