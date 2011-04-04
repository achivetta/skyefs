#include "cache.h"
#include <assert.h>

/* FIXME: this file is not thread safe */

static struct skye_directory *dircache = NULL;

int cache_init(){
   return 0; 
}

/* FIXME: need better error handleing here, any specific PVFS error should
 * propigate back */
static struct skye_directory* new_directory(PVFS_object_ref *handle){
    struct skye_directory *dir = malloc(sizeof(struct skye_directory));
    if (!dir)
        return NULL;

    memcpy(&dir->handle, handle, sizeof(PVFS_object_ref));
    giga_init_mapping(&dir->mapping, -1); // FIXME: what should flag be?
    dir->refcount = 0;
    dir->buckets = NULL;
    HASH_ADD(hh, dircache, handle, sizeof(PVFS_object_ref), dir);

    return dir;
}

struct skye_directory* cache_fetch(PVFS_object_ref *handle){
    struct skye_directory *dir = NULL;

    HASH_FIND(hh, dircache, handle, sizeof(PVFS_object_ref), dir);

    if (!dir)
        dir = new_directory(handle);

    if (!dir)
        return NULL;

    dir->refcount++;

    return dir;
}

void cache_return(struct skye_directory *dir){
    assert(dir->refcount > 0);
    dir->refcount--;
}
