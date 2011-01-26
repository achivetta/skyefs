#include "common/skye_rpc.h"
#include "common/defaults.h"
#include "client.h"

#include <rpc/rpc.h>
#include <fuse.h>
#include <arpa/inet.h>
#include <errno.h>

static CLIENT *skye_client = NULL;

void* skye_init(struct fuse_conn_info *conn){
    int sock = RPC_ANYSOCK;

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(client_options.port);
    if (inet_pton(AF_INET, client_options.host, &addr.sin_addr) < 0) {
        fprintf(stderr, "ERROR: handling source IP (%s). %s \n",
                DEFAULT_IP, strerror(errno));
        exit(EXIT_FAILURE);
    } 
    
	skye_client = clnttcp_create (&addr, SKYE_RPC_PROG, SKYE_RPC_VERSION, &sock, 0, 0);
	if (skye_client == NULL) {
		clnt_pcreateerror (NULL);
		exit (EXIT_FAILURE);
	}

    return NULL;
}

int skye_readdir(char * path, void * buf, fuse_fill_dir_t filler, off_t offset,
                 struct fuse_file_info *fi){
    skye_dirlist result;
    bzero(&result,sizeof(result));

	enum clnt_stat retval = skye_rpc_readdir_1(path, &result, skye_client);
	if (retval != RPC_SUCCESS) {
		clnt_perror (skye_client, "call failed");
        return -EIO;
	} else if (result.errnum != 0){
        return -result.errnum;
    } 

    skye_dnode *dnode = result.skye_dirlist_u.dlist;
    while (dnode != NULL){
        if (filler(buf, dnode->name, &dnode->stbuf, 0) != 0)
            return -EIO;
        dnode = dnode->next;
    }

    return 0;
}

void skye_destroy(void * ptr){
	clnt_destroy (skye_client);
}
