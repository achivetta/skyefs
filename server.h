#ifndef SERVER_H
#define SERVER_H   

#define NUM_BACKLOG_CONN 128

/* FIXME: do we really need this? */
struct server_settings {
    int     port_num;
    char    *mount_point;
};
struct server_settings srv_settings;

#endif /* SERVER_H */
