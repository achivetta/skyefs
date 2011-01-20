#include "server.h"
#include "trace.h"
#include "defaults.h"
#include "skye_rpc.h"

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
        printf("usage: %s -p <port_number> -M <mount_point>\n",argv[0]);
        exit(EXIT_FAILURE);
    }

    // set STDERR non-buffering 
    setbuf(stderr, NULL);
    log_fp = stderr;

    srv_settings.mount_point = DEFAULT_MOUNT;
    srv_settings.port_num = DEFAULT_PORT;

    char c;
    while (-1 != (c = getopt(argc, argv,
                             "p:"           // port number
                             "M:"           // mount point for server "root"
           ))) {
        switch(c) {
            case 'p':
                srv_settings.port_num = atoi(optarg);
                break;
            case 'M':
                srv_settings.mount_point = 
                    (char*)malloc(sizeof(char)*MAX_PATHNAME_LEN);
                if (srv_settings.mount_point == NULL)
                    err_sys("[%s] ERROR: malloc() mount_point.", __func__);
                strcpy(srv_settings.mount_point, optarg);
                break;
            default:
                fprintf(stdout, "Illegal parameter: %c\n", c);
                exit(1);
                break;
        }

    }

    // handling SIGINT
    signal(SIGINT, sig_handler);

    if (srv_settings.mount_point && (chdir(srv_settings.mount_point)) < 0){
        fprintf(stdout, "Cannot chdir(%s): %s",srv_settings.mount_point, strerror(errno));
        exit(EXIT_FAILURE);
    }

    server_socket(); 

    exit(1);
}
