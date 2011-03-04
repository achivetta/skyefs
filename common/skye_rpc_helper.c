#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>

#include <pvfs2-types.h>

#include "skye_rpc.h"
#include "skye_rpc_helper.h"

/* DANGER, WILL ROBINSON!: this depends on the internal details of
 * PVFS_object_ref and could break in newer/older PVFS versions
 */
bool_t xdr_PVFS_object_ref(XDR *xdrs, PVFS_object_ref *objp)
{
    if (!xdr_uint64_t(xdrs, (uint64_t *)&objp->handle))
        return FALSE;
    if (!xdr_uint32_t(xdrs, (uint32_t *)&objp->fs_id))
        return FALSE;
    return TRUE;
}

/* DANGER, WILL ROBINSON!: this depends on the internal details of
 * PVFS_object_ref and could break in newer/older PVFS versions
 */
bool_t xdr_PVFS_credentials(XDR *xdrs, PVFS_credentials *objp){
    if (!xdr_uint32_t(xdrs, (uint32_t *)&objp->uid))
        return FALSE;
    if (!xdr_uint32_t(xdrs, (uint32_t *)&objp->gid))
        return FALSE;
    return TRUE;
}

/* DANGER, WILL ROBINSON!: this depends on mode_t being a 32 bit int. It will
 * break otherwise
 */
bool_t xdr_mode_t(XDR *xdrs, mode_t *mode)
{
    if (!xdr_uint32_t(xdrs, (uint32_t *)mode))
        return FALSE;
    return TRUE;
}

