CFLAGS	:= -Wall -D_FILE_OFFSET_BITS=64 -DDBG_ENABLED
LDFLAGS	:= -lpthread -lm -lfuse
RPCGENFLAGS := -N -M

CLIENT := skyefs_client
SERVER := skyefs_server

RPC_X = skyefs_rpc
RPC_H = ${RPC_X}.h
RPC_C = ${RPC_X}_svc.c ${RPC_X}_clnt.c ${RPC_X}_xdr.c
RPC_O = ${RPC_X}_xdr.o  ${RPC_X}_clnt.o ${RPC_X}_svc.o

SRCS := $(wildcard *.c)
HDRS := $(wildcard *.h) $(RPC_H)
OBJS := $(addsuffix .o, $(basename $(SRCS)))

COMMON_O := trace.o
SERVER_O := server.o 
CLIENT_O :=

all: $(SERVER)

$(SERVER) : $(COMMON_O) $(SERVER_O) $(RPC_O)
	$(CC) -o $@ $^ $(LDFLAGS)

$(CLIENT) : $(COMMON_O) $(CLIENT_O) $(RPC_O)
	$(CC) -o $@ $^ $(LDFLAGS)

$(RPC_O) : $(RPC_C)
$(RPC_H) $(RPC_C) : $(RPC_X).x
	rpcgen $(RPCGENFLAGS) $(RPC_X).x
	rpcgen $(RPCGENFLAGS) -m $(RPC_X).x > $(RPC_X)_svc.c

$(SRCS) : $(HDRS)

clean:
	/bin/rm -f $(TARGETS) $(RPC_H) $(RPC_C) $(RPC_O) $(OBJS)
