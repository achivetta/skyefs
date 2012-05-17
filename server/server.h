/* This file is part of SkyeFS, an implementatino of Giga+ on PVFS.
 *
 * Copyright 2011-2012, Anthony Chivetta <anthony@chivetta.org>
 *
 * See COPYING for licence information.
 */
#ifndef SERVER_H
#define SERVER_H   

#include <semaphore.h>
#include <pvfs2-sysint.h>

#define NUM_BACKLOG_CONN 128

#define SPLIT_THRESHOLD 4000

/* FIXME: do we really need this? */
struct server_settings {
    int port_num;
};
extern struct server_settings srv_settings;

extern sem_t flow_sem;

#endif /* SERVER_H */
