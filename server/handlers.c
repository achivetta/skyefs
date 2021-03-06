/* This file is part of SkyeFS, an implementatino of Giga+ on PVFS.
 *
 * Copyright 2011-2012, Anthony Chivetta <anthony@chivetta.org>
 *
 * See COPYING for licence information.
 */
#include "common/defaults.h"
#include "common/trace.h"
#include "common/skye_rpc.h"
#include "common/options.h"
#include "common/connection.h"
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

static PVFS_handle get_partition_handle(PVFS_credentials *creds, struct skye_directory *dir, index_t index)
{
    index_t bucket_number = giga_get_bucket_num_for_server(&dir->mapping, index);

    if (bucket_number >= dir->partition_handles_length - 1){
        dir->partition_handles = realloc(dir->partition_handles, dir->partition_handles_length * sizeof(PVFS_handle) * 2);
        assert(dir->partition_handles); // FIXME: handle the allocation error
        int i;
        for (i = dir->partition_handles_length; i < dir->partition_handles_length * 2; i++)
            dir->partition_handles[i] = 0;
        dir->partition_handles_length = dir->partition_handles_length * 2;
    }

    if (dir->partition_handles[bucket_number] != 0)
        return dir->partition_handles[bucket_number];

    char physical_path[MAX_LEN];
    snprintf(physical_path, MAX_LEN, "p%05d", index);

    PVFS_sysresp_lookup lk_response;
    memset(&lk_response, 0, sizeof(lk_response));
    int ret = PVFS_sys_ref_lookup(pvfs_fsid, physical_path, dir->handle,
                              creds, &lk_response, PVFS2_LOOKUP_LINK_NO_FOLLOW,
                              PVFS_HINT_NULL);

    // FIXME: There's an error path here that diserves more consideration. Why
    // did we fail?
    assert(ret == 0);

    dir->partition_handles[bucket_number] = lk_response.ref.handle;
    return dir->partition_handles[bucket_number];
}

static int enter_bucket(PVFS_credentials *creds, struct skye_directory *dir, const char *name, PVFS_object_ref *handle, skye_bitmap *bitmap)
{
    int index = giga_get_index_for_file(&(dir->mapping), name);

    /* don't do locality check if not provided bitmap to update */
    if (bitmap){
        int server = giga_get_server_for_index(&(dir->mapping), index);

        if (server != skye_options.servernum){
            memcpy(bitmap, &dir->mapping, sizeof(dir->mapping));
            return -EAGAIN;
        }
    }

    /* If we are currently splitting, try the child directory */
    if (index == dir->splitting_index){
        int cindex = giga_index_for_splitting(&dir->mapping, index);
        if (giga_file_migration_status(name, cindex))
            index = cindex;

        char physical_path[MAX_LEN];
        snprintf(physical_path, MAX_LEN, "p%05d", index);

        PVFS_sysresp_lookup lk_response;

        memset(&lk_response, 0, sizeof(lk_response));
        int ret = PVFS_sys_ref_lookup(pvfs_fsid, physical_path, *handle,
                                      creds, &lk_response, PVFS2_LOOKUP_LINK_NO_FOLLOW,
                                      PVFS_HINT_NULL);

        ret =  -1 * PVFS_get_errno_mapping(ret);

        *handle = lk_response.ref;
        return ret;
    } else {
        *handle = dir->handle;
        handle->handle = get_partition_handle(creds, dir, index);
        return 0;
    }

}

static int isdir(PVFS_credentials *creds, PVFS_object_ref *handle)
{
    PVFS_sysresp_getattr getattr_response;
    PVFS_sys_attr*	attrs;

    int			ret;

    memset(&getattr_response,0, sizeof(PVFS_sysresp_getattr));

    ret = PVFS_sys_getattr(*handle, PVFS_ATTR_SYS_ALL_NOHINT, creds,
                           &getattr_response, PVFS_HINT_NULL);
    if ( ret < 0 )
        return 0;

    attrs = &getattr_response.attr;

    if (attrs->objtype == PVFS_TYPE_DIRECTORY)
        return 1;
    return 0;
} 

sem_t flow_sem;
static int flow_start()
{
    struct timespec ts;
    gettimeofday((struct timeval*)&ts,NULL);
    ts.tv_sec += 50;
    ts.tv_nsec *= 1000;

    return sem_timedwait(&flow_sem, &ts);
}

