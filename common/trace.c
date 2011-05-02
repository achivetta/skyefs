#include "trace.h"

#include <errno.h>
#include <stdarg.h>		
#include <stdlib.h>		
#include <stdio.h>
#include <string.h>
#include <syslog.h>	
#include <sys/time.h>	
#include <unistd.h>

/* TODO:
 * #define WHERESTR  "[file %s, line %d]: "
 * #define WHEREARG  __FILE__, __LINE__
 * #define DEBUGPRINT2(...)       fprintf(stderr, __VA_ARGS__)
 * #define DEBUGPRINT(_fmt, ...)  DEBUGPRINT2(WHERESTR _fmt, WHEREARG, __VA_ARGS__)
 */

#define TRACE_BUF_SIZE      1024
int		daemon_proc;		// set nonzero by daemon_init()

static void	err_doit(int, int, const char *, va_list);
static int dbg_msg_fp_va(FILE *fp, const char *format, va_list ap);

/** Nonfatal error related to a system call.
 *
 * Print a message and return.
 */
void err_ret(const char *fmt, ...)
{
	va_list		ap;

	va_start(ap, fmt);
	err_doit(1, LOG_INFO, fmt, ap);
	va_end(ap);

	return;
}

/** Fatal error related to a system call.
 *
 * Print a message and terminate.
 */
void err_sys(const char *fmt, ...)
{
	va_list		ap;

	va_start(ap, fmt);
	err_doit(1, LOG_ERR, fmt, ap);
	va_end(ap);

	exit(1);
}

/** Fatal error related to a system call.
 *
 * Print a message, dump core, and terminate.
 */
void err_dump(const char *fmt, ...)
{
	va_list		ap;

	va_start(ap, fmt);
	err_doit(1, LOG_ERR, fmt, ap);
	va_end(ap);

	abort();		// dump core and terminate
	exit(1);		// shouldn't get here
}

/** Nonfatal error unrelated to a system call.
 *
 * Print a message and return.
 */
void err_msg(const char *fmt, ...)
{
	va_list		ap;

	va_start(ap, fmt);
	err_doit(0, LOG_INFO, fmt, ap);
	va_end(ap);

	return;
}

/** Fatal error unrelated to a system call.
 *
 * Print a message and terminate.
 */
void err_quit(const char *fmt, ...)
{
	va_list		ap;

	va_start(ap, fmt);
	err_doit(0, LOG_ERR, fmt, ap);
	va_end(ap);

	exit(1);
}

/** Debug messages printed to STDOUT */
void dbg_msg(FILE *fp, const char *fmt, ...)
{
	va_list		ap;

#ifdef  DBG_ENABLED	
    // rip out the variable arguments
    va_start(ap, fmt);
    int ret_val = dbg_msg_fp_va(fp, fmt, ap);
    if (ret_val < 0)
        fprintf(stdout,"[%s] tracing error \n", __func__);
    va_end(ap);
#endif

	return;
}

/** Debug messages for tracing inside a function by setting or resetting the
 * DBG_FUNC_ENABLED flag in trace.h
 *
 * If you want to check what's going wrong in a function, just enable the flag
 * and you are good to go. If this flag is disabled (== 0), this function 
 * never appears.
 */
void dbg_msg_func(FILE *fp, const char *fmt, ...)
{
    va_list ap __attribute__((unused));

#ifdef  DBG_ENABLED
#ifdef  DBG_FUNC_ENABLED	
    // rip out the variable arguments
    va_start(ap, fmt);
    int ret_val = dbg_msg_fp_va(fp, fmt, ap);
    if (ret_val < 0)
        fprintf(stdout,"[%s] tracing error \n", __func__);
    va_end(ap);
#endif
#endif

    return;
}

static int dbg_msg_fp_va(FILE *fp, const char *format, va_list ap)
{
    char buffer[MAX_ERR_BUF_SIZE], *bptr = buffer;
    int bsize = sizeof(buffer);
    int ret = -EINVAL;

    ret = vsnprintf(bptr, bsize, format, ap);
    if (ret < 0)
        return -errno;

	strcat(buffer, "\n");
    ret = fprintf(fp, buffer);
    if (ret < 0) 
        return -errno;
    
    fflush(fp);

    return 0;
}

/** Print a message and return to caller.
 *
 * Caller specifies "errnoflag" and "level".
 */
static void err_doit(int errnoflag, int level, const char *fmt, va_list ap)
{
	int		errno_save, n;
	char	buf[MAX_ERR_BUF_SIZE];

	errno_save = errno;		// value caller might want printed
#ifdef	HAVE_VSNPRINTF
	vsnprintf(buf, sizeof(buf), fmt, ap);	// this is safe 
#else
	vsprintf(buf, fmt, ap);					// this is not safe
#endif

	n = strlen(buf);
	if (errnoflag)
		snprintf(buf+n, sizeof(buf)-n, "[%s]", strerror(errno_save));
	strcat(buf, "\n");

	if (daemon_proc) {
		syslog(level, buf, strlen(buf));
    } else {
		fflush(stdout);		/* in case stdout and stderr are the same */
		fputs(buf, stderr);
		fflush(stderr);
	}

	return;
}

double get_time()
{
    struct timeval t;
    gettimeofday(&t, NULL);
    return ((double)t.tv_sec + ((double)(t.tv_usec)/1000000));
}
