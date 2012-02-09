#ifndef CONNECTION_H
#define CONNECTION_H
#include <rpc/clnt.h>
#include <rpc/pmap_clnt.h>
#include <rpc/rpc.h>

#define pvfs2errno(n) ((-1)*(PVFS_get_errno_mapping(n)))

extern PVFS_fs_id pvfs_fsid;

CLIENT *get_connection(int serverid);

int rpc_connect(void);
void rpc_disconnect(void);

int pvfs_connect(char *);

int pvfs_get_mds(PVFS_object_ref *ref);

extern char *myhostname;

#endif
