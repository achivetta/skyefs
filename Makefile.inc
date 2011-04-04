CFLAGS	= -g -Wall -Wextra `pkg-config fuse --cflags` 
LDFLAGS = -lm
SRCS = $(wildcard *.c)
HDRS = $(wildcard *.h) $(RPC_H)
OBJS = $(addsuffix .o, $(basename $(SRCS)))
