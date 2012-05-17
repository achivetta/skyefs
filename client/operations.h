/* This file is part of SkyeFS, an implementatino of Giga+ on PVFS.
 *
 * Copyright 2011-2012, Anthony Chivetta <anthony@chivetta.org>
 *
 * See COPYING for licence information.
 */
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
void skye_ll_mknod (fuse_req_t req, fuse_ino_t parent, const char *filename, 
                    mode_t mode, dev_t rdev);
void skye_ll_write (fuse_req_t req, fuse_ino_t ino, const char *buf,
                    size_t size, off_t offset, struct fuse_file_info *fi);
void skye_ll_rename (fuse_req_t req, fuse_ino_t src_parent, const char *src_name,
                     fuse_ino_t dst_parent, const char *dst_name);
void skye_ll_unlink(fuse_req_t req, fuse_ino_t parent, const char *name);
void skye_ll_rmdir(fuse_req_t req, fuse_ino_t parent, const char *name);
void skye_ll_setattr(fuse_req_t req, fuse_ino_t ino, struct stat *attr,
                     int to_set, struct fuse_file_info *fi);
#endif
