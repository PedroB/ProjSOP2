#ifndef SERVER_PARSER_H
#define SERVER_PARSER_H

// Header content here

#include <stddef.h>

enum Server_Command {
  // CMD_CONNECT,
  CMD_DISCONNECT,
  CMD_SUBSCRIBE,
  CMD_UNSUBSCRIBE
  // End of commands
};

enum Server_Command get_next2(int f_req);

#endif
