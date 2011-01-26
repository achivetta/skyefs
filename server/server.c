#include "server.h"
#include "common/trace.h"
#include "common/defaults.h"
#include "common/skye_rpc.h"

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <rpc/clnt.h>
#include <rpc/pmap_clnt.h>
#include <rpc/rpc.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>

#include <pvfs2.h>
#include <pvfs2-sysint.h>
#include <pvfs2-debug.h>

struct server_settings srv_settings;

// FIXME: rpcgen should put this in giga_rpc.h, but it doesn't. Why?
extern void skye_rpc_prog_1(struct svc_req *rqstp, register SVCXPRT *transp);

// Methods to handle requests from client connections
static void * handler_thread(void *arg);

// Methods to setup server's socket connections
static void server_socket();
static void setup_listener(int listen_fd);
static void main_select_loop(const int listen_fd);

//FIXME: becomes dynamically allocated based on rlimit
static uint32_t conn_fd_table[FD_SETSIZE]; 

static void sig_handler(const int sig)
{
    printf("SIGINT handled.\n");
    exit(1);
}

static void * handler_thread(void *arg)
{
    dbg_msg(log_fp, "[%s] Start thread handler.", __func__);
    
    int fd = (int) (long) arg;
    SVCXPRT *svc = svcfd_create(fd, 0, 0);
    uint32_t start_gen = conn_fd_table[fd];
    
    if(!svc_register(svc, SKYE_RPC_PROG, SKYE_RPC_VERSION, skye_rpc_prog_1, 0)) {
        err_sys("ERROR: svc_register() error.\n");
        svc_destroy(svc);
        return 0;
    }
    
    dbg_msg(log_fp, "[%s] Enter RPC select().", __func__);
    while (1) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);

        svc_getreqset(&fds);

        //FIXME: need to do something to check if the socket is closed??
        if (conn_fd_table[fd] != start_gen) {
            dbg_msg(log_fp, "[%s] Leave RPC select()\n", __func__);
            break;
        }

        // Check if the socket is closed
        struct sockaddr_in sin;
        socklen_t sin_len = sizeof(sin);
        if (getpeername(fd, (struct sockaddr *) &sin, &sin_len) < 0) {
            dbg_msg(log_fp, "[%s] Leave RPC select() (getpeername)", __func__);
            break;
        }

    }

    dbg_msg(log_fp, "[%s] Stop thread handler.", __func__);

    return 0;
}

static void main_select_loop(const int listen_fd)
{
    int conn_fd;
    
    dbg_msg(log_fp, "[%s] Starting select().", __func__);
    
    while (1) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(listen_fd, &fds);

        int i = select(listen_fd+1, &fds, 0, 0, 0);
        if (i <= 0) {
            err_msg("ERROR: something weird with select().");
            continue;
        }

        struct sockaddr_in remote_addr;
        socklen_t len = sizeof(remote_addr);
        conn_fd = accept(listen_fd, (struct sockaddr *) &remote_addr, &len);
        if (conn_fd < 0) {
            //FIXME: how to handle this error? close(listenfd)? exit(?)
            //close(listen_fd); 
            err_sys("ERROR: during accept(). %s\n",strerror(errno));
        }
        dbg_msg(log_fp, "[%s] connection accept()ed from {%s:%d}.", __func__, 
               inet_ntoa(remote_addr.sin_addr),ntohs(remote_addr.sin_port));
        
        conn_fd_table[conn_fd]++;
        pthread_t tid;
        if (pthread_create(&tid, 0, handler_thread, (void *)(unsigned long long)conn_fd) < 0) {
            err_msg("ERROR: during pthread_create().\n");
            close(conn_fd);
            //FIXME: should we exit with an error???
        } else {
            // Thread detach kicks in if you are running the client and server
            // on the localhost (127.0.0.1). 
            dbg_msg(log_fp, "[%s] detaching the handler thread.", __func__);
            pthread_detach(tid);
        }
    }
    
    dbg_msg(log_fp, "[%s] WARNING: Exiting select(). WHY??? HOW???", __func__);
}

