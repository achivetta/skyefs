/* FIXME: not all of these are needed */
#include "common/defaults.h"
#include "common/trace.h"
#include "common/skye_rpc.h"
#include "common/options.h"
#include "common/connection.h"
#include "common/utlist.h"
#include "server.h"
#include "cache.h"
#include "split.h"

#include <assert.h>
#include <stdbool.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#include <pvfs2-util.h>
#include <pvfs2-sysint.h>
#include <pvfs2-mgmt.h>

struct split_task {
    PVFS_object_ref parent;
    index_t pindex;
    struct split_task *prev;
    struct split_task *next;
};

struct split_task *queue = NULL;
pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;

static void do_split(PVFS_object_ref parent, index_t pindex);

void * split_thread(void * unused){
    (void)unused;

    int ret = pthread_mutex_lock(&queue_mutex);
    if (ret)
        err_dump("Split thread unable to take queue mutex");

    while (!pthread_cond_wait(&queue_cond, &queue_mutex)){
        while (queue){
            struct split_task *task = queue;
            DL_DELETE(queue, task);

            pthread_mutex_unlock(&queue_mutex);
            do_split(task->parent, task->pindex);
            pthread_mutex_lock(&queue_mutex);

            free(task);
        }
    }

    return NULL;
}

void perform_split(PVFS_object_ref *parent, index_t pindex){

    int ret = pthread_mutex_lock(&queue_mutex);
    if (ret) err_dump("unable to lock queue_mutex");

    struct split_task *last = NULL;
    if (queue)
        last = queue->prev;
    if (last && memcmp(&last->parent, parent, sizeof(parent)) == 0 &&
        memcmp(&last->pindex, &pindex, sizeof(pindex)) == 0){
        ret = pthread_mutex_unlock(&queue_mutex);
        if (ret) err_dump("unable to unlock queue_mutex");

        return;
    }

    struct split_task *task = malloc(sizeof(struct split_task));
    if (!task)
        return;

    task->parent = *parent;
    task->pindex = pindex;

    DL_APPEND(queue, task);

    ret = pthread_cond_signal(&queue_cond);
    if (ret) err_dump("unable to signal queue_cond");

    ret = pthread_mutex_unlock(&queue_mutex);
    if (ret) err_dump("unable to unlock queue_mutex");
}

