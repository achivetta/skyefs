#include "trace.h"
#include "skye_rpc.h"
#include "server.h"
#include "defaults.h"

#include <assert.h>
#include <stdbool.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>

bool_t skye_rpc_init_1_svc(bool_t *result, struct svc_req *rqstp)
{
    dbg_msg(log_fp, "[%s] recv:init()", __func__);

    bool_t retval = 1;

    *result = true;

    return retval;
}

bool_t skye_rpc_readdir_1_svc(skye_readdir_req arg, skye_readdir_reply *result,
                              struct svc_req *rqstp)
{
    dbg_msg(log_fp, "[%s] recv:readdir(%s)", __func__, arg.path);

    DIR *dir = opendir(arg.path);
    if (!dir){
        dbg_msg(log_fp, "[%s] unable to opendir(%s): %s", __func__, arg.path,
                strerror(errno));
        result->errnum = errno;
        return true;
    }

    struct dirent *dent;

    errno = 0;
    while ((dent = readdir(dir)) != NULL){
        /* create a new dnode */
        skye_dnode *dnode = malloc(sizeof(skye_dnode));

        /* construct pathname of file to stat */
        char path_name[MAX_PATHNAME_LEN];
        if (snprintf(path_name, sizeof(path_name), "%s/%s", arg.path,
                                       dent->d_name) >= sizeof(path_name)){
            dbg_msg(log_fp, "[%s] %s/%s is longer than MAX_PATHNAME_LEN ",
                    __func__, arg.path, dent->d_name);
            free(dnode);
            continue;
        }

        /* stat file */
        if (lstat(path_name, &dnode->stbuf) < 0){
            dbg_msg(log_fp, "[%s] unable to readdir(%s): %s", __func__, arg.path,
                    strerror(errno));
            free(dnode);
            continue;
        }

        /* insert at front of list */
        dnode->next = result->skye_readdir_reply_u.dlist;
        result->skye_readdir_reply_u.dlist = dnode;
    }

    if (errno){
        dbg_msg(log_fp, "[%s] unable to readdir(%s): %s", __func__, arg.path,
                strerror(errno));
        result->errnum = errno;

        skye_dnode *dnode;
        while ((dnode = result->skye_readdir_reply_u.dlist) != NULL){
            result->skye_readdir_reply_u.dlist = dnode->next;
            free(dnode);
        }

        return true;
    }

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
