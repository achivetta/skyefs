#include "skye_rpc.h"
#include "defaults.h"

#include <errno.h>

int main (int argc, char *argv[])
{
	CLIENT *clnt;
	enum clnt_stat retval;
	skye_dirlist result;
	skye_pathname path = "./";
    int sock = RPC_ANYSOCK;

    bzero(&result,sizeof(result));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(DEFAULT_PORT);
    if (inet_pton(AF_INET, DEFAULT_IP, &addr.sin_addr) < 0) {
        fprintf(stderr, "ERROR: handling source IP (%s). %s \n",
                DEFAULT_IP, strerror(errno));
        exit(EXIT_FAILURE);
    } 

	clnt = clnttcp_create (&addr, SKYE_RPC_PROG, SKYE_RPC_VERSION, &sock, 0, 0);
	if (clnt == NULL) {
		clnt_pcreateerror (NULL);
		exit (EXIT_FAILURE);
	}

	retval = skye_rpc_readdir_1(path, &result, clnt);
	if (retval != RPC_SUCCESS) {
		clnt_perror (clnt, "call failed");
	} else if (result.errnum != 0){
        printf("errno: %d\n", result.errnum);
    } else {
        skye_dnode *dnode = result.skye_dirlist_u.dlist;
        while (dnode != NULL){
            printf("%s\n",dnode->name);
            dnode = dnode->next;
        }
    }

	clnt_destroy (clnt);

    return 0;
}
