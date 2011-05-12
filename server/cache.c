#include "cache.h"
#include "common/giga_index.h"
#include "common/connection.h"
#include "common/options.h"
#include "common/trace.h"
#include <pvfs2-types.h>
#include <pvfs2-util.h>
#include <assert.h>
#include <stdio.h>
#include <time.h>

static struct skye_directory *dircache = NULL;
static pthread_rwlockattr_t rwlockattr;

int cache_init(void){
   int ret = pthread_rwlockattr_setkind_np(&rwlockattr, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);
   if (ret != 0)
       err_quit("[%s] Unable to set rwlock attributes. (ret=%d)", __func__, ret);
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

    if (pthread_rwlock_init(&dir->rwlock, &rwlockattr)){
        free(dir);
        return NULL;
    }

    memcpy(&dir->handle, handle, sizeof(PVFS_object_ref));

    unsigned int zeroth_server = pvfs_get_mds(handle);

    // FIXME: what should flag be?
    giga_init_mapping(&dir->mapping, -1, zeroth_server,
                      skye_options.servercount); 

    dir->refcount = 1; /* the hash table */

    dir->splitting_index = -1;

    HASH_ADD(hh, dircache, handle, sizeof(PVFS_object_ref), dir);

    fill_bitmap(&(dir->mapping),handle);

    return dir;
}

static struct skye_directory *fetch(PVFS_object_ref *handle){
    struct skye_directory *dir = NULL;

    HASH_FIND(hh, dircache, handle, sizeof(PVFS_object_ref), dir);

    if (!dir)
        dir = new_directory(handle);

    if (!dir){
        dbg_msg(stderr, "[%s] Unable to find or initialize skye_directory for %lu", __func__, handle->handle);
        return NULL;
    }

    /* increment ref count */
    __sync_fetch_and_add(&dir->refcount, 1);

    return dir;
}

struct skye_directory* cache_fetch(PVFS_object_ref *handle){
    struct skye_directory *dir = fetch(handle);
    if (dir)
        pthread_rwlock_rdlock(&dir->rwlock);
    return dir;
}

struct skye_directory* cache_fetch_w(PVFS_object_ref *handle){
    struct skye_directory *dir = fetch(handle);
    if (dir){
        time_t start = time(NULL);
        pthread_rwlock_wrlock(&dir->rwlock);
        time_t end = time(NULL);
        double elapsed = difftime(end, start);
        if (elapsed > 1.0)
            dbg_msg(stderr, "[%s] WARNING: took %f seconds to get writer lock.", __func__, elapsed);
    }
    return dir;
}

void cache_return(struct skye_directory *dir){
    pthread_rwlock_unlock(&dir->rwlock);

    assert(dir->refcount > 0);

    if (__sync_sub_and_fetch(&dir->refcount, 1) == 0)
        free(dir);
}

/* when an object is deleted */
void cache_destroy(struct skye_directory *dir){
    assert(dir->refcount > 1);

    /* once to release from the caller */
    __sync_fetch_and_sub(&dir->refcount, 1);

    HASH_DEL(dircache, dir);

    if (__sync_sub_and_fetch(&dir->refcount, 1) == 0)
        free(dir);
}
