CFLAGS	:= -g -Wall -DDBG_ENABLED -std=gnu99 `pkg-config fuse --cflags`
LDFLAGS	:= -lpthread -lm -lfuse `pkg-config fuse --libs`
RPCGENFLAGS := -N -M

CLIENT := skye_client
SERVER := skye_server

RPC_X = skye_rpc
RPC_H = ${RPC_X}.h
RPC_C = ${RPC_X}_svc.c ${RPC_X}_clnt.c ${RPC_X}_xdr.c
RPC_COMMON_O = ${RPC_X}_xdr.o
RPC_SERVER_O = ${RPC_X}_svc.o
RPC_CLIENT_O = ${RPC_X}_clnt.o
RPC_O = ${RPC_COMMON_O} ${RPC_SERVER_O} ${RPC_CLIENT_O}

SRCS := $(wildcard *.c)
HDRS := $(wildcard *.h) $(RPC_H)
OBJS := $(addsuffix .o, $(basename $(SRCS)))

COMMON_O := trace.o $(RPC_X)_helper.o ${RPC_COMMON_O}
SERVER_O := server.o $(RPC_X)_handlers.o ${RPC_SERVER_O}
CLIENT_O := client.c ${RPC_CLIENT_O}

all: $(SERVER) $(CLIENT)

$(SERVER) : $(COMMON_O) $(SERVER_O)
	$(CC) -o $@ $^ $(LDFLAGS)

$(CLIENT) : $(COMMON_O) $(CLIENT_O)
	$(CC) -o $@ $^ $(LDFLAGS)

$(RPC_O) : $(RPC_C)
$(RPC_H) $(RPC_C) : $(RPC_X).x
	rpcgen $(RPCGENFLAGS) $(RPC_X).x
	rpcgen $(RPCGENFLAGS) -m $(RPC_X).x > $(RPC_X)_svc.c

$(SRCS) : $(HDRS)

clean:
	/bin/rm -f $(TARGETS) $(RPC_H) $(RPC_C) $(RPC_O) $(OBJS)

tags: $(SRCS) $(HDRS) $(RPC_H) $(RPC_C)
	ctags $(SRCS) $(HDRS) $(RPC_H) $(RPC_C) /usr/include/rpc/