static void flow_stop(int start_ret)
{
    if (start_ret == 0)
        sem_post(&flow_sem);
}

bool_t skye_rpc_init_1_svc(bool_t *result, struct svc_req *rqstp)
{
    (void)rqstp;
    assert(result);

    dbg_msg(log_fp, "[%s] recv:init()", __func__);

    *result = true;

    return true;
}

bool_t skye_rpc_lookup_1_svc(PVFS_credentials creds, PVFS_object_ref parent,
                             skye_pathname path, skye_lookup *result, 
                             struct svc_req *rqstp)
{
    (void)rqstp;

    int flow = flow_start();
    struct skye_directory *dir = cache_fetch(&parent);
    if (!dir){
        dbg_msg(stderr, "[%s] Unable to fetch %lu from cache", __func__, parent.handle);
        result->errnum = -EIO;
        return true;
    }

    int index, server;
    index = giga_get_index_for_file(&dir->mapping, (const char*)path);
    server = giga_get_server_for_index(&dir->mapping, index);

    if (server != skye_options.servernum){
        result->errnum = -EAGAIN;
        memcpy(&(result->skye_lookup_u.bitmap), &dir->mapping, sizeof(dir->mapping));
        goto exit;
    }

    PVFS_handle old_parent = parent.handle;
    parent.handle = get_partition_handle(&creds, dir, index);

    PVFS_sysresp_lookup lk_response;
    int ret;

    memset(&lk_response, 0, sizeof(lk_response));
    ret = PVFS_sys_ref_lookup(pvfs_fsid, path, parent,
                              &creds, &lk_response, PVFS2_LOOKUP_LINK_NO_FOLLOW,
                              PVFS_HINT_NULL);


    result->errnum = -1 * PVFS_get_errno_mapping(ret);
    result->skye_lookup_u.ref = lk_response.ref;

    /* If we are currently splitting, try again in the child directory */
    // FIXME: this is vulnerable to repeated splits
    if (result->errnum == -ENOENT && index == dir->splitting_index){
        index = giga_index_for_splitting(&dir->mapping, index);
        parent.handle = old_parent;

        char physical_path[MAX_LEN];
        snprintf(physical_path, MAX_LEN, "p%05d/%s", index, (const char*)path);

        memset(&lk_response, 0, sizeof(lk_response));
        ret = PVFS_sys_ref_lookup(pvfs_fsid, path, parent,
                                  &creds, &lk_response, PVFS2_LOOKUP_LINK_NO_FOLLOW,
                                  PVFS_HINT_NULL);

        result->errnum = -1 * PVFS_get_errno_mapping(ret);
        result->skye_lookup_u.ref = lk_response.ref;
    }

exit:
    cache_return(dir);
    flow_stop(flow);

    return true;;
}

/* FIXME: lots of code duplication with lookup */
bool_t skye_rpc_partition_1_svc(PVFS_credentials creds, PVFS_object_ref parent,
                             skye_pathname path, skye_lookup *result, 
                             struct svc_req *rqstp)
{
    (void)rqstp;

    int flow = flow_start();
    struct skye_directory *dir = cache_fetch(&parent);
    if (!dir){
        dbg_msg(stderr, "[%s] Unable to fetch %lu from cache", __func__, parent.handle);
        result->errnum = -EIO;
        return true;
    }

    int index, server;
    index = giga_get_index_for_file(&dir->mapping, (const char*)path);
gotindex:
    server = giga_get_server_for_index(&dir->mapping, index);

    if (server != skye_options.servernum){
        result->errnum = -EAGAIN;
        memcpy(&(result->skye_lookup_u.bitmap), &dir->mapping, sizeof(dir->mapping));
        goto exit;
    }

    char physical_path[MAX_LEN];
    snprintf(physical_path, MAX_LEN, "p%05d/%s", index, (const char*)path);

    PVFS_sysresp_lookup lk_response;
    int ret;

    // FIXME: Cache the partition!
    memset(&lk_response, 0, sizeof(lk_response));
    ret = PVFS_sys_ref_lookup(pvfs_fsid, physical_path, parent,
                              &creds, &lk_response, PVFS2_LOOKUP_LINK_NO_FOLLOW,
                              PVFS_HINT_NULL);


