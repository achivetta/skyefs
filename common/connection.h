#ifndef CONNECTION_H
#define CONNECTION_H

#define pvfs2errno(n) ((-1)*(PVFS_get_errno_mapping(n)))

extern PVFS_fs_id pvfs_fsid;

CLIENT *get_connection(int serverid);

int rpc_connect(void);
void rpc_disconnect(void);

int pvfs_connect(char *);

#endif
