#ifndef CACHE_H
#define CACHE_H

#include <pthread.h>
#include <pvfs2-sysint.h>
#include "common/giga_index.h"
#include "common/uthash.h"

/** Contains the metadata for a logical directory. */
struct skye_directory {
    /** The Giga+ mapping. */
    struct giga_mapping_t mapping;

    /** A reference count */
    int refcount;

    /** The PVFS handle for the logical directory.
     * This is also used as the key for the hash table. */
    PVFS_object_ref handle;

    /** The index for the currently splitting partition.
     * -1 if no partition is currently splitting. */
    int splitting_index;

    /* A reader/writer lock for the structure.  
     *
     * This lock is taken in reader mode by threads operating inside the
     * directory.  It is taken in writer mode at the start and end of split
     * operations. */
    pthread_rwlock_t rwlock;

    /* The hashtable handle */
    UT_hash_handle hh;

    /* The cache of partition handles.
     * We are delibrarely racy here.  Either a slot is filled or not.  It's okay
     * if we fill it twice.
     */
    int partition_handles_length;
    PVFS_handle *partition_handles;
};

/* initialize the directory cache */
int cache_init(void);

/* get the skye_directory object for a given PVFS_object_ref. */
struct skye_directory* cache_fetch(PVFS_object_ref *handle);
struct skye_directory* cache_fetch_w(PVFS_object_ref *handle);

/* return a previously fetched skye_directory.  This is necessairy because we
 * refcount skye_directory objects */
void cache_return(struct skye_directory *dir);

void cache_destroy(struct skye_directory *dir);

#endif
