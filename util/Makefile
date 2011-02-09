include ../Makefile.inc

CFLAGS += -iquote ..
LDFLAGS += -lpthread `pkg-config fuse --libs`

TARGETS =

all: $(TARGETS)

$(OBJS) : $(HDRS)

clean :
	rm -f $(TARGETS) $(OBJS)
