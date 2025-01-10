#include "api.h"

#include "src/common/constants.h"
#include "src/common/protocol.h"
#include "src/common/io.h"
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <string.h>
// #include <sys/_types/_ssize_t.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>


char req_Pipe_Path[MAX_PIPE_PATH_LENGTH + 1], resp_Pipe_Path[MAX_PIPE_PATH_LENGTH + 1], notif_Pipe_Path[MAX_PIPE_PATH_LENGTH + 1];

int f_req, f_resp,f_notif, f_server, session_id;
sessionRqst sessionRQST;


int kvs_connect(char const *req_pipe_path, char const *resp_pipe_path,
                char const *server_pipe_path, char const *notif_pipe_path){
  strncpy(req_Pipe_Path, req_pipe_path, MAX_PIPE_PATH_LENGTH + 1);
  strncpy(resp_Pipe_Path, resp_pipe_path, MAX_PIPE_PATH_LENGTH + 1);
  strncpy(notif_Pipe_Path,notif_pipe_path,MAX_PIPE_PATH_LENGTH + 1);
  unlink(req_pipe_path);
  unlink(resp_pipe_path);
  unlink(notif_pipe_path);

  if (mkfifo(req_pipe_path, 0777) < 0) exit(1);
  if (mkfifo(resp_pipe_path, 0777) < 0) exit(1);
  if (mkfifo(notif_pipe_path,0777) < 0) exit(1);

  if ((f_server = open(server_pipe_path, O_WRONLY)) < 0) exit(1);

  sessionRQST.result = '1';
  memset(sessionRQST.req_pipe_path, 0, MAX_PIPE_PATH_LENGTH);
  memset(sessionRQST.resp_pipe_path, 0, MAX_PIPE_PATH_LENGTH);
  memset(sessionRQST.notif_pipe_path, 0, MAX_PIPE_PATH_LENGTH);
  strncpy(sessionRQST.req_pipe_path, req_pipe_path, MAX_PIPE_PATH_LENGTH);
  strncpy(sessionRQST.resp_pipe_path, resp_pipe_path, MAX_PIPE_PATH_LENGTH);
  strncpy(sessionRQST.notif_pipe_path, notif_pipe_path, MAX_PIPE_PATH_LENGTH);


  ssize_t n = write_all(f_server, (void *)&sessionRQST, sizeof(sessionRQST));
  if (n != sizeof(sessionRQST)) {
    exit(1);
  }

  return 0;
}

void read_response() {

  // read response in response pipe
    if ((f_resp = open(sessionRQST.resp_pipe_path, O_RDONLY)) < 0) exit(1);


  char response[MAX_STRING_SIZE]; // Adjust size based on expected response length
  ssize_t resp_len = read_all(f_resp, response, sizeof(response) - 1, NULL); // Leave space for null terminator
  if (resp_len < 0) {
      perror("Error reading from response pipe");
      close(f_resp);

  }
  response[resp_len] = '\0';

	printf("Server returned %c for operation: %c", response[1], response[0]);
  close(f_resp);
}

int kvs_disconnect(void) {
  sessionRqst dcRQST;

  size_t msgSize = 1 + sizeof(dcRQST);
  char *msg = malloc(msgSize);

  msg[0] = '2';
  dcRQST.session_id = session_id;
  memcpy(&msg[1], &dcRQST, sizeof(dcRQST));
  ssize_t n = write_all(f_req, (void *)msg, msgSize);
  if (n != (ssize_t)msgSize) {
    exit(1);
  }

  read_response();

  free(msg);
  close(f_req);
  // close(f_resp);
  close(f_notif);
  unlink(req_Pipe_Path);
  unlink(resp_Pipe_Path);
  unlink(notif_Pipe_Path);
  return 1;
  return 0;
}



int kvs_subscribe_unsubscribe(const char *key, int mode) {
  // send subscribe message to request pipe 

    const char *req_pipe_path = sessionRQST.req_pipe_path; 
    // const char *resp_pipe_path = sessionRQST.resp_pipe_path; // Response pipe path

    if ((f_req = open(req_pipe_path, O_WRONLY)) < 0) exit (1);

    // Prepare the message with format: "OP_CODE|key"             
    char msg[MAX_STRING_SIZE]; 
    int msg_len = snprintf(msg, sizeof(msg), "%d|%s", mode, key);

    if (msg_len < 0 || (size_t)msg_len >= sizeof(msg)) {
        
        close(f_req);
        return 1;
    }
    // Write the message to the request pipe
    ssize_t n = write_all(f_req, msg,(size_t) msg_len);
    if (n != msg_len) {

        close(f_req);
        return 1;
    }
    close(f_req);

    read_response();
  return 0;
}

//////////////////////////////////////////////////////////////////////////////////////
// read from notif pipe and print to stdout
void *read_Thread() {


  // read response in response pipe
  if ((f_notif = open(sessionRQST.notif_pipe_path, O_RDONLY)) < 0) exit(1);

  char response[MAX_STRING_SIZE]; // Adjust size based on expected response length
  ssize_t resp_len = read_all(f_resp, response, sizeof(response) - 1, NULL); // Leave space for null terminator
  if (resp_len < 0) {
      perror("Error reading from response pipe");
      close(f_resp);
  }
  response[resp_len] = '\0';

  printf("%s", response);
  close(f_notif);
  return NULL;
}

