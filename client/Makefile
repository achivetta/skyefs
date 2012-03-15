include ../Makefile.inc

CFLAGS += -iquote ..
LDFLAGS += -lpthread -lpvfs2 `pkg-config fuse --libs` /usr/lib/libssl.so.0.9.8

TARGETS = ../skye_client

all: $(TARGETS)

$(OBJS) : $(HDRS)

../skye_client : $(OBJS) ../common.a
	$(CC) -o $@ $^ ../common.a $(LDFLAGS)

clean :
	rm -f $(TARGETS) $(OBJS)
