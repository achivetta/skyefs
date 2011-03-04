#ifndef CLIENT_OPERATIONS_H
#define CLIENT_OPERATIONS_H

#include <fuse.h>

int skye_readdir(const char * path, void * buf, fuse_fill_dir_t filler, off_t offset,
            struct fuse_file_info *fi);
int skye_getattr(const char *path, struct stat *stbuf);
int skye_create(const char *path, mode_t, struct fuse_file_info *);
int skye_mkdir(const char * path, mode_t mode);
int skye_rename(const char *src_path, const char *dst_path);
int skye_unlink(const char *path);
int skye_rmdir(const char *path);
int skye_chmod(const char *path, mode_t mode);
int skye_chown(const char *path, uid_t uid, gid_t gid);
int skye_truncate(const char *path, off_t offset);
int skye_utime(const char *path, struct utimbuf *time);


int skye_write(const char* path, const char *buf, size_t size, off_t offset, 
              struct fuse_file_info *fi);
int skye_read(const char* path, char *buf, size_t size, off_t offset, 
              struct fuse_file_info *fi);
int skye_open(const char *path, struct fuse_file_info *fi);

#endif
