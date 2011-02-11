#ifndef CLIENT_OPERATIONS_H
#define CLIENT_OPERATIONS_H

#include <fuse.h>

int skye_readdir(const char * path, void * buf, fuse_fill_dir_t filler, off_t offset,
            struct fuse_file_info *fi);
int skye_getattr(const char *path, struct stat *stbuf);
int skye_create(const char *path, mode_t, struct fuse_file_info *);
int skye_mkdir(const char * path, mode_t mode);
int skye_rename(const char *src_path, const char *dst_path);

#endif
