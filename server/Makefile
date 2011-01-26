include ../Makefile.inc

CFLAGS += -I..
LDFLAGS += -lpthread -lpvfs2

TARGETS = ../skye_server

all: $(TARGETS)

$(SRCS) : $(HDRS)

../skye_server : $(OBJS)
	$(CC) -o $@ $^ ../common.a $(LDFLAGS)

clean :
	rm -f $(TARGETS) $(OBJS)
