#ifndef CLIENT_H
#define CLIENT_H

#include <pvfs2-util.h>
#include <rpc/rpc.h>

struct client_options {
   char* pvfs_spec;
   int servercount;
   const char ** serverlist;
};

extern PVFS_fs_id pvfs_fsid;
extern struct client_options client_options;

#endif
