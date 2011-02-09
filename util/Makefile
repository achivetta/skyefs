include ../Makefile.inc

CFLAGS += -iquote ..
LDFLAGS += -lpthread `pkg-config fuse --libs`

TARGETS = skye-ls

all: $(TARGETS)

$(OBJS) : $(HDRS)

skye-ls : skye-ls.o
	$(CC) -o $@ $^ ../common.a $(LDFLAGS)

clean :
	rm -f $(TARGETS) $(OBJS)
