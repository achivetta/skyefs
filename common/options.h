#ifndef OPTIONS_H
#define OPTIONS_H

#include <pvfs2-util.h>
#include <rpc/rpc.h>

struct skye_options {
   char* pvfs_spec;
   int servercount;
   const char ** serverlist;
};

extern struct skye_options skye_options;

#endif
