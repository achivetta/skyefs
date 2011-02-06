#ifndef GIGA_RPC_HELPER_H
#define GIGA_RPC_HELPER_H   

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <inttypes.h>

bool_t xdr_PVFS_object_ref(XDR *xdrs, PVFS_object_ref *objp);

#endif /* GIGA_RPC_HELPER_H */
