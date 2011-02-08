#include "common/skye_rpc.h"
#include "common/defaults.h"
#include "client.h"

#include <rpc/rpc.h>
#include <fuse.h>
#include <arpa/inet.h>
#include <errno.h>

int skye_readdir(char * path, void * buf, fuse_fill_dir_t filler, off_t offset,
                 struct fuse_file_info *fi){
    /*
    skye_dirlist result;
    bzero(&result,sizeof(result));

	enum clnt_stat retval = skye_rpc_readdir_1(path, &result, skye_client);
	if (retval != RPC_SUCCESS) {
		clnt_perror (skye_client, "call failed");
        return -EIO;
	} else if (result.errnum != 0){
        return result.errnum;
    } 

    skye_dnode *dnode = result.skye_dirlist_u.dlist;
    while (dnode != NULL){
        if (filler(buf, dnode->name, &dnode->stbuf, 0) != 0)
            return -EIO;
        dnode = dnode->next;
    }
    */
    return 0;
}

int skye_getattr(char *path, struct stat *stbuf){
    /*
    skye_stat result;
    bzero(&result,sizeof(result));

	enum clnt_stat retval = skye_rpc_getattr_1(path, &result, skye_client);
	if (retval != RPC_SUCCESS) {
		clnt_perror (skye_client, "call failed");
        return -EIO;
	} else if (result.errnum != 0){
        return result.errnum;
    } 

    memcpy(stbuf, &result.skye_stat_u.stbuf, sizeof(struct stat));
    */
    return 0;
}
