#include "common/defaults.h"
#include "client.h"
#include "operations.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <fuse.h>
#include <fuse_opt.h>

struct client_options client_options;

/** macro to define options */
#define SKYE_OPT_KEY(t, p, v) { t, offsetof(struct client_options, p), v }

static struct fuse_opt skye_opts[] = {
    SKYE_OPT_KEY("host=%s", host, 0),
    SKYE_OPT_KEY("port=%s", port, 0),

    FUSE_OPT_END
};

/** This tells FUSE how to do every operation */
static struct fuse_operations skye_oper = {
    .init      = skye_init,
    .readdir   = skye_readdir,
    .destroy   = skye_destroy,
    .getattr   = skye_getattr
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

    /** free arguments */
    fuse_opt_free_args(&args);

    return ret;
}
