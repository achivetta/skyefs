#include "common/trace.h"
#include "common/skye_rpc.h"
#include "server.h"
#include "common/defaults.h"

#include <assert.h>
#include <stdbool.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>

typedef struct {
	  PVFS_object_ref	ref;
	  PVFS_credentials	creds;
} pvfs_handle_t;

/* FIXME */
static void pvfs_gen_credentials(
   PVFS_credentials *credentials)
{
   //credentials->uid = fuse_get_context()->uid;
   //credentials->gid = fuse_get_context()->gid;
   credentials->uid = 1000;
   credentials->gid = 1000;
}

/* FIXME: this will segfault if "./" is specified as the path */
static int pvfs_lookup( const char *path, pvfs_handle_t *pfh, 
				   int32_t follow_link )
{
   PVFS_sysresp_lookup lk_response;
   int ret;

   /* we don't have to do a PVFS_util_resolve
	* because FUSE resolves the path for us
	*/

   pvfs_gen_credentials(&pfh->creds);

   memset(&lk_response, 0, sizeof(lk_response));
   ret = PVFS_sys_lookup(srv_settings.fs_id, (char *)path, &pfh->creds,
                         &lk_response, follow_link, PVFS_HINT_NULL);
   if ( ret < 0 ) {
	  return ret;
   }

   pfh->ref.handle = lk_response.ref.handle;
   pfh->ref.fs_id  = srv_settings.fs_id;

   return 0;
}

bool_t skye_rpc_init_1_svc(bool_t *result, struct svc_req *rqstp)
{
    assert(result);

    dbg_msg(log_fp, "[%s] recv:init()", __func__);

    *result = true;

    return true;
}

#define MAX_NUM_DIRENTS 25

bool_t skye_rpc_readdir_1_svc(skye_pathname path, skye_dirlist *result,  struct
                              svc_req *rqstp)
{
    assert(result);
    int ret;

    dbg_msg(log_fp, "[%s] recv:readdir(%s)", __func__, path);

    pvfs_handle_t pfh;

    ret = pvfs_lookup( path, &pfh, PVFS2_LOOKUP_LINK_FOLLOW );
    if ( ret < 0 ){
        result->errnum = -ret;
        return true;
    }

    result->errnum = 0;
    result->skye_dirlist_u.dlist = NULL;

    /* while we keep maxing out our response size... */
    int pvfs_dirent_incount = MAX_NUM_DIRENTS;
    PVFS_ds_position token = 0;
    PVFS_sysresp_readdir rd_response;
    do {
        memset(&rd_response, 0, sizeof(PVFS_sysresp_readdir));

        ret = PVFS_sys_readdir(pfh.ref, (!token ? PVFS_READDIR_START : token),
                               pvfs_dirent_incount, &pfh.creds, &rd_response,
                               PVFS_HINT_NULL);
        if(ret < 0){
            result->errnum = -ret;
            return true;
        }

        /* for each returned file */
        for(int i = 0; i < rd_response.pvfs_dirent_outcount; i++) {
            /* create a new dnode */
            skye_dnode *dnode = calloc(sizeof(skye_dnode),1);

            char *cur_file = rd_response.dirent_array[i].d_name;

            /* populate filename */
            dnode->name = strdup(cur_file);
            if (dnode->name == NULL){
                dbg_msg(log_fp, "[%s] Unable to duplicate string \"%s\". ",
                        __func__, cur_file);
                free(dnode);
                continue;
            }

            /* insert at front of list */
            dnode->next = result->skye_dirlist_u.dlist;
            result->skye_dirlist_u.dlist = dnode;
        }

        token += rd_response.pvfs_dirent_outcount;

        if (rd_response.pvfs_dirent_outcount) {
            free(rd_response.dirent_array);
            rd_response.dirent_array = NULL;
        }
    } while (rd_response.pvfs_dirent_outcount == pvfs_dirent_incount);

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

