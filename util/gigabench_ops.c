
#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "gigabench_ops.h"

static char* makename(char *dir, char *file);

void scan_readdir(char *dir_path, int readdir_flag) 
{
    DIR *pDir;
    struct dirent *pDentry;
   
    if ((pDir = opendir(dir_path)) == NULL) {
        printf("[%s] ERROR: opendir(%s) - [%s]\n", __func__,
               dir_path, strerror(errno));
        exit(EXIT_FAILURE);
    }
    else {
        for (;;) {
            if ((pDentry = readdir(pDir)) == NULL) {
                printf("[%s] ERROR: during readdir(%s) - [%s]\n", __func__,
                       dir_path, strerror(errno));
                break;
            }

            if (readdir_flag == DO_GETATTR) {
                struct stat sb;
                if (stat(makename(dir_path, pDentry->d_name), &sb) == -1) {
                    printf("[%s] ERR: stat(%s) in dir(%s) - [%s]\n", __func__, 
                            pDentry->d_name, dir_path, strerror(errno));
                    exit(EXIT_FAILURE);
                }
            }

        }
    }
    
    if (closedir(pDir) < 0) {
        printf("[%s] ERROR: closedir(%s) - [%s]\n", __func__,
                dir_path, strerror(errno));
        exit(EXIT_FAILURE);
    }

    return;
}

void lookup_file(const char *path_name)
{
    struct stat sb;

    if (stat(path_name, &sb) == -1) {
        printf("[%s] ERROR: stat(%s) - [%s]\n", __func__,
               path_name, strerror(errno));
        exit(EXIT_FAILURE);
    }

    return;
}

void create_file(const char *path_name)
{
    (void)path_name;
}

/*
 * Construct a pathname of the form <dir/file>
 *
 * Reusing code from OpenSolaris "ls" @ src/cmd/ls/ls.c
 *
 */
static char* makename(char *dir, char *file)
{
    // PATH_MAX is the maximum length of a path name.
    // MAXNAMLEN is the maximum length of any path name component.
    // Allocate space for both, plus the '/' in the middle
    // and the null character at the end.
    // dfile is static as this is returned by makename().
    //
    static char dfile[MAX_LEN + 1];
    char *dp, *fp;

    dp = dfile;
    fp = dir;
    while (*fp)
        *dp++ = *fp++;
    if (dp > dfile && *(dp - 1) != '/')
        *dp++ = '/';
    fp = file;
    while (*fp)
        *dp++ = *fp++;
    *dp = '\0';
    return (dfile);
}

