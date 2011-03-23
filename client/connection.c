#include "common/defaults.h"
#include "common/skye_rpc.h"
#include "client.h"
#include "operations.h"
#include "connection.h"

#include <rpc/rpc.h>
#include <arpa/inet.h>

static CLIENT *rpc_client;

CLIENT *get_client(int serverid){
    (void)serverid;
    return rpc_client;
}

int rpc_connect()
{
    int sock = RPC_ANYSOCK;

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(client_options.port);
    if (inet_pton(AF_INET, client_options.host, &addr.sin_addr) < 0) {
        fprintf(stderr, "ERROR: handling source IP (%s). %s \n",
                DEFAULT_IP, strerror(errno));
        exit(EXIT_FAILURE);
    } 

    rpc_client = clnttcp_create (&addr, SKYE_RPC_PROG, SKYE_RPC_VERSION, &sock, 0, 0);
    if (rpc_client == NULL) {
        clnt_pcreateerror (NULL);
        exit (EXIT_FAILURE);
    }

    return 0;
}

void rpc_disconnect()
{
	clnt_destroy (rpc_client);
}
