

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#include "common/constants.h"
#include "common/io.h"
#include "common/constants.h"
#include "server_parser.h"


enum Server_Command get_next2(int f_req) {
  char op_code[1];
  if (read(f_req, &op_code[0], sizeof(op_code)) != 1) {
    return CMD_DISCONNECT;
  }
  
  switch (op_code[0]) {
    case '3':
      return CMD_SUBSCRIBE;

    case '4':
      return CMD_UNSUBSCRIBE;

    default:
      return CMD_INVALID;
  }
}