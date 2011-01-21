#ifndef CLIENT_H
#define CLIENT_H

/** options for fuse_opt.h */
struct client_options {
   char* host;
   int port;
};

extern struct client_options client_options;

#endif
