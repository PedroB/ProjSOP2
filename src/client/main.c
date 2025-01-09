#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "parser.h"
#include "src/client/api.h"
#include "src/common/constants.h"
#include "src/common/io.h"

int main(int argc, char *argv[]) {
  if (argc < 3) {
    fprintf(stderr, "Usage: %s <client_unique_id> <register_pipe_path>\n",
            argv[0]);
    return 1;
  }

  char req_pipe_path[256] = "/tmp/req";
  char resp_pipe_path[256] = "/tmp/resp";
  char notif_pipe_path[256] = "/tmp/notif";

  // char e[MAX_NUMBER_SUB][MAX_KEY_CHARS] = {0};
  unsigned int delay_ms;
  size_t num;

  strncat(req_pipe_path, argv[1], strlen(argv[1]) * sizeof(char));
  strncat(resp_pipe_path, argv[1], strlen(argv[1]) * sizeof(char));
  strncat(notif_pipe_path, argv[1], strlen(argv[1]) * sizeof(char));


  if(kvs_connect(req_pipe_path,resp_pipe_path,argv[2],notif_pipe_path)){
    fprintf(stderr, "Failed to connect to KVS\n");
    return 1;
  } 

  //dispatch the thread that reads from the response pipe and prints to stdout
  if (pthread_create(&read_thread, NULL, read_Thread, NULL) != 0) {
      fprintf(stderr, "Erro ao criar thread");
      exit(EXIT_FAILURE);
  }


  while (1) {
    switch (get_next(STDIN_FILENO)) {
    case CMD_DISCONNECT:
      if (kvs_disconnect() != 0) {
        fprintf(stderr, "Failed to disconnect to the server\n");
        return 1;
      }
      // TODO: end notifications thread
      printf("Disconnected from server\n");
      return 0;

    case CMD_SUBSCRIBE:
      // if (num == 0) {
      //   fprintf(stderr, "Invalid command. See HELP for usage\n");
      //   continue;
      // }

      if (kvs_subscribe_unsubscribe(keys[0], OP_CODE_SUBSCRIBE)) {
        fprintf(stderr, "Command subscribe failed\n");
      }

      break;

    case CMD_UNSUBSCRIBE:
      num = parse_list(STDIN_FILENO, keys, 1, MAX_STRING_SIZE);
      if (num == 0) {
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        continue;
      }

      if (kvs_subscribe_unsubscribe(keys[0], OP_CODE_UNSUBSCRIBE)) {
        fprintf(stderr, "Command subscribe failed\n");
      }

      break;

    case CMD_DELAY:
      if (parse_delay(STDIN_FILENO, &delay_ms) == -1) {
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        continue;
      }

      if (delay_ms > 0) {
        printf("Waiting...\n");
        delay(delay_ms);
      }
      break;

    case CMD_INVALID:
      fprintf(stderr, "Invalid command. See HELP for usage\n");
      break;

    case CMD_EMPTY:
      break;

    case EOC:
      kvs_disconnect();
      break;
    }
  }

  pthread_join(read_Thread, NULL);

}
