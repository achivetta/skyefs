#ifndef GIGA_RPC_HELPER_H
#define GIGA_RPC_HELPER_H   

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <inttypes.h>

bool_t xdr_stat(XDR *xdrs, struct stat *objp);

#endif /* GIGA_RPC_HELPER_H */
