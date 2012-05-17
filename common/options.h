/* This file is part of SkyeFS, an implementatino of Giga+ on PVFS.
 *
 * Copyright 2011-2012, Anthony Chivetta <anthony@chivetta.org>
 *
 * See COPYING for licence information.
 */
#ifndef OPTIONS_H
#define OPTIONS_H

#include <pvfs2-util.h>
#include <rpc/rpc.h>

struct skye_options {
   char* pvfs_spec;
   int servercount;
   int servernum;
   const char ** serverlist;
   PVFS_BMI_addr_t *serveraddrs;
};

extern struct skye_options skye_options;

#endif
