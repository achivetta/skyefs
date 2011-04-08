#include "common/defaults.h"
#include "common/skye_rpc.h"
#include "common/trace.h"
#include "common/connection.h"
#include "common/options.h"
#include "client.h"
#include "operations.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <fuse.h>
#include <fuse_opt.h>
#include <pvfs2-sysint.h>
#include <pvfs2-util.h>
#include <rpc/rpc.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/types.h>

struct skye_options skye_options;

static void* skye_init(struct fuse_conn_info *conn);
static void skye_destroy(void *);

/** macro to define options */
#define SKYE_OPT_KEY(t, p, v) { t, offsetof(struct skye_options, p), v }

static struct fuse_opt skye_opts[] = {
    SKYE_OPT_KEY("pvfs=%s", pvfs_spec, 0),

    FUSE_OPT_END
};

/** This tells FUSE how to do every operation */
static struct fuse_operations skye_oper = {
    .init      = skye_init,
    .destroy   = skye_destroy,
    .getattr   = skye_getattr,
    .mkdir     = skye_mkdir,
    .create    = skye_create,
    .readdir   = skye_readdir,
    .rename    = skye_rename,
    .unlink    = skye_unlink,
    .rmdir     = skye_rmdir,
    .chmod     = skye_chmod,
    .chown     = skye_chown,
    .truncate  = skye_truncate,
    .utime     = skye_utime,
    .write     = skye_write,
    .read      = skye_read,
    .open      = skye_open
};

int main(int argc, char *argv[])
{
    int ret;
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

    /* clear structure that holds our options */
    memset(&skye_options, 0, sizeof(struct skye_options));

    if (fuse_opt_parse(&args, &skye_options, skye_opts, NULL) == -1)
        /** error parsing options */
        return -1;

    fuse_opt_insert_arg(&args, 1, "-odirect_io");
    fuse_opt_insert_arg(&args, 1, "-oattr_timeout=0");
    fuse_opt_insert_arg(&args, 1, "-omax_write=524288");
    if ( getpid() == 0 )
        fuse_opt_insert_arg( &args, 1, "-oallow_other" );
    fuse_opt_insert_arg(&args, 1, "-s");

    ret = fuse_main(args.argc, args.argv, &skye_oper, NULL);

    fuse_opt_free_args(&args);

    return ret;
}

static void* skye_init(struct fuse_conn_info *conn) 
{
    int ret;
    (void)conn;
    if ((ret = pvfs_connect(skye_options.pvfs_spec)) < 0){
        err_quit("Unable to connect to PVFS (%d). Quitting.\n", ret);
    }
    if ((ret = rpc_connect()) < 0){
        err_quit("Unable to establish RPC connections (%d). Quitting.\n", ret);
    }

    return NULL;
}

static void skye_destroy(void * unused)
{
    (void)unused;
    rpc_disconnect();
}

