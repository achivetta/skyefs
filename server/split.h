/* This file is part of SkyeFS, an implementatino of Giga+ on PVFS.
 *
 * Copyright 2011-2012, Anthony Chivetta <anthony@chivetta.org>
 *
 * See COPYING for licence information.
 */
#ifndef SPLIT_H
#define SPLIT_H

void * split_thread(void *);
void perform_split(PVFS_object_ref *parent, int pindex);
int isdir_overflow(PVFS_credentials *creds, PVFS_object_ref *handle);
int pvfs_mkdir_server(PVFS_credentials *creds, PVFS_object_ref *parent, char
                      *dirname, PVFS_sys_attr *attr, int server, PVFS_object_ref
                      *child);


#endif
