#ifndef CACHE_H
#define CACHE_H

#include <pvfs2-sysint.h>
#include "common/giga_index.h"
#include "common/uthash.h"

struct skye_directory {
    struct giga_mapping_t mapping;
    int refcount;
    PVFS_object_ref handle;
    UT_hash_handle hh;
};

/* initialize the directory cache */
int cache_init();

/* get the skye_directory object for a given PVFS_object_ref. */
struct skye_directory* cache_fetch(PVFS_object_ref *handle);

/* return a previously fetched skye_directory.  This is necessairy because we
 * refcount skye_directory objects */
void cache_return(struct skye_directory *dir);

#endif
