#ifndef SERVER_H
#define SERVER_H   

#include <inttypes.h>
#include <stdio.h>

#include "skyefs_rpc.h"

#define NUM_BACKLOG_CONN 128

struct server_settings {
    int     port_num;
    char    *mount_point;
};
struct server_settings srv_settings;

#endif /* SERVER_H */