static void do_split(PVFS_object_ref parent, index_t pindex){
    PVFS_credentials creds; PVFS_util_gen_credentials(&creds);
    index_t cindex;
    PVFS_object_ref phandle, chandle; /* child/logical directory handles */
    int ret;

    dbg_msg(stderr, "[%s] START splitting %lu/%d", __func__, parent.handle, pindex);
    time_t startime = time(NULL);

    struct skye_directory *dir = cache_fetch_w(&parent);
    if (!dir){
        return;
    }

    cindex = giga_index_for_splitting(&dir->mapping, pindex);

    char physical_path[MAX_LEN];
    PVFS_sysresp_lookup lk_response;

    /* lookup parent bucket's handle */
    snprintf(physical_path, MAX_LEN, "p%05d", pindex);
    memset(&lk_response, 0, sizeof(lk_response));
    ret = PVFS_sys_ref_lookup(pvfs_fsid, physical_path, parent,
                              &creds, &lk_response, PVFS2_LOOKUP_LINK_NO_FOLLOW,
                              PVFS_HINT_NULL);
    if ( ret < 0 ){
        err_msg("Couldn't lookup parent handle! (%d)\n", ret);
        goto exit;
    }
    phandle = lk_response.ref;

    if (!isdir_overflow(&creds, &phandle)){
        dbg_msg(stderr, "[%s] No need to split %lu/%d", __func__, parent.handle, pindex);
        goto exit;
    }

    //(3) create a new partition (use giga to find the new index)

    snprintf(physical_path, MAX_LEN, "p%05d", cindex);
    int server = giga_get_server_for_index(&dir->mapping, cindex);


    /* FIXME: clone attributes of parent directory */
    PVFS_sys_attr attr;
    memset(&attr, 0, sizeof(PVFS_sys_attr));
    attr.owner = creds.uid;
    attr.group = creds.gid;
    attr.perms = 0777;
    attr.mask = PVFS_ATTR_SYS_ALL_SETABLE;

    // FIXME: this really shouldn't be called p000x yet in case we die...
    ret = pvfs_mkdir_server(&creds, &parent, physical_path, &attr, server, &chandle);
    if (ret < 0){
        err_msg("couldn't create child bucket!\n");
        goto exit;
    }

    dir->splitting_index = pindex;

    /*
    cache_return(dir);
    dir = NULL;
    */
    
    //(4) readdir() old partition, rename the files that will move

    dbg_msg(stderr, "[%s] beginning move of files in %lu/%d", __func__, parent.handle, pindex);
    int moved;
    do {
        moved = 0;
        PVFS_sysresp_readdir rd_response;
        unsigned int pvfs_dirent_incount = 128; // reasonable chank size
        PVFS_ds_position token = 0;

        do {
            int i, issued = 0;

            memset(&rd_response, 0, sizeof(PVFS_sysresp_readdir));
            ret = PVFS_sys_readdir(phandle, (!token ? PVFS_READDIR_START : token),
                                   pvfs_dirent_incount, &creds, &rd_response,
                                   PVFS_HINT_NULL);
            if (ret < 0){
                err_dump("couldn't list parent directory");
                goto exit; /* FIXME: handle error */
            }

            if (!rd_response.pvfs_dirent_outcount) goto next_batch;

            /* issue requests */
            for (i = 0; (unsigned int)i < rd_response.pvfs_dirent_outcount; i++) {
                char *name = rd_response.dirent_array[i].d_name;

                if (!giga_file_migration_status(name, cindex)){
                    continue;
                }

                ret = PVFS_sys_rename(name, phandle, 
                                      name, chandle, 
                                      &creds, PVFS_HINT_NULL);


                if (ret)
                    dbg_msg(stderr, "[%s] WARNING: Unable to rename file (%d)", __func__, PVFS_get_errno_mapping(ret));
                else
                    issued++;

                moved++;
            }

            /* cleanup */
            free(rd_response.dirent_array);
            rd_response.dirent_array = NULL;

next_batch:
            if (!token)
                token = rd_response.pvfs_dirent_outcount - 1;
            else
                token += rd_response.pvfs_dirent_outcount;

        } while(rd_response.pvfs_dirent_outcount == pvfs_dirent_incount);

        dbg_msg(stderr, "[%s] moved %d files in %lu/%d", __func__, moved, parent.handle, pindex);

    } while (moved > 0);

    dbg_msg(stderr, "[%s] completed move of files in %lu/%d", __func__, parent.handle, pindex);

    //(5) after all entries have been moved, send RPC to server
    
    // FIXME: if this fails?!?
    if (server != skye_options.servernum){
        CLIENT *client = get_connection(server);
        ret = skye_rpc_bucket_add_1(parent, cindex, &ret, client);
        if (ret != RPC_SUCCESS){
            clnt_perror(client, "RPC bucket remove failed");
            err_dump("Unable to inform new server of new bucket.");
        }
    }

    dir->splitting_index = -1;
    giga_update_mapping(&dir->mapping, cindex);

    time_t endtime = time(NULL);
    double elapsedtime = difftime(endtime, startime);
    dbg_msg(stderr, "[%s] DONE splitting %lu/%d in %f seconds", __func__, parent.handle, pindex, elapsedtime);

exit:
    if (dir)
        cache_return(dir);
}

int pvfs_mkdir_server(PVFS_credentials *creds, PVFS_object_ref *parent, 
                        char *dirname, PVFS_sys_attr *attr, int server, 
                        PVFS_object_ref *child)
{
    (void)server;
    PVFS_sysresp_mkdir resp_mkdir;

    int created_on = -1;
    PVFS_error rc = 0;
    
    while (1){
        rc = PVFS_sys_mkdir(dirname, *parent, *attr, creds, &resp_mkdir,
                            PVFS_HINT_NULL);
        if (rc != 0)
            return -1 * PVFS_get_errno_mapping(rc);

        created_on = pvfs_get_mds(&resp_mkdir.ref);

        if (created_on == server)
            break;

        rc = PVFS_sys_remove(dirname, *parent, creds, PVFS_HINT_NULL);
        if (rc != 0)
            return -1 * PVFS_get_errno_mapping(rc);
    }

    if (child)
        memcpy(child, &(resp_mkdir.ref), sizeof(*child));

    return 0;
}

int isdir_overflow(PVFS_credentials *creds, PVFS_object_ref *handle)
{
    int	ret;

    PVFS_sysresp_getattr getattr_response;
    PVFS_sys_attr*	attrs;
    memset(&getattr_response,0, sizeof(PVFS_sysresp_getattr));

    ret = PVFS_sys_getattr(*handle, PVFS_ATTR_SYS_DIRENT_COUNT, creds,
                           &getattr_response, PVFS_HINT_NULL);
    if (ret < 0 )
        return -1;

    attrs = &getattr_response.attr;

    //XXX: fix the size for now
    if (attrs->dirent_count > SPLIT_THRESHOLD){
        return 1;
    }
    return 0;
} 
