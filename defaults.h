#ifndef DEFAULTS_H
#define DEFAULTS_H   

// client and server settings specific constants
#define DEFAULT_PORT 55677
#define DEFAULT_IP "127.0.0.1"
#define DEFAULT_MOUNT "./"

// The following excerpt is from the /usr/include/fuse/fuse.h
//
// IMPORTANT: you should define FUSE_USE_VERSION before including this
// header.  To use the newest API define it to 26 (recommended for any
// new application), to use the old API define it to 21 (default) 22
// or 25, to use the even older 1.X API define it to 11.
//
#define FUSE_USE_VERSION    26

// error messages returned to FUSE (at the client).
//
#define FUSE_SUCCESS    0
#define FUSE_ERROR      -ENOMEM
#define FUSE_ENOMEM     -ENOMEM
#define FUSE_EAGAIN     -EAGAIN
#define FUSE_RPC_ERROR  -EHOSTUNREACH
#define FUSE_ENOENT     -ENOENT
#define FUSE_EEXIST     -EEXIST

#define FUSE_READDR_REQUEST 111

// GIGA update messages from server
//
#define GIGA_UPD_NOTHING    0
#define GIGA_UPD_REROUTE    11    // re-route to correct server
#define GIGA_UPD_MAPPING    22    // update the bitmap mapping

#define SYNC_BACKUP 12345
#define ASYNC_BACKUP 67890 
//#define GIGA_BACKUP SYNC_BACKUP

// string and buffer lengths
//
#define MAX_GIGA_MSG_LEN    1024
#define MAX_FILENAME_LEN    128
#define MAX_PATHNAME_LEN    512
#define MAX_HOSTNAME_LEN    64

#define MAX_DBG_STR_LEN     128

#define MAX_IP_ADDR_LEN     24

#define MAX_LEN     512     // for those needing "length" (e.g., path name, ip)
#define MAX_SIZE    4096    // for those needing "buffer" (e.g., read/write)

// File/Directory permission bits
//
#define DEFAULT_MODE    (S_IRWXU | S_IRWXG | S_IRWXO )

#define USER_RW         (S_IRUSR | S_IWUSR)
#define GRP_RW          (S_IRGRP | S_IWGRP)
#define OTHER_RW        (S_IROTH | S_IWOTH)

#define CREATE_MODE     (USER_RW | GRP_RW | OTHER_RW)
#define CREATE_FLAGS    (O_CREAT | O_APPEND)
#define CREATE_RDEV     0

// giga+ specific constants
#define SPLIT_THRESHOLD          8000 
#define SPLIT_THRESHOLD_FOR_TEST 8

#define MAX_SPLIT_BATCH_SIZE    512
#define SPLIT_COMPLETE          123
#define SPLIT_INCOMPLETE        456

// different split policies that define when should the buckets stop
// splitting
//
#define SPLIT_T_NO_SPLITTING_EVER   20202       //XXX: not implemented
#define SPLIT_T_NO_BOUND            30303       // keep splitting 
#define SPLIT_T_NEXT_HIGHEST_POW2   40404       //XXX: not implemented
#define SPLIT_T_NUM_SERVERS_BOUND   50505       // after using all servers
#define SPLIT_T_NUM_BUCKETS_BOUND   60606       // XXX: not implemented
                                                // after N buckets/server
#define MAX_BKTS_PER_SERVER         2           // used with NUM_BUCKETS_BOUND

#define SPLIT_TYPE                  SPLIT_T_NUM_SERVERS_BOUND

// Names of extended attributes in the directory i-node
//
#define XA_ZEROTH_SRV           "zeroth_srv"            // Zeroth server
#define XA_NUM_SERVERS_EPOCH    "num_servers_epoch"     
#define XA_NUM_SERVERS_ACTIVE   "num_servers_active"        
#define XA_NUM_VIRTUAL_SERVERS  "virtual_servers"       // # of virtual servers
#define XA_SPLIT_THRESHOLD      "split_thresh"          // When to split??


#define DEFAULT_SPLIT_THRESHOLD     8000                // When to split??

//#define SPLIT_TYPE_KEEP_SPLITTING   30303       // keep splitting if overflows
//#define SPLIT_TYPE_NEVER_SPLIT      40404       // static, a priori partitioning 
//#define SPLIT_TYPE_ALL_SERVERS      50505       // 
//#define SPLIT_TYPE_POWER_OF_2       60606       // stop when you have pow-of-2

#define GIGA_VIRTUAL_SERVERS 0

#define LOCAL_SPLIT         10621
#define REMOTE_SPLIT        73073

#define MAX_NUM_SERVERS     100

#endif /* DEFAULTS_H */