    if ( ret < 0 ) {
        result->errnum = -1 * PVFS_get_errno_mapping(ret);
        /* If we are currently splitting, try again in the child directory */
        if (result->errnum == -ENOENT && index == dir->splitting_index){
            index = giga_index_for_splitting(&dir->mapping, index);
            goto gotindex;
        }
        goto exit;
    }

    /* so, the file exists, now we descend just into the bucket */

    snprintf(physical_path, MAX_LEN, "p%05d", index);

    memset(&lk_response, 0, sizeof(lk_response));
    ret = PVFS_sys_ref_lookup(pvfs_fsid, physical_path, parent,
                              &creds, &lk_response, PVFS2_LOOKUP_LINK_NO_FOLLOW,
                              PVFS_HINT_NULL);


    result->errnum = -1 * PVFS_get_errno_mapping(ret);
    result->skye_lookup_u.ref = lk_response.ref;

exit:
    cache_return(dir);
    flow_stop(flow);

    return true;
}

bool_t skye_rpc_create_1_svc(PVFS_credentials creds, PVFS_object_ref parent,
                             skye_pathname filename, mode_t mode, 
                             skye_lookup *result, struct svc_req *rqstp)
{
    (void)rqstp;
    int rc;

    time_t starttime, endtime;
    starttime = time(NULL);

    int flow = flow_start();
    struct skye_directory *dir = cache_fetch(&parent);
    if (!dir){
        dbg_msg(stderr, "[%s] Unable to fetch %lu from cache", __func__, parent.handle);
        result->errnum = -EIO;
        return true;
    }

    if ((rc = enter_bucket(&creds, dir, (char*)filename, &parent, &(result->skye_lookup_u.bitmap))) < 0){
        result->errnum = rc;
        cache_return(dir);
        flow_stop(flow);
        return true;
    }

    PVFS_sysresp_create resp_create;

    /* Set attributes */
    PVFS_sys_attr attr;
    memset(&attr, 0, sizeof(PVFS_sys_attr));
    attr.owner = creds.uid;
    attr.group = creds.gid;
    attr.perms = mode;
    attr.atime = time(NULL);
    attr.mtime = attr.atime;
    attr.mask = PVFS_ATTR_SYS_ALL_SETABLE;
    attr.dfile_count = 0;

    rc = PVFS_sys_create(filename, parent, attr, &creds, NULL, &resp_create,
                             PVFS_SYS_LAYOUT_DEFAULT, PVFS_HINT_NULL);
    if (rc != 0){
        if ( rc == -PVFS_ENOENT )
            result->errnum = -EACCES;
        else
            result->errnum = -1 * PVFS_get_errno_mapping(rc);
        cache_return(dir);
        flow_stop(flow);
        return true;
    }


    // Only check to see if we need to split some of the time
    static int create_count;
    if (dir->splitting_index == -1 && (create_count++ % 100 == 0)){
        int index = giga_get_index_for_file(&dir->mapping, filename);
        if (giga_is_splittable(&dir->mapping, index) && isdir_overflow(&creds, &parent))
            perform_split(&dir->handle, index);
    }
    cache_return(dir);
    flow_stop(flow);

    result->errnum = 0;
    result->skye_lookup_u.ref = resp_create.ref;

    endtime = time(NULL);
    double elapsedtime = difftime(endtime, starttime);

    if (elapsedtime > 1.0)
        dbg_msg(log_fp, "[%s] created %lu/%s in %lf", __func__, parent.handle, filename, elapsedtime);

    return true;
}

