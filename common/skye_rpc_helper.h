/* This file is part of SkyeFS, an implementatino of Giga+ on PVFS.
 *
 * Copyright 2011-2012, Anthony Chivetta <anthony@chivetta.org>
 *
 * See COPYING for licence information.
 */
#ifndef GIGA_RPC_HELPER_H
#define GIGA_RPC_HELPER_H   

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <inttypes.h>

#include "giga_index.h"

bool_t xdr_PVFS_object_ref(XDR *xdrs, PVFS_object_ref *objp);
bool_t xdr_PVFS_credentials(XDR *xdrs, PVFS_credentials *objp);
bool_t xdr_mode_t(XDR *xdrs, mode_t *mode);
bool_t xdr_giga_mapping_t(XDR *xdrs, struct giga_mapping_t *objp);

#endif /* GIGA_RPC_HELPER_H */
