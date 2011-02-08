CFLAGS	:= -g -Wall -Wextra -DDBG_ENABLED `pkg-config fuse --cflags` 
SRCS = $(wildcard *.c)
HDRS = $(wildcard *.h) $(RPC_H)
OBJS = $(addsuffix .o, $(basename $(SRCS)))
