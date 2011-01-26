#ifndef CLIENT_OPERATIONS_H
#define CLIENT_OPERATIONS_H

#include <fuse.h>

void* skye_init(struct fuse_conn_info *conn);
int skye_readdir(const char * path, void * buf, fuse_fill_dir_t filler, off_t offset,
            struct fuse_file_info *fi);
void skye_destroy(void * ptr);
int skye_getattr(char *path, struct stat *stbuf);

#endif
