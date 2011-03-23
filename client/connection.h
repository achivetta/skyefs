#ifndef CONNECTION_H
#define CONNECTION_H

CLIENT *get_client(int serverid);

int rpc_connect();
void rpc_disconnect();

#endif
