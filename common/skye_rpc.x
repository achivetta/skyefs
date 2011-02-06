/*
 * RPC definitions for the SkyeFs
 */

#include <asm-generic/errno-base.h>

#ifdef RPC_HDR
%#include <pvfs2-types.h>
#elif RPC_XDR
%#include "skye_rpc_helper.h"
#endif

/* FIXME: this should be global */
#define MAX_PATHNAME    256
#define MAX_BUF_SIZE    4096

typedef string skye_pathname<MAX_PATHNAME>;
typedef opaque skye_file_data<MAX_BUF_SIZE>;
typedef long skye_bitmap;

union skye_lookup switch (int errnum) {
	case 0:
		PVFS_object_ref ref;
	case -EAGAIN:
		skye_bitmap bitmap;
	default:
		void;
};

/* RPC definitions */

program SKYE_RPC_PROG {                 /* program number */
	version SKYE_RPC_VERSION {          /* version number */
		bool SKYE_RPC_INIT(void) = 1;
		skye_lookup SKYE_RPC_LOOKUP(PVFS_object_ref, skye_pathname) = 2;
	} = 1;
} = 522222; /* FIXME: Is this a okay value for program number? */
