#include "api.h"

#include "src/common/constants.h"
#include "src/common/protocol.h"
#include <sys/fcntl.h>
#include <sys/_types/_ssize_t.h>

char req_Pipe_Path[MAX_PIPE_PATH_LENGTH + 1], resp_Pipe_Path[MAX_PIPE_PATH_LENGTH + 1];

int f_req, f_resp, f_server, session_id;
sessionRqst sessionRQST;

int kvs_connect(char const *req_pipe_path, char const *resp_pipe_path,
                char const *server_pipe_path, char const *notif_pipe_path,
                int *notif_pipe) {
  strncpy(req_Pipe_Path, req_pipe_path, MAX_PIPE_PATH_LENGTH + 1);
  strncpy(resp_Pipe_Path, resp_pipe_path, MAX_PIPE_PATH_LENGTH + 1);
  unlink(req_pipe_path);
  unlink(resp_pipe_path);

  if (mkfifo(req_pipe_path, 0777) < 0) exit(1);
  if (mkfifo(resp_pipe_path, 0777) < 0) exit(1);
  if (mkfifo(notif_pipe,0777) < 0) exit(1);

  if ((f_server = open(server_pipe_path, O_WRONLY)) < 0) exit(1);

  sessionRQST.op_code = '1';
  memset(sessionRQST.req_pipe_path, 0, MAX_PIPE_PATH_LENGTH);
  memset(sessionRQST.resp_pipe_path, 0, MAX_PIPE_PATH_LENGTH);
  strncpy(sessionRQST.req_pipe_path, req_pipe_path, MAX_PIPE_PATH_LENGTH);
  strncpy(sessionRQST.resp_pipe_path, resp_pipe_path, MAX_PIPE_PATH_LENGTH);


  ssize_t n = write(f_server, (void *)&sessionRQST, sizeof(sessionRQST));
  if (n != sizeof(sessionRQST)) {
    exit(1);
  }

  return 0;
}

int kvs_disconnect(void) {
  // close pipes and unlink pipe files
  return 0;
}

int kvs_subscribe(const char *key) {
  // send subscribe message to request pipe and wait for response in response
  // pipe
  return 0;
}

int kvs_unsubscribe(const char *key) {
  // send unsubscribe message to request pipe and wait for response in response
  // pipe
  return 0;
}
