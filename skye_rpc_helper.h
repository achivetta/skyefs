#ifndef GIGA_RPC_HELPER_H
#define GIGA_RPC_HELPER_H   

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <inttypes.h>

#include "GigaIndex.h"

extern bool_t xdr_GigaMapping();
extern bool_t xdr_stat();
extern bool_t xdr_statvfs();

//bool_t xdr_stat(XDR *xdrs, struct stat *objp);
//bool_t xdr_statvfs(XDR *xdrs, struct statvfs *objp);

#endif /* GIGA_RPC_HELPER_H */
