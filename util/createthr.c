#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

volatile int curfile;
volatile int lastfile;

void alarmhandler(int signal){
    (void)signal;

    int now = curfile;
    printf("%d\n", now - lastfile);
    lastfile = now;

    alarm(1);
}

int main(int argc, char **argv){
    signal(SIGALRM, alarmhandler);
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

    //ualarm(1000000, 1000000);
    alarm(1);
    int errors = 0;
    for (curfile = 0; curfile < maxfiles; curfile++){
        sprintf(postfix, "%d", curfile);

        if (mknod(filename, 0666, 0) < 0) {
            fprintf(stderr, "mknod(%s) error: %s\n",
                    filename, strerror(errno));
            if (++errors > 50)
                exit(2);
        }
    }

    exit(0);
}
