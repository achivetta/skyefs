CFLAGS	:= -g -Wall -DDBG_ENABLED -std=gnu99 `pkg-config fuse --cflags` 
SRCS = $(wildcard *.c)
HDRS = $(wildcard *.h) $(RPC_H)
OBJS = $(addsuffix .o, $(basename $(SRCS)))
