#ifndef SERVER_H
#define SERVER_H   

#include <pvfs2-sysint.h>

#define NUM_BACKLOG_CONN 128

/* FIXME: do we really need this? */
struct server_settings {
    int port_num;
    struct PVFS_sys_mntent mntent;
    PVFS_fs_id fs_id;
};
extern struct server_settings srv_settings;

#endif /* SERVER_H */