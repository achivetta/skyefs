#ifndef CACHE_H
#define CACHE_H

#include <pvfs2-sysint.h>
#include "common/giga_index.h"
#include "common/uthash.h"

struct skye_bucket {
    int index;          // ID of the bucket (the 'i' in P_i)
    int state;          // status flag indicating if the bucket is splitting
    UT_hash_handle hh;
};

struct skye_directory {
    struct giga_mapping_t mapping;
    int refcount;
    PVFS_object_ref handle;         // key for the hash table
    struct skye_bucket *buckets;
    UT_hash_handle hh;
};

/* initialize the directory cache */
int cache_init(void);

/* get the skye_directory object for a given PVFS_object_ref. */
struct skye_directory* cache_fetch(PVFS_object_ref *handle);

/* return a previously fetched skye_directory.  This is necessairy because we
 * refcount skye_directory objects */
void cache_return(struct skye_directory *dir);

void cache_destroy(struct skye_directory *dir);

#endif
