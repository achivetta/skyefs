#include "defaults.h"
#include "options.h"
#include "skye_rpc.h"
#include "connection.h"
#include "trace.h"

#include <pvfs2-mgmt.h>
#include <rpc/rpc.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fuse/fuse.h>
#include <unistd.h>
#include <string.h>

static CLIENT **rpc_clients;
struct PVFS_sys_mntent pvfs_mntent;
PVFS_fs_id pvfs_fsid;

char *myhostname = NULL;

static int pvfs_generate_serverlist(){
    int ret, servercount, i;
    PVFS_credentials credentials; PVFS_util_gen_credentials(&credentials);

    // What hostname are we using
    if (!myhostname)
        myhostname = getenv("HOSTNAME");
    if (!myhostname){
        myhostname = malloc(HOST_NAME_MAX + 1);
        gethostname(myhostname, HOST_NAME_MAX);
        myhostname[HOST_NAME_MAX] = '\0';
    }

    skye_options.servernum = -1;

    ret = PVFS_mgmt_count_servers(pvfs_fsid,&credentials,PVFS_MGMT_META_SERVER,&servercount);
    if (ret < 0) return ret;

    skye_options.serveraddrs = malloc(sizeof(PVFS_BMI_addr_t)*(servercount));
    if (!skye_options.serveraddrs) return -ENOMEM;

    ret = PVFS_mgmt_get_server_array(pvfs_fsid,&credentials,PVFS_MGMT_META_SERVER, skye_options.serveraddrs, &servercount);
    if (ret < 0) return ret;

    const char **servers = malloc(sizeof(char*)*(servercount));
    if (!servers) return -ENOMEM;

    for (i = 0; i < servercount; i++){
        servers[i] = PVFS_mgmt_map_addr(pvfs_fsid,&credentials,skye_options.serveraddrs[i],NULL);    

        char *start, *end;

        /* cut out just the server portion of tcp://servername:port */
        end = rindex(servers[i], ':');
        start = rindex(servers[i], '/');

        servers[i] = strndup(start + 1, end - start - 1);

        if (!servers[i]) return -ENOMEM; 
        
        if (strcmp(servers[i],myhostname) == 0)
            skye_options.servernum = i;
    }

    skye_options.serverlist = servers;
    skye_options.servercount = servercount;

    return 0;
}

CLIENT *get_connection(int serverid)
{
    assert(serverid >= 0 && serverid < skye_options.servercount);

    return rpc_clients[serverid];
}

static int rpc_host_connect(CLIENT **rpc_client, const char *host)
{
    int sock = RPC_ANYSOCK;

    struct hostent *he;
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(DEFAULT_PORT);

    he = gethostbyname(host);
    if (!he){
        err_ret("unable to resolve %s\n", host);
        return -1;
    }
    memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);

    *rpc_client = clnttcp_create (&addr, SKYE_RPC_PROG, SKYE_RPC_VERSION, &sock, 0, 0);
    if (*rpc_client == NULL) {
        clnt_pcreateerror (NULL);
        return -1;
    }

    return 0;
}

int rpc_connect(void)
{
    int i;

    rpc_clients = malloc(sizeof(CLIENT *)*skye_options.servercount);
    if (!rpc_clients)
        return -ENOMEM;

    for (i = 0; i < skye_options.servercount; i++){
        int ret = rpc_host_connect(&rpc_clients[i], skye_options.serverlist[i]);
        if (ret < 0)
            return ret;
    }
    
    return 0;
}

void rpc_disconnect(void)
{
    int i;

    for (i = 0; i < skye_options.servercount; i++){
        clnt_destroy (rpc_clients[i]);
    }
}

int pvfs_connect(char *fs_spec)
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
    me->encoding = PVFS2_ENCODING_DEFAULT;

    /* FIXME default_num_dfiles should be an option */

    ret = PVFS_sys_fs_add(me);
    if( ret < 0 )
    {
        PVFS_perror("Could not add mnt entry", ret);
        return(-1);
    }
    pvfs_fsid = me->fs_id;

    ret = pvfs_generate_serverlist();

    /* XXX: Turn off attribute caches, they screw with us.  In the future, we
     * might want to investigate invalidating them only when needed. */
    PVFS_sys_set_info(PVFS_SYS_NCACHE_TIMEOUT_MSECS, 0);
    PVFS_sys_set_info(PVFS_SYS_ACACHE_TIMEOUT_MSECS, 0);

    return ret;
}

int pvfs_get_mds(PVFS_object_ref *ref){
    PVFS_error rc;
    PVFS_BMI_addr_t addr;
    int i;

    rc = PVFS_mgmt_map_handle(ref->fs_id, ref->handle, &addr);
    if (rc)
        return rc;

    for (i = 0; i < skye_options.servercount; i++){
        if (memcmp(&addr, skye_options.serveraddrs+i, sizeof(PVFS_BMI_addr_t)) == 0)
            break;
    }

    assert(i != skye_options.servercount);
    return i;
}

