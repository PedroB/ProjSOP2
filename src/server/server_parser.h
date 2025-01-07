
#define CLIENT_PARSER_H

#include <stddef.h>

enum Server_Command {
  CMD_INVALID,
  CMD_CONNECT,
  CMD_DISCONNECT,
  CMD_SUBSCRIBE,
  CMD_UNSUBSCRIBE
  // End of commands
};