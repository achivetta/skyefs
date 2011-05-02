#include "cache.h"
#include "common/giga_index.h"
#include "common/connection.h"
#include "common/options.h"
#include "common/trace.h"
#include <pvfs2-types.h>
#include <pvfs2-util.h>
#include <assert.h>
#include <stdio.h>

/* FIXME: this file is not thread safe */

static struct skye_directory *dircache = NULL;

int cache_init(void){
   return 0; 
}

static void fill_bitmap(struct giga_mapping_t *mapping, PVFS_object_ref *handle){
    PVFS_sysresp_readdir rd_response;
    unsigned int pvfs_dirent_incount = 32; // reasonable chank size
    PVFS_ds_position token = 0;
    PVFS_credentials credentials; PVFS_util_gen_credentials(&credentials);

    (void)mapping;

    do {
        PVFS_dirent *cur_file = NULL;
        unsigned int i;

        memset(&rd_response, 0, sizeof(PVFS_sysresp_readdir));
        int ret = PVFS_sys_readdir(*handle, (!token ? PVFS_READDIR_START : token),
                                pvfs_dirent_incount, &credentials, &rd_response,
                                PVFS_HINT_NULL);
        if (ret < 0)
            return;

        for (i = 0; i < rd_response.pvfs_dirent_outcount; i++) {
            cur_file = &(rd_response.dirent_array[i]);

            index_t index;
            if (sscanf(cur_file->d_name, "p%u", &index) == 1)
                giga_update_mapping(mapping, index);
            else
                err_msg("[%s] Unable to add directory %s to bitmap for #{%lu}\n", __func__, cur_file->d_name, handle->handle);

            //TODO: establish bucket structures here
        }
        
        if (!token)
            token = rd_response.pvfs_dirent_outcount - 1;
        else
            token += rd_response.pvfs_dirent_outcount;

        if (rd_response.pvfs_dirent_outcount) {
            free(rd_response.dirent_array);
            rd_response.dirent_array = NULL;
        }

    } while(rd_response.pvfs_dirent_outcount == pvfs_dirent_incount);
}

/* FIXME: need better error handleing here, any specific PVFS error should
 * propigate back */
static struct skye_directory* new_directory(PVFS_object_ref *handle){
    struct skye_directory *dir = malloc(sizeof(struct skye_directory));
    if (!dir)
        return NULL;

    memcpy(&dir->handle, handle, sizeof(PVFS_object_ref));

    unsigned int zeroth_server = pvfs_get_mds(handle);

    // FIXME: what should flag be?
    giga_init_mapping(&dir->mapping, -1, zeroth_server,
                      skye_options.servercount); 
    dir->refcount = 1; /* the hash table */
    dir->buckets = NULL;
    HASH_ADD(hh, dircache, handle, sizeof(PVFS_object_ref), dir);

    fill_bitmap(&(dir->mapping),handle);

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

    if (dir->refcount == 0)
        free(dir);
}

/* when an object is deleted */
void cache_destroy(struct skye_directory *dir){
    assert(dir->refcount > 1);
    dir->refcount--; /* once to release from the caller */

    HASH_DEL(dircache, dir);
    dir->refcount--; /* another to release from the hash table */

    if (dir->refcount == 0)
        free(dir);
}

void cache_invalidate(PVFS_object_ref *handle){
    struct skye_directory *dir = cache_fetch(handle);
    cache_destroy(dir);
}