static void setup_listener(int listen_fd)
{
    dbg_msg(log_fp, "[%s] Listener setup.", __func__);

    struct sockaddr_in serv_addr;
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(srv_settings.port_num);
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
   
    // bind() the socket to the appropriate ip:port combination
    if (bind(listen_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        close(listen_fd);
        err_sys("ERROR: bind() failed.");
    }
   
    // listen() for incoming connections
    if (listen(listen_fd, NUM_BACKLOG_CONN) < 0) {
        close(listen_fd);
        err_sys("ERROR: while listen()ing.");
    }
    
    dbg_msg(log_fp, "[%s] Listener setup (on port %d of %s). Success.",
            __func__, ntohs(serv_addr.sin_port), inet_ntoa(serv_addr.sin_addr));

    return;
}

/** Set socket options for server use.
 *
 * FIXME: Document these options
 */
static void set_sockopt_server(int sock_fd)
{
    int flags;
   
    if ((flags = fcntl(sock_fd, F_GETFL, 0)) < 0) {
        close(sock_fd);
        err_sys("ERROR: fcntl(F_GETFL) failed.");
    }
    if (fcntl(sock_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        close(sock_fd);
        err_sys("ERROR: fcntl(F_SETFL) failed.");
    }

    flags = 1;
    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, 
                   (void *)&flags, sizeof(flags)) < 0) {
        err_ret("ERROR: setsockopt(SO_REUSEADDR).");
    }
    if (setsockopt(sock_fd, SOL_SOCKET, SO_KEEPALIVE, 
                   (void *)&flags, sizeof(flags)) < 0) {
        err_ret("ERROR: setsockopt(SO_KEEPALIVE).");
    }
    if (setsockopt(sock_fd, SOL_SOCKET, SO_LINGER, 
                   (void *)&flags, sizeof(flags)) < 0) {
        err_ret("ERROR: setsockopt(SO_LINGER).");
    }
    if (setsockopt(sock_fd, IPPROTO_TCP, TCP_NODELAY, 
                   (void *)&flags, sizeof(flags)) < 0) {
        err_ret("ERROR: setsockopt(TCP_NODELAY).");
    }

    return;
}

/* from pvfs2fuse.c:main */
static int pvfs_connection(char *fs_spec){
    int ret = 0;
    struct PVFS_sys_mntent *me = &srv_settings.mntent;
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
    memset( me, 0, sizeof(srv_settings.mntent) );

    /* Enable integrity checks by default */
    me->integrity_check = 1;
    /* comma-separated list of ways to contact a config server */
    me->num_pvfs_config_servers = 1;

    for (cp=fs_spec; *cp; cp++)
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

    cp = fs_spec;
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
                    fs_spec);
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
                        fs_spec);
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
    srv_settings.fs_id = me->fs_id;

    return ret;

}

static void server_socket()
{
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) 
        err_sys("ERROR: socket() creation failed.");

    set_sockopt_server(listen_fd);
    setup_listener(listen_fd);

    main_select_loop(listen_fd);
}

int main(int argc, char **argv)
{
    if (argc == 2) {
        printf("usage: %s -p <port_number> -f <pvfs_server>\n",argv[0]);
        exit(EXIT_FAILURE);
    }

    // set STDERR non-buffering 
    setbuf(stderr, NULL);
    log_fp = stderr;

    char * fs_spec = NULL;
    srv_settings.port_num = DEFAULT_PORT;

    char c;
    while (-1 != (c = getopt(argc, argv,
                             "p:"           // port number
                             "f:"           // mount point for server "root"
           ))) {
        switch(c) {
            case 'p':
                srv_settings.port_num = atoi(optarg);
                break;
            case 'f':
                fs_spec = strdup(optarg);
                break;
            default:
                fprintf(stdout, "Illegal parameter: %c\n", c);
                exit(1);
                break;
        }

    }

    if (!(fs_spec || (fs_spec = strdup(DEFAULT_PVFS_FS) )))
        err_sys("[%s] ERROR: malloc() mount_point.", __func__);

    // handling SIGINT
    signal(SIGINT, sig_handler);

    pvfs_connection(fs_spec);

    server_socket(); 

    exit(1);
}
