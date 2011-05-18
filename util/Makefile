include ../Makefile.inc

CFLAGS += -iquote ..
LDFLAGS += -lpthread `pkg-config fuse --libs`

TARGETS = gigabench mdtest simul createthr

all: createthr

$(OBJS) : $(HDRS)

mdtest: mdtest.c
	mpicc -Wall -DLinux -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE=1 -D__USE_LARGEFILE64=1 -g -o mdtest mdtest.c -lm

simul: simul.c
	mpicc -Wall -o simul simul.c

gigabench: gigabench.o gigabench_ops.o

createthr: createthr.o

clean :
	rm -f $(TARGETS) $(OBJS)
