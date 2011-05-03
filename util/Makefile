include ../Makefile.inc

CFLAGS += -iquote ..
LDFLAGS += -lpthread `pkg-config fuse --libs`

TARGETS = mdtest

all: $(TARGETS)

$(OBJS) : $(HDRS)

mdtest: mdtest.c
	mpicc -Wall -DLinux -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE=1 -D__USE_LARGEFILE64=1 -g -o mdtest mdtest.c -lm

clean :
	rm -f $(TARGETS) $(OBJS)
