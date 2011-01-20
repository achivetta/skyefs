#include "skye_rpc.h"
#include "defaults.h"

#include <errno.h>
#include <unistd.h>

void ls(char *server, int port, skye_pathname path){
	CLIENT *clnt;
	enum clnt_stat retval;
	skye_dirlist result;
    int sock = RPC_ANYSOCK;

    bzero(&result,sizeof(result));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, server, &addr.sin_addr) < 0) {
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
}

int main (int argc, char *argv[])
{
    char *server = DEFAULT_IP;
    int port = DEFAULT_PORT;
    skye_pathname path = DEFAULT_MOUNT;

    char c;
    while (-1 != (c = getopt(argc, argv,
                             "h:"
                             "p:"
                             "P:"
           ))) {
        switch(c) {
            case 'h':
                server = optarg;
                break;
            case 'p':
                port = atoi(optarg);
                break;
            case 'P':
                path = optarg;
                break;
            default:
                printf("usage: %s [-h ip_address] [-p port] [-P path]\n", argv[0]);
                exit(1);
                break;
        }

    }

    fprintf(stderr, "Listing skyefs://%s:%d/%s\n",server,port,path);
    ls(server, port, path);

    return 0;
}
