include ../Makefile.inc

CFLAGS += -I..
LDFLAGS += `pkg-config fuse --libs`

RPCGENFLAGS := -N -M

RPC_X = skye_rpc
RPC_H = ${RPC_X}.h
RPC_C = ${RPC_X}_svc.c ${RPC_X}_clnt.c ${RPC_X}_xdr.c
RPC_O = $(addsuffix .o, $(basename $(RPC_C))) skye_rpc_helper.o

OJBS = $(RPC_O) trace.o
TARGETS = ../common.a

all: $(TARGETS)

$(SRCS) : $(HDRS)

$(RPC_H) $(RPC_C) : $(RPC_X).x
	rpcgen $(RPCGENFLAGS) $(RPC_X).x
	rpcgen $(RPCGENFLAGS) -m $(RPC_X).x > $(RPC_X)_svc.c

../common.a : $(OBJS)
	ar rv ../common.a $?
	ranlib ../common.a

clean :
	rm -f $(TARGETS) $(OBJS) $(RPC_C) $(RPC_H)