bool_t skye_rpc_mkdir_1_svc(PVFS_credentials creds, PVFS_object_ref parent,
                            skye_pathname dirname, mode_t mode, 
                            skye_lookup *result, struct svc_req *rqstp)
{
    (void)rqstp;
    int rc;

    int flow = flow_start();
    struct skye_directory *dir = cache_fetch(&parent);
    if (!dir){
        dbg_msg(stderr, "[%s] Unable to fetch %lu from cache", __func__, parent.handle);
        result->errnum = -EIO;
        flow_stop(flow);
        return true;
    }

    if ((rc = enter_bucket(&creds, dir, (char*)dirname, &parent, &(result->skye_lookup_u.bitmap))) < 0){
        result->errnum = rc;
        cache_return(dir);
        flow_stop(flow);
        return true;
    }

    /* Set attributes */
    PVFS_sys_attr attr;
    memset(&attr, 0, sizeof(PVFS_sys_attr));
    attr.owner = creds.uid;
    attr.group = creds.gid;
    attr.perms = mode;
    attr.mask = PVFS_ATTR_SYS_ALL_SETABLE;

    PVFS_sysresp_mkdir resp_mkdir;
    rc = PVFS_sys_mkdir(dirname, parent, attr, &creds, &resp_mkdir,
                            PVFS_HINT_NULL);

    if (rc != 0){
        result->errnum = -1 * PVFS_get_errno_mapping(rc);
        cache_return(dir);
        flow_stop(flow);
        return true;
    }

    /* FIXME: handle cleanup in the case that this fails */

    parent = resp_mkdir.ref;

    int server = pvfs_get_mds(&parent);

    dirname = "p00000";
    rc = pvfs_mkdir_server(&creds, &parent, dirname, &attr, server, NULL);
    result->errnum = rc;
    result->skye_lookup_u.ref = resp_mkdir.ref;

    cache_return(dir);
    flow_stop(flow);

	return true;
}

static int remove_bucket(PVFS_credentials *creds, struct skye_directory *dir, int index)
{
    char physical_path[MAX_LEN];
    snprintf(physical_path, MAX_LEN, "p%05d", index);

    int rc = PVFS_sys_remove(physical_path, dir->handle, creds, PVFS_HINT_NULL);
    if ( rc < 0 ){
        return  -1 * PVFS_get_errno_mapping(rc);
    }

    giga_update_mapping_remove(&dir->mapping, index);

    return 0;
}

