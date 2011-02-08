#ifndef CLIENT_H
#define CLIENT_H

#include <pvfs2-util.h>
#include <rpc/rpc.h>

struct client_options {
   char* host;
   int port;
   char* pvfs_spec;
};

extern PVFS_fs_id pvfs_fsid;
extern CLIENT *rpc_client;
extern struct client_options client_options;

#endif
