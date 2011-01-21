include ../Makefile.inc

CFLAGS += -I..
LDFLAGS += -lpthread `pkg-config fuse --libs`

TARGETS = skye-ls

all: $(TARGETS)

$(SRCS) : $(HDRS)

skye-ls : skye-ls.o
	$(CC) -o $@ $^ ../common.a $(LDFLAGS)

clean :
	rm -f $(TARGETS) $(OBJS)
