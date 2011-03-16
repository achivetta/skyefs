#ifndef DEFAULTS_H
#define DEFAULTS_H   

/* client and server settings specific constants */
#define DEFAULT_PORT 55677
#define DEFAULT_IP "127.0.0.1"
#define DEFAULT_MOUNT "./"
#define DEFAULT_PVFS_FS "tcp://localhost:3334/pvfs2-fs"

/* The following excerpt is from the /usr/include/fuse/fuse.h
 *
 * IMPORTANT: you should define FUSE_USE_VERSION before including this
 * header.  To use the newest API define it to 26 (recommended for any
 * new application), to use the old API define it to 21 (default) 22
 * or 25, to use the even older 1.X API define it to 11.
*/
#define FUSE_USE_VERSION    26

#define FUSE_SUCCESS    0

/* string and buffer lengths */
#define MAX_FILENAME_LEN    256
#define MAX_PATHNAME_LEN    4096
#define MAX_HOSTNAME_LEN    64

#define MAX_DBG_STR_LEN     128

#define MAX_IP_ADDR_LEN     24

#define MAX_LEN     512     /* for those needing "length" (e.g., path name, ip) */
#define MAX_SIZE    4096    /* for those needing "buffer" (e.g., read/write) */

/* File/Directory permission bits */
#define DEFAULT_MODE    (S_IRWXU | S_IRWXG | S_IRWXO )

#define USER_RW         (S_IRUSR | S_IWUSR)
#define GRP_RW          (S_IRGRP | S_IWGRP)
#define OTHER_RW        (S_IROTH | S_IWOTH)

#define CREATE_MODE     (USER_RW | GRP_RW | OTHER_RW)
#define CREATE_FLAGS    (O_CREAT | O_APPEND)
#define CREATE_RDEV     0

#endif /* DEFAULTS_H */
