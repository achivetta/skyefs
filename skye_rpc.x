/*
 * RPC definitions for the SkyeFs
 */

#ifdef RPC_HDR
%#include <sys/stat.h>
#elif RPC_XDR
%#include "skye_rpc_helper.h"
#endif

/* FIXME: this should be global */
#define MAX_PATHNAME    256
#define MAX_BUF_SIZE    4096

typedef string pathname<MAX_PATHNAME>;
typedef opaque file_data<MAX_BUF_SIZE>;

enum skye_error {
	SKYE_RPC_ENONE = 0,
	SKYE_RPC_EINVAL,
	SKYE_RPC_ENOENT,
	SKYE_RPC_EEXIST,
	SKYE_RPC_ENOTDIR,
	SKYE_RPC_EISDIR,
	SKYE_RPC_EIO
};

struct skye_timespec {
	int tv_sec;      /* time_t */
	long tv_nsec;
};

struct skye_readdir_req {
	pathname path;
};

struct skye_dnode {
	pathname name;
	struct stat stbuf;
	struct skye_dnode *next;
};

union skye_readdir_reply switch (int errnum) {
	case 0:
		struct skye_dnode *dlist;
	default:
		void;
};

/* RPC definitions */

program SKYE_RPC_PROG {                 /* program number */
	version SKYE_RPC_VERSION {          /* version number */
		bool SKYE_RPC_INIT(void) = 1;
		skye_readdir_reply SKYE_RPC_READDIR(skye_readdir_req) = 2;
	} = 1;
} = 522222; /* FIXME: Is this a okay value for program number? */