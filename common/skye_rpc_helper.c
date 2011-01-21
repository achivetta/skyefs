#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>

#include "skye_rpc.h"
#include "skye_rpc_helper.h"

typedef unsigned int uint_t;
typedef unsigned long ulong_t;

bool_t xdr_stat(XDR *xdrs, struct stat *objp)
{
    if(!xdr_u_int (xdrs, (uint_t *)&objp->st_dev))
        return FALSE;
    if(!xdr_u_int (xdrs, (uint_t *)&objp->st_ino))
        return FALSE;
    if(!xdr_u_int (xdrs, (uint_t *)&objp->st_mode))
        return FALSE;
    if(!xdr_u_int (xdrs, (uint_t *)&objp->st_nlink))
        return FALSE;
    if(!xdr_u_int (xdrs, (uint_t *)&objp->st_uid))
        return FALSE;
    if(!xdr_u_int (xdrs, (uint_t *)&objp->st_gid))
        return FALSE;
    if(!xdr_u_int (xdrs, (uint_t *)&objp->st_rdev))
        return FALSE;
    if(!xdr_u_int (xdrs, (uint_t *)&objp->st_size))
        return FALSE;
    if(!xdr_u_int (xdrs, (uint_t *)&objp->st_blksize))
        return FALSE;
    if(!xdr_u_int (xdrs, (uint_t *)&objp->st_blocks))
        return FALSE;
    if(!xdr_skye_timespec (xdrs, &objp->st_atime)) 
        return FALSE;
    if(!xdr_skye_timespec (xdrs, &objp->st_mtime))
        return FALSE;
    if(!xdr_skye_timespec (xdrs, &objp->st_ctime))
        return FALSE;
    return TRUE;
}
