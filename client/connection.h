#ifndef CONNECTION_H
#define CONNECTION_H

#define pvfs2errno(n) ((-1)*(PVFS_get_errno_mapping(n)))

/* FIXME: really should be get_connection */
CLIENT *get_client(int serverid);

int rpc_connect();
void rpc_disconnect();

int pvfs_connect();
void gen_credentials(PVFS_credentials *credentials);

#endif