/* except the 0th bucket */
static int remove_all_buckets(PVFS_credentials *creds, struct skye_directory *dir)
{
    PVFS_sysresp_readdir rd_response;
    unsigned int pvfs_dirent_incount = 32; // reasonable chank size
    PVFS_ds_position token = 0;

    PVFS_object_ref *parent = &dir->handle;

    do {
        PVFS_dirent *cur_file = NULL;
        unsigned int i;

        memset(&rd_response, 0, sizeof(PVFS_sysresp_readdir));
        int ret = PVFS_sys_readdir(*parent, (!token ? PVFS_READDIR_START : token),
                                pvfs_dirent_incount, creds, &rd_response,
                                PVFS_HINT_NULL);
        if (ret < 0)
            return -1 * PVFS_get_errno_mapping(ret);

        // FIXME: just abort if any partition is splitting
        for (i = 0; i < rd_response.pvfs_dirent_outcount; i++) {
            cur_file = &(rd_response.dirent_array[i]);

            index_t index;
            sscanf(cur_file->d_name, "p%u", &index);

            /* we don't remove the 0th bucket until the end */
            if (index == 0)
                continue;

            int server = giga_get_server_for_index(&dir->mapping, index);

            if (server == skye_options.servernum){
                ret = remove_bucket(creds, dir, index);
                if (ret)
                    return ret;
            } else {
                CLIENT *client = get_connection(server);

                enum clnt_stat retval;
                retval = skye_rpc_bucket_remove_1(*creds, *parent, index, &ret, client);
                if (retval != RPC_SUCCESS){
                    clnt_perror(client, "RPC bucket remove failed");
                    return -EIO;
                }

                if (ret != 0)
                    return ret;
            }
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

    return 0;
}

bool_t skye_rpc_remove_1_svc(PVFS_credentials creds, PVFS_object_ref parent,
                            skye_pathname filename, skye_result *result, 
                            struct svc_req *rqstp)
{
    (void)rqstp;
    int rc;

    int flow = flow_start();
    struct skye_directory *dir = cache_fetch(&parent);
    if (!dir){
        dbg_msg(stderr, "[%s] Unable to fetch %lu from cache", __func__, parent.handle);
        result->errnum = -EIO;
        flow_stop(flow);
        return true;
    }

    if ((rc = enter_bucket(&creds, dir, (char*)filename, &parent, &(result->skye_result_u.bitmap))) < 0){
        result->errnum = rc;
        goto exit;
    }

    PVFS_sysresp_lookup lk_response;

    memset(&lk_response, 0, sizeof(lk_response));
    rc = PVFS_sys_ref_lookup(pvfs_fsid, filename, parent, &creds,
                              &lk_response, PVFS2_LOOKUP_LINK_NO_FOLLOW,
                              PVFS_HINT_NULL);
    if ( rc < 0 ){
        result->errnum =  -1 * PVFS_get_errno_mapping(rc);
        goto exit;
    }

    if (isdir(&creds, &lk_response.ref)){
        cache_return(dir);
        dir = cache_fetch_w(&lk_response.ref);
        rc = remove_all_buckets(&creds, dir);
        if ( rc < 0 ){
            result->errnum = rc;
            goto exit;
        }

        rc = PVFS_sys_remove("p00000", dir->handle, &creds, PVFS_HINT_NULL);
        if ( rc < 0 ){
            result->errnum = -1 * PVFS_get_errno_mapping(rc);
            goto exit;
        }

        /* FIXME: lock out access to the directory here */
        rc = PVFS_sys_remove(filename, parent, &creds, PVFS_HINT_NULL);
        if ( rc < 0 ){
            result->errnum = -1 * PVFS_get_errno_mapping(rc);
            /* FIXME: inconsistent state */
        } else {
            result->errnum = 0;
        }

        cache_destroy(dir);
        dir = NULL;
        flow_stop(flow);

        return true;
    }

    rc = PVFS_sys_remove(filename, parent, &creds, PVFS_HINT_NULL);
    result->errnum = -1 * PVFS_get_errno_mapping(rc);

exit:
    cache_return(dir);
    flow_stop(flow);
    
	return true;
}

/* FIXME: what if we cause the destination bucket to overflow */
bool_t skye_rpc_rename_1_svc(PVFS_credentials creds, 
                             skye_pathname src_name, PVFS_object_ref src_parent,
                             skye_pathname dst_name, PVFS_object_ref dst_parent,
                             skye_result *result,  struct svc_req *rqstp)
{
    (void)rqstp;
    int rc;

    int flow = flow_start();
    struct skye_directory *dir = cache_fetch(&dst_parent);
    if (!dir){
        dbg_msg(stderr, "[%s] Unable to fetch %lu from cache", __func__, dst_parent.handle);
        result->errnum = -EIO;
        flow_stop(flow);
        return true;
    }

    if ((rc = enter_bucket(&creds, dir, (char*)dst_name, &dst_parent, &(result->skye_result_u.bitmap))) < 0){
        result->errnum = rc;
        cache_return(dir);
        flow_stop(flow);
        return true;
    }

    rc = PVFS_sys_rename(src_name, src_parent, dst_name, dst_parent,
                             &creds, PVFS_HINT_NULL);
    result->errnum = -1 * PVFS_get_errno_mapping(rc);

    cache_return(dir);
    flow_stop(flow);
	return true;
}

bool_t skye_rpc_bucket_add_1_svc(PVFS_object_ref handle, int index, int *result,
                                 struct svc_req *rqstp) 
{
    bool_t retval = true;
    (void)result;
    (void)rqstp;

    dbg_msg(stderr, "[%s] I've now got %lu/%d", __func__, handle.handle, index);

    struct skye_directory *dir = cache_fetch_w(&handle);

    giga_update_mapping(&dir->mapping, index);

    cache_return(dir);

    return retval;
}

bool_t skye_rpc_bucket_remove_1_svc(PVFS_credentials creds, 
                                    PVFS_object_ref handle, 
                                    int index, int *result, 
                                    struct svc_req *rqstp)
{
    (void)rqstp;

    struct skye_directory *dir = cache_fetch_w(&handle);

    if (!dir) {
        *result = -ENOMEM;
        return true;
    }

    *result = remove_bucket(&creds, dir, index);

    cache_return(dir);

    return true;
}

/* TODO: What exactly am I supposed to do here? */
int skye_rpc_prog_1_freeresult (SVCXPRT *transp, xdrproc_t xdr_result, 
                                caddr_t result)
{
    (void)transp;

    xdr_free (xdr_result, result);

    /*
     * Insert additional freeing code here, if needed
     */

    return 1;
}
