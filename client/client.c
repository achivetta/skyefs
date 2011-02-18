#include "common/defaults.h"
#include "common/skye_rpc.h"
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

struct client_options client_options;
struct PVFS_sys_mntent pvfs_mntent;
PVFS_fs_id pvfs_fsid;
CLIENT *rpc_client;

static int pvfs_connect();
static int rpc_connect();
static void rpc_disconnect();
static void* skye_init(struct fuse_conn_info *conn);
static void skye_destroy(void *);

/** macro to define options */
#define SKYE_OPT_KEY(t, p, v) { t, offsetof(struct client_options, p), v }

static struct fuse_opt skye_opts[] = {
    SKYE_OPT_KEY("host=%s", host, 0),
    SKYE_OPT_KEY("port=%s", port, 0),
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
};

int main(int argc, char *argv[])
{
    int ret;
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

    /* clear structure that holds our options */
    memset(&client_options, 0, sizeof(struct client_options));

    if (fuse_opt_parse(&args, &client_options, skye_opts, NULL) == -1)
        /** error parsing options */
        return -1;

    if (!client_options.host) client_options.host = DEFAULT_IP;
    if (!client_options.port) client_options.port = DEFAULT_PORT;

    ret = fuse_main(args.argc, args.argv, &skye_oper, NULL);

    fuse_opt_free_args(&args);

    return ret;
}

static void* skye_init(struct fuse_conn_info *conn) 
{
    (void)conn;
    pvfs_connect();
    rpc_connect();
    return NULL;
}

static void skye_destroy(void * unused)
{
    (void)unused;
    rpc_disconnect();
}

static int pvfs_connect()
{
    int ret = 0;
    struct PVFS_sys_mntent *me = &pvfs_mntent;
    char *cp;
    int cur_server;

    /* the following is copied from PVFS_util_init_defaults()
       in fuse/lib/pvfs2-util.c */

    /* initialize pvfs system interface */
    ret = PVFS_sys_initialize(GOSSIP_NO_DEBUG);
    if (ret < 0)
    {
        return(ret);
    }

    /* the following is copied from PVFS_util_parse_pvfstab()
       in fuse/lib/pvfs2-util.c */
    memset( me, 0, sizeof(pvfs_mntent) );

    /* Enable integrity checks by default */
    me->integrity_check = 1;
    /* comma-separated list of ways to contact a config server */
    me->num_pvfs_config_servers = 1;

    for (cp=client_options.pvfs_spec; *cp; cp++)
        if (*cp == ',')
            ++me->num_pvfs_config_servers;

    /* allocate room for our copies of the strings */
    me->pvfs_config_servers =
        malloc(me->num_pvfs_config_servers *
               sizeof(*me->pvfs_config_servers));
    if (!me->pvfs_config_servers)
        exit(-1);
    memset(me->pvfs_config_servers, 0,
           me->num_pvfs_config_servers * sizeof(*me->pvfs_config_servers));

    me->mnt_dir = NULL;
    me->mnt_opts = NULL;

    cp = client_options.pvfs_spec;
    cur_server = 0;
    for (;;) {
        char *tok;
        int slashcount;
        char *slash;
        char *last_slash;

        tok = strsep(&cp, ",");
        if (!tok) break;

        slash = tok;
        slashcount = 0;
        while ((slash = index(slash, '/')))
        {
            slash++;
            slashcount++;
        }
        if (slashcount != 3)
        {
            fprintf(stderr,"Error: invalid FS spec: %s\n",
                    client_options.pvfs_spec);
            exit(-1);
        }

        /* find a reference point in the string */
        last_slash = rindex(tok, '/');
        *last_slash = '\0';

        /* config server and fs name are a special case, take one 
         * string and split it in half on "/" delimiter
         */
        me->pvfs_config_servers[cur_server] = strdup(tok);
        if (!me->pvfs_config_servers[cur_server])
            exit(-1);

        ++last_slash;

        if (cur_server == 0) {
            me->pvfs_fs_name = strdup(last_slash);
            if (!me->pvfs_fs_name)
                exit(-1);
        } else {
            if (strcmp(last_slash, me->pvfs_fs_name) != 0) {
                fprintf(stderr,
                        "Error: different fs names in server addresses: %s\n",
                        client_options.pvfs_spec);
                exit(-1);
            }
        }
        ++cur_server;
    }

    /* FIXME flowproto should be an option */
    me->flowproto = FLOWPROTO_DEFAULT;

    /* FIXME encoding should be an option */
    me->encoding = ENCODING_DEFAULT;

    /* FIXME default_num_dfiles should be an option */

    ret = PVFS_sys_fs_add(me);
    if( ret < 0 )
    {
        PVFS_perror("Could not add mnt entry", ret);
        return(-1);
    }
    pvfs_fsid = me->fs_id;

    return ret;
}

static int rpc_connect()
{
    int sock = RPC_ANYSOCK;

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(client_options.port);
    if (inet_pton(AF_INET, client_options.host, &addr.sin_addr) < 0) {
        fprintf(stderr, "ERROR: handling source IP (%s). %s \n",
                DEFAULT_IP, strerror(errno));
        exit(EXIT_FAILURE);
    } 

    rpc_client = clnttcp_create (&addr, SKYE_RPC_PROG, SKYE_RPC_VERSION, &sock, 0, 0);
    if (rpc_client == NULL) {
        clnt_pcreateerror (NULL);
        exit (EXIT_FAILURE);
    }

    return 0;
}

static void rpc_disconnect()
{
	clnt_destroy (rpc_client);
}
