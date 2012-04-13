#include "server.h"
#include "split.h"
#include "cache.h"
#include "common/trace.h"
#include "common/defaults.h"
#include "common/skye_rpc.h"
#include "common/connection.h"
#include "common/options.h"

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

#include <semaphore.h>

#include <pvfs2.h>
#include <pvfs2-sysint.h>
#include <pvfs2-debug.h>

extern SVCXPRT *svcfd_create (int __sock, u_int __sendsize, u_int __recvsize);

struct server_settings srv_settings;
struct skye_options skye_options;

static pthread_t listen_tid, split_tid;

static sem_t flow_sem;

// FIXME: rpcgen should put this in giga_rpc.h, but it doesn't. Why?
extern void skye_rpc_prog_1(struct svc_req *rqstp, register SVCXPRT *transp);

// Methods to handle requests from client connections
static void * handler_thread(void *arg);

// Methods to setup server's socket connections
static void server_socket();
static void setup_listener(int listen_fd);
static void * main_select_loop(void * listen_fd);

static void sig_handler(const int sig)
{
    (void)sig;
    printf("SIGINT handled.\n");
    exit(1);
}

static void * handler_thread(void *arg)
{
    int fd = (int) (long) arg;
    SVCXPRT *svc = svcfd_create(fd, 0, 0);
    
    if(!svc_register(svc, SKYE_RPC_PROG, SKYE_RPC_VERSION, skye_rpc_prog_1, 0)) {
        err_msg("ERROR: svc_register() error.\n");
        svc_destroy(svc);
        goto leave;
    }
    
    while (1) {
        fd_set readfds, exceptfds;
        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);

        FD_ZERO(&exceptfds);
        FD_SET(fd, &exceptfds);

        if (select(fd + 1, &readfds, NULL, &exceptfds, NULL) < 0){
            err_msg("[%s] Error during select() on a client socket. %s", __func__, strerror(errno));
            break;
        }

        if (FD_ISSET(fd, &exceptfds)){
            dbg_msg(log_fp, "[%s] Leave RPC select(), descripter registered an exception.\n", __func__);
            break;
        }

        if (FD_ISSET(fd, &readfds)){
            // poor man's flow control
            
            struct timespec ts;
            gettimeofday((struct timeval*)&ts,NULL);
            ts.tv_sec += 2;
            ts.tv_nsec *= 1000;

            int semret = sem_timedwait(&flow_sem, &ts);
            svc_getreqset(&readfds);
            if (semret == 0)
                sem_post(&flow_sem);
        }
    }

leave:
    close(fd);

    dbg_msg(log_fp, "[%s] Connection closed.", __func__);

    return 0;
}

static void * main_select_loop(void * listen_fd_arg)
{
    int conn_fd;
    long listen_fd = (long) listen_fd_arg;
    
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
            err_sys("ERROR: during accept(). %s\n",strerror(errno));
            continue;
        }
        dbg_msg(log_fp, "[%s] connection accept()ed from {%s:%d}.", __func__, 
               inet_ntoa(remote_addr.sin_addr),ntohs(remote_addr.sin_port));
        
        pthread_t tid;
        if (pthread_create(&tid, 0, handler_thread, (void *)(unsigned long long)conn_fd) < 0) {
            err_msg("ERROR: during pthread_create().\n");
            close(conn_fd);
            continue;
        } 

        if (pthread_detach(tid) < 0){
            err_msg("ERROR: unable to detach thread().\n");
        }
    }
    
    dbg_msg(log_fp, "[%s] WARNING: Exiting select(). WHY??? HOW???", __func__);

    return NULL;
}

static void setup_listener(int listen_fd)
{
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

    pthread_create(&listen_tid, NULL, main_select_loop, (void*)(long)listen_fd);
    
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
    /* FIXME
    if (setsockopt(sock_fd, SOL_SOCKET, SO_LINGER, 
                   (void *)&flags, sizeof(flags)) < 0) {
        err_ret("ERROR: setsockopt(SO_LINGER).");
    }
    */
    if (setsockopt(sock_fd, IPPROTO_TCP, TCP_NODELAY, 
                   (void *)&flags, sizeof(flags)) < 0) {
        err_ret("ERROR: setsockopt(TCP_NODELAY).");
    }

    return;
}

static void server_socket()
{
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) 
        err_sys("ERROR: socket() creation failed.");

    set_sockopt_server(listen_fd);
    setup_listener(listen_fd);
}

static void create_root_partition(void)
{
    PVFS_credentials creds; PVFS_util_gen_credentials(&creds);

    PVFS_sysresp_lookup lk_response;
    int ret;

    memset(&lk_response, 0, sizeof(lk_response));

    ret = PVFS_sys_lookup(pvfs_fsid, (char *)"/p00000", &creds, &lk_response,
                          PVFS2_LOOKUP_LINK_NO_FOLLOW, PVFS_HINT_NULL);
    if ( ret == 0 )
        return;
    if ( PVFS_get_errno_mapping(ret) != ENOENT )
        err_quit("[%s] Unable to confirm existance of /p00000, error %d", __func__, PVFS_get_errno_mapping(ret));

    ret = PVFS_sys_lookup(pvfs_fsid, (char *)"/", &creds, &lk_response,
                          PVFS2_LOOKUP_LINK_NO_FOLLOW, PVFS_HINT_NULL);
    if (ret)
        err_quit("[%s] Unable to get root handle, error %d", __func__, PVFS_get_errno_mapping(ret));

    /* FIXME */
    PVFS_sys_attr attr;
    memset(&attr, 0, sizeof(PVFS_sys_attr));
    attr.owner = creds.uid;
    attr.group = creds.gid;
    attr.perms = 0777;
    attr.mask = PVFS_ATTR_SYS_ALL_SETABLE;

    ret = pvfs_mkdir_server(&creds, &lk_response.ref, "p00000", &attr, pvfs_get_mds(&lk_response.ref), NULL);

    if (ret)
        err_quit("[%s] Unable to root's zeroth partition, error %d", __func__, PVFS_get_errno_mapping(ret));

    return;
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

    cache_init();
    sem_init(&flow_sem, 0, 32);

    pvfs_connect(fs_spec);

    if (skye_options.servercount == 1)
        skye_options.servernum = 0;
    if (skye_options.servernum == -1){
        err_quit("ERROR: server hostname does not match any server in PVFS server list.");
    }


    if (skye_options.servernum == 0)
        create_root_partition();

    server_socket(); 

    if (pthread_create(&split_tid, NULL, split_thread, NULL) < 0){
        err_dump("ERROR: during pthread_create() of split thread.");
    } 
    if (pthread_detach(split_tid) < 0){
        err_msg("ERROR: unable to detach split thread().");
    }

    /* FIXME: we sleep 15 seconds here to let the other servers startup.  This
     * mechanism needs to be replaced by an intelligent reconnection system.
     */
    sleep(15);
    if (rpc_connect())
        err_quit("Unable to make RPC connections, quitting.");

    dbg_msg(stderr, "[%s] Server %d online!", __func__, skye_options.servernum);

    void *retval;

    pthread_join(listen_tid, &retval);

    exit((long)retval);
}
