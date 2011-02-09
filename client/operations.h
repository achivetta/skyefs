#ifndef CLIENT_OPERATIONS_H
#define CLIENT_OPERATIONS_H

#include <fuse.h>

int skye_readdir(const char * path, void * buf, fuse_fill_dir_t filler, off_t offset,
            struct fuse_file_info *fi);
int skye_getattr(const char *path, struct stat *stbuf);

#endif
