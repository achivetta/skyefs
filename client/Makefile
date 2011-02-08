include ../Makefile.inc

CFLAGS += -iquote ..
LDFLAGS += -lpthread -lpvfs2 `pkg-config fuse --libs`

TARGETS = ../skye_client

all: $(TARGETS)

$(SRCS) : $(HDRS)

../skye_client : $(OBJS)
	$(CC) -o $@ $^ ../common.a $(LDFLAGS)

clean :
	rm -f $(TARGETS) $(OBJS)
