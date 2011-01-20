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
    assert(result);

    dbg_msg(log_fp, "[%s] recv:init()", __func__);

    *result = true;

    return true;
}

bool_t skye_rpc_readdir_1_svc(skye_pathname path, skye_dirlist *result,  struct
                              svc_req *rqstp)
{
    assert(result);

    dbg_msg(log_fp, "[%s] recv:readdir(%s)", __func__, path);

    DIR *dir = opendir(path);
    if (!dir){
        dbg_msg(log_fp, "[%s] unable to opendir(%s): %s", __func__, path,
                strerror(errno));
        result->errnum = errno;
        return true;
    }

    result->errnum = 0;
    result->skye_dirlist_u.dlist = NULL;

    struct dirent *dent;

    errno = 0;
    while ((dent = readdir(dir)) != NULL){
        /* create a new dnode */
        skye_dnode *dnode = calloc(sizeof(skye_dnode),1);

        /* populate filename */
        dnode->name = strdup(dent->d_name);
        if (dnode->name == NULL){
            dbg_msg(log_fp, "[%s] Unable to duplicate string \"%s\". ",
                    __func__, dent->d_name);
            free(dnode);
            continue;
        }

        /* construct pathname of file to stat */
        char path_name[MAX_PATHNAME_LEN];
        if (snprintf(path_name, sizeof(path_name), "%s/%s", path,
                                       dent->d_name) >= sizeof(path_name)){
            dbg_msg(log_fp, "[%s] %s/%s is longer than MAX_PATHNAME_LEN ",
                    __func__, path, dent->d_name);
            free(dnode->name);
            free(dnode);
            continue;
        }

        /* stat file */
        if (lstat(path_name, &dnode->stbuf) < 0){
            dbg_msg(log_fp, "[%s] unable to lstat(%s): %s", __func__, path_name,
                    strerror(errno));
            free(dnode->name);
            free(dnode);
            continue;
        }

        /* insert at front of list */
        dnode->next = result->skye_dirlist_u.dlist;
        result->skye_dirlist_u.dlist = dnode;
    }

    if (errno != 0){
        dbg_msg(log_fp, "[%s] unable to readdir(%s): %s", __func__, path,
                strerror(errno));
        result->errnum = errno;

        skye_dnode *dnode;
        while ((dnode = result->skye_dirlist_u.dlist) != NULL){
            result->skye_dirlist_u.dlist = dnode->next;
            free(dnode->name);
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

