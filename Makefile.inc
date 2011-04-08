CFLAGS	= -g -Wall -Wextra `pkg-config fuse --cflags` -Werror
LDFLAGS = -lm
SRCS = $(wildcard *.c)
HDRS = $(wildcard *.h) $(RPC_H)
OBJS = $(addsuffix .o, $(basename $(SRCS)))
