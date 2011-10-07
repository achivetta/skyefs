include ../Makefile.inc

CFLAGS += -iquote ..
LDFLAGS += -lpthread -lpvfs2 -lssl

TARGETS = ../skye_server

all: $(TARGETS)

$(OBJS) : $(HDRS)

../skye_server : $(OBJS) ../common.a
	$(CC) -o $@ $^ ../common.a $(LDFLAGS)

clean :
	rm -f $(TARGETS) $(OBJS)
