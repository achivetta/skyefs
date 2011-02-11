#ifndef TRACE_H
#define TRACE_H   

#include	<stdarg.h>		
#include	<stdio.h>		
#include	<syslog.h>	

#define MAX_ERR_BUF_SIZE        1024

// TODO: add err_pvfs()
// Error dumping methods
void err_ret(const char *fmt, ...);
void err_sys(const char *fmt, ...);
void err_dump(const char *fmt, ...);
void err_msg(const char *fmt, ...);
void err_quit(const char *fmt, ...);

#define DBG_ENABLED         1       // Flag for general debugging
#define DBG_FUNC_ENABLED    1       // Flag for in-function debugging

FILE *log_fp;

double get_time();

// Debug message dumping methods 
void dbg_msg(FILE* fp, const char *fmt, ...);
void dbg_msg_func(FILE* fp, const char *fmt, ...);

#endif /* TRACE_H */
