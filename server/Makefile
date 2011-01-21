include ../Makefile.inc

CFLAGS += -I..
LDFLAGS += -lpthread

TARGETS = ../skye_server

all: $(TARGETS)

$(SRCS) : $(HDRS)

../skye_server : $(OBJS)
	$(CC) -o $@ $^ ../common.a $(LDFLAGS)

clean :
	rm -f $(TARGETS) $(OBJS)
