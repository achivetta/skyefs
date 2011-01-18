#include "trace.h"
#include "skyefs_rpc.h"

#include <assert.h>

bool_t
skye_rpc_null_1_svc(void *result, struct svc_req *rqstp)
{
    dbg_msg(log_fp, "[%s] recv(NULL_REQ) ... send(NULL_REPLY)", __func__);
    return 1;
}

bool_t
skye_rpc_init_1_svc(skye_init arg1, skye_init *result,  struct svc_req *rqstp)
{
    dbg_msg(log_fp, "[%s] recv(INIT_REQ)", __func__);

    assert(result);

    bzero(result, sizeof(skye_init)); // FIXME: Do I need to do this?

    dbg_msg(log_fp, "[%s] send(INIT_REPLY)", __func__);
	return 1;
}

bool_t
skye_rpc_readdir_1_svc(skye_readdir_req arg1, skye_readdir_reply *result,  struct svc_req *rqstp)
{
	bool_t retval = 1;

	return retval;
}

int
skye_rpc_prog_1_freeresult (SVCXPRT *transp, xdrproc_t xdr_result, caddr_t result)
{
	xdr_free (xdr_result, result);

	/*
	 * Insert additional freeing code here, if needed
	 */

	return 1;
}
