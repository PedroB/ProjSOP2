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


// char req_Pipe_Path[MAX_PIPE_PATH_LENGTH + 1], resp_Pipe_Path[MAX_PIPE_PATH_LENGTH + 1], notif_Pipe_Path[MAX_PIPE_PATH_LENGTH + 1];

int f_req, f_resp,f_notif, f_server;
sessionProtoMessage sessionMessage;


int kvs_connect(char const *req_pipe_path, char const *resp_pipe_path,
                char const *server_pipe_path, char const *notif_pipe_path){
  // strncpy(req_Pipe_Path, req_pipe_path, MAX_PIPE_PATH_LENGTH + 1);
  // strncpy(resp_Pipe_Path, resp_pipe_path, MAX_PIPE_PATH_LENGTH + 1);
  // strncpy(notif_Pipe_Path,notif_pipe_path,MAX_PIPE_PATH_LENGTH + 1);
  unlink(req_pipe_path);
  unlink(resp_pipe_path);
  unlink(notif_pipe_path);

  puts("entrou connect");
  if (mkfifo(req_pipe_path, 0777) < 0) exit(1);
  if (mkfifo(resp_pipe_path, 0777) < 0) exit(1);
  if (mkfifo(notif_pipe_path,0777) < 0) exit(1);

  if ((f_server = open(server_pipe_path, O_WRONLY)) < 0) exit(1);

  sessionMessage.opcode = '1';
  memset(sessionMessage.req_pipe_path, 0, MAX_PIPE_PATH_LENGTH);
  memset(sessionMessage.resp_pipe_path, 0, MAX_PIPE_PATH_LENGTH);
  memset(sessionMessage.notif_pipe_path, 0, MAX_PIPE_PATH_LENGTH);
  strncpy(sessionMessage.req_pipe_path, req_pipe_path, MAX_PIPE_PATH_LENGTH);
  strncpy(sessionMessage.resp_pipe_path, resp_pipe_path, MAX_PIPE_PATH_LENGTH);
  strncpy(sessionMessage.notif_pipe_path, notif_pipe_path, MAX_PIPE_PATH_LENGTH);

  printf("preencheu sessionMessage: %s", sessionMessage.req_pipe_path);

  // ssize_t n = write_all(f_server, (void *)&sessionMessage, sizeof(sessionProtoMessage));
  // write_all(f_server, (void *)&sessionMessage, sizeof(sessionProtoMessage));

  ssize_t bytes_written = write_all(f_server, (void *)&sessionMessage, sizeof(sessionProtoMessage));


    //     // Print raw data received
    // printf("Raw data WRITTEN (%zd bytes):\n", bytes_written);
    // for (ssize_t i = 0; i < bytes_written; i++) {
    //     printf("%02x ", ((unsigned char *)&sessionMessage)[i]);
    // }
    // printf("\n");


  // if (n != sizeof(sessionProtoMessage)) {
  //   exit(1);
  // }
  read_response();
  puts("acabou connect");

  return 0;
}

void read_response() {

  // read response in response pipe
    if ((f_resp = open(sessionMessage.resp_pipe_path, O_RDONLY)) < 0) exit(1);
    puts("response is...");

  char response[2]; // Adjust size based on expected response length
  // ssize_t resp_len = read_all(f_resp, response, 2, NULL); // Leave space for null terminator
  read_all(f_resp, response, 2, NULL); // Leave space for null terminator

  // if (resp_len < 0) {
  //     perror("Error reading from response pipe");
  //     close(f_resp);

  // }
  // response[2] = '\0';

	printf("Server returned %c for operation: %c", response[1], response[0]);
  close(f_resp);
}

int kvs_disconnect(void) {
  // sessionProtoMessage sessionRQST;

  // size_t msgSize = 1 + sizeof(sessionRQST);
  // char *msg = malloc(msgSize);

  char msg = '2';
  // sessionRQST.session_id = session_id;
  // memcpy(&msg[1], &dcRQST, sizeof(dcRQST));



  // ssize_t n = write_all(f_req, (void *)msg, msgSize);
    write_all(f_req, &msg, 1);


  // if (n != (ssize_t)msgSize) {
  //   exit(1);
  // }

  read_response();

  close(f_req);
  // close(f_resp);
  close(f_notif);
  unlink(sessionMessage.req_pipe_path);
  unlink(sessionMessage.resp_pipe_path);
  unlink(sessionMessage.notif_pipe_path);

  return 0;
}


///////////////////////////////////////////////////////
// int kvs_subscribe_unsubscribe(const char *key, char mode) {
//   // send subscribe message to request pipe 
//     // const char *req_pipe_path = sessionMessage.req_pipe_path; 
//     // const char *resp_pipe_path = sessionMessage.resp_pipe_path; // Response pipe path

//   puts("entrou subs!!!!!");
//     if ((f_req = open(sessionMessage.req_pipe_path, O_WRONLY)) < 0) exit (1);



//     // Prepare the message with format: "OP_CODE|key"             
//     char msg[MAX_STRING_SIZE + 2]; 
//     int msg_len = snprintf(msg, sizeof(msg), "%c%s", mode, key);

//     // if (msg_len < 0 || (size_t)msg_len >= sizeof(msg)) {
        
//     //     close(f_req);
//     //     return 1;
//     // }
//     // Write the message to the request pipe
//     // ssize_t n = write_all(f_req, msg,(size_t) msg_len);
//       // ssize_t n = write_all(f_req, msg, 42);
//       write_all(f_req, msg, 42);
//       char tmp_msg[MAX_STRING_SIZE + 2] = {0};
//       memcpy(&tmp_msg, &msg, 42);
//       printf("pediu subscribe: %s", tmp_msg);

//     // if (n != msg_len) {
//     //     close(f_req);
//     //     return 1;
//     // }
//     close(f_req);

//     read_response();
//   return 0;
// }
/////////////////////////////////////////////////////////////////////////////
int kvs_subscribe_unsubscribe(const char *key, char mode) {
    puts("entrou subs!!!!!");

    printf("Key: %s, Mode: %c\n", key, mode);


    // Open the request pipe for writing
    if ((f_req = open(sessionMessage.req_pipe_path, O_WRONLY)) < 0) exit(1);
    printf("Opening request pipe: %s\n", sessionMessage.req_pipe_path);


    // Prepare the message with format: "OP_CODE|key"
    char msg[MAX_STRING_SIZE + 2];  // +2 for mode and null-terminator
    int snprintf_len = snprintf(msg, sizeof(msg), "%c%s", mode, key);

    if (snprintf_len < 0 || (size_t)snprintf_len >= sizeof(msg)) {
        fprintf(stderr, "Message too long or error in snprintf\n");
        close(f_req);
        return 1;
    }

    size_t msg_len = (size_t)snprintf_len;  // Explicit conversion to size_t

    // Debug: Show the message content before sending it
    printf("Prepared message: %s (length: %zu)\n", msg, msg_len);

    // Write the message to the request pipe
    ssize_t n = write_all(f_req, msg, msg_len);
    if (n < 0) {
        fprintf(stderr, "write_all failed\n");
        close(f_req);
        return 1;
    }

    // Debug: Check if the message was written successfully
    printf("pediu subscribe: %s\n", msg);

    // Close the request pipe
    close(f_req);

    // Read the response (assuming this is handled elsewhere)
    read_response();
    return 0;
}

//////////////////////////////////////////////////////////////////////////////////////
// read from notif pipe and print to stdout
void *read_Thread() {


  // read from notif pipe
  if ((f_notif = open(sessionMessage.notif_pipe_path, O_RDONLY)) < 0) exit(1);


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

