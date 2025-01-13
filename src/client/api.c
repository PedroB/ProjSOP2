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

int f_req, f_resp,f_notif, f_server;
sessionProtoMessage sessionMessage;

int kvs_connect(char const *req_pipe_path, char const *resp_pipe_path,
                char const *server_pipe_path, char const *notif_pipe_path){

  unlink(req_pipe_path);
  unlink(resp_pipe_path);
  unlink(notif_pipe_path);

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

  ssize_t n = write(f_server, (void *)&sessionMessage, sizeof(sessionMessage));
  
  if (n != sizeof(sessionMessage)) {
    exit(1);
  }

   if ((f_resp = open(sessionMessage.resp_pipe_path, O_RDONLY)) < 0) exit(1);
   read_response();


   if ((f_req = open(sessionMessage.req_pipe_path, O_WRONLY)) < 0) exit(1);
     if ((f_notif = open(sessionMessage.notif_pipe_path, O_RDONLY)) < 0) exit(1);


  return 0;
}

void read_response() {

  char response[2]; 
  read(f_resp, response, 2); 

	printf("Server returned %c for operation: %c\n", response[1], response[0]);
}

int kvs_disconnect(void) {
 
  char msg = '2';
  ssize_t n = write_all(f_req, &msg, 1);
    
  if (n != sizeof(msg)) {
    exit(1);
  }

  read_response();

  close(f_req);
  close(f_resp);
  close(f_notif);
  unlink(sessionMessage.req_pipe_path);
  unlink(sessionMessage.resp_pipe_path);
  unlink(sessionMessage.notif_pipe_path);

  return 0;
}

/////////////////////////////////////////////////////////////////////////////
int kvs_subscribe_unsubscribe(const char *key, char mode) {

    sessionMessage.opcode = mode;

    // Prepare the message with format: "OP_CODE|key"
    char msg[MAX_STRING_SIZE + 2];  // +2 for mode and null-terminator
    int snprintf_len = snprintf(msg, sizeof(msg), "%c%s", mode, key);

    if (snprintf_len < 0 || (size_t)snprintf_len >= sizeof(msg)) {
        fprintf(stderr, "Message too long or error in snprintf\n");
        close(f_req);
        return 1;
    }

    size_t msg_len = (size_t)snprintf_len;  

    write(f_req, msg, msg_len );

   read_response();
    return 0;
}
////////////////////////////////////////////
// Read from notif pipe and print to stdout only when the notif pipe has changed
void *read_Thread() {
    notifMessage notification;
    notifMessage last_notification = { .key = "", .value = "" }; // Initialize with empty values

    while (1) {
        // Read from the pipe
        read_all(f_notif, &notification, sizeof(notification), NULL);

        // Compare the new notification with the last one
        if (strcmp(notification.key, last_notification.key) != 0 || 
            strcmp(notification.value, last_notification.value) != 0) {
            // New data detected, print it
            printf("(%s,%s)\n", notification.key, notification.value);

            // Update the last_notification
            last_notification = notification;
        }

        // Optional: Add a short sleep to reduce CPU usage
        sleep(10); 
    }

    return NULL;
}

