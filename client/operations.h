#ifndef CLIENT_OPERATIONS_H
#define CLIENT_OPERATIONS_H

#include <fuse_lowlevel.h>

void skye_ll_readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t offset,
                     struct fuse_file_info *fi);
void skye_ll_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
void skye_ll_lookup(fuse_req_t req, fuse_ino_t parent, const char *name);
void skye_ll_open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
void skye_ll_read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t offset,
                  struct fuse_file_info *fi);
void skye_ll_create(fuse_req_t req, fuse_ino_t parent, const char *filename, 
                     mode_t mode, struct fuse_file_info *fi);
void skye_ll_mkdir(fuse_req_t req, fuse_ino_t parent, const char *filename,
               mode_t mode);
void skye_ll_opendir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
void skye_ll_releasedir (fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
#endif
