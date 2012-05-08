#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <pthread.h>

volatile int curfile;
volatile int lastfile;
int errors;

void *thread_main(void *unused){
    (void)unused;
    int now, ret;

    struct timespec ts;
    ts.tv_sec = 1;
    ts.tv_nsec = 0;

    struct timespec rem;

top:

    now = curfile;
    printf("%d\n", now - lastfile);
    lastfile = now;

    ret = nanosleep(&ts, &rem);
    if (ret == -1){
        if (errno == EINTR)
            nanosleep(&rem, NULL);
        else
            errors++;
    }


    if (errors > 50)
        return NULL;
    else
        goto top;
}

int main(int argc, char **argv){
    setvbuf(stdout,NULL,_IONBF,0);

    if (argc != 3){
        fprintf(stderr, "Usage: %s <test directory> <number of files to create>\n", argv[0]);
        exit(1);
    }

    if(chdir(argv[1])){
        fprintf(stderr, "chdir(%s) error: %s\n",
                argv[1], strerror(errno));
        exit(1);
    }

    char filename[64];
    gethostname(filename, 64);

    char *postfix;
    postfix = filename + strlen(filename);
    sprintf(postfix, ".%d.", getpid());
    postfix = postfix + strlen(postfix);

    lastfile = 0;
    int maxfiles = atoi(argv[2]);

    fprintf(stderr, "prefix: %s\n", filename);

    pthread_t tid;
    int ret;
    if ((ret = pthread_create(&tid, NULL, thread_main, NULL))){
        fprintf(stderr, "pthread_create() error: %d\n",
                ret);
        exit(1);
    }
    if ((ret = pthread_detach(tid))){
        fprintf(stderr, "pthread_detach() error: %d\n",
                ret);
        exit(1);
    }

    for (curfile = 0; curfile < maxfiles; curfile++){
        sprintf(postfix, "%d", curfile);

        if (mknod(filename, 0666, 0) < 0) {
            fprintf(stderr, "mknod(%s) error: %s\n",
                    filename, strerror(errno));
            if (++errors > 50)
                exit(2);
        }
    }

    errors = 100;

    exit(0);
}
