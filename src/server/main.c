#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <semaphore.h>

#include "constants.h"
#include "io.h"
#include "operations.h"
#include "parser.h"
#include "pthread.h"
#include "common/protocol.h"
#include "server/server_parser.h"

struct SharedData {
  DIR *dir;
  char *dir_name;
  pthread_mutex_t directory_mutex;
};

sessionRqst buf[MAX_BUFFER_SIZE];
int f_server;
int session_num = 0;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t n_current_backups_lock = PTHREAD_MUTEX_INITIALIZER;


sem_t semEmpty;
sem_t semFull;
pthread_mutex_t mutexBuffer;


int prodptr=0, consptr=0, count=0;

size_t active_backups = 0; // Number of active backups
size_t max_backups;        // Maximum allowed simultaneous backups
size_t max_threads;        // Maximum allowed simultaneous threads
char *jobs_directory = NULL;

int filter_job_files(const struct dirent *entry) {
  const char *dot = strrchr(entry->d_name, '.');
  if (dot != NULL && strcmp(dot, ".job") == 0) {
    return 1; // Keep this file (it has the .job extension)
  }
  return 0;
}

static int entry_files(const char *dir, struct dirent *entry, char *in_path,
                       char *out_path) {
  const char *dot = strrchr(entry->d_name, '.');
  if (dot == NULL || dot == entry->d_name || strlen(dot) != 4 ||
      strcmp(dot, ".job")) {
    return 1;
  }

  if (strlen(entry->d_name) + strlen(dir) + 2 > MAX_JOB_FILE_NAME_SIZE) {
    fprintf(stderr, "%s/%s\n", dir, entry->d_name);
    return 1;
  }

  strcpy(in_path, dir);
  strcat(in_path, "/");
  strcat(in_path, entry->d_name);

  strcpy(out_path, in_path);
  strcpy(strrchr(out_path, '.'), ".out");

  return 0;
}

void *main_Thread(void *arg) {
    sessionRqst sessionRQST;

    while (1) {
        ssize_t n = read_all(f_server, (void *)&sessionRQST, sizeof(sessionRQST), NULL);
        if (n != sizeof(sessionRQST)) {
            exit(1);
        }

        if (sessionRQST.result == '0') {
            pthread_mutex_lock(&mutexBuffer);
            sem_wait(&semEmpty); // Espera que haja espaço no buffer
           
            // Adiciona o item ao buffer
            memcpy(&buf[count], &sessionRQST, sizeof(sessionRQST));
            count++;
            pthread_mutex_unlock(&mutexBuffer); // Sai da seção crítica
            sem_post(&semFull); // Indica que há um item disponível no buffer
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
int write_to_resp_pipe(const char *pipe_path, char op_code, char result) {
    int f_pipe;

    if ((f_pipe = open(pipe_path, O_WRONLY)) < 0) exit(1);

    char msg[MAX_STRING_SIZE];
    int msg_len = snprintf(msg, sizeof(msg), "%c%c", op_code, result);

    // Check if snprintf succeeded
    if (msg_len < 0 || (size_t)msg_len >= sizeof(msg)) {
        fprintf(stderr, "Error: message too large or formatting failed\n");
        close(f_pipe);
        return -1;
    }

    ssize_t n = write_all(f_pipe, msg, msg_len);
    if (n != msg_len) {
        perror("Error writing to named pipe");
        close(f_pipe);
        return -1;
    }

    close(f_pipe);

    return 0;
}


void *manager_thread(void *){
  sessionRqst sessionRQST;
  char req_pipe_path[MAX_BUFFER_SIZE+1], resp_pipe_path[MAX_BUFFER_SIZE+1], notif_pipe_path[MAX_BUFFER_SIZE+1];
  int f_req, f_resp, f_notif;

  pthread_mutex_lock(&mutexBuffer);
  int session_id = session_num;
  session_num++;
  pthread_mutex_unlock(&mutexBuffer);


  while(1){
    pthread_mutex_lock(&mutexBuffer);
    sem_wait(&semFull);
    memcpy(&sessionRQST, &buf[count], sizeof(sessionRQST));
    count--;
    pthread_mutex_unlock(&mutexBuffer);
    sem_post(&semEmpty);

    memset(req_pipe_path,0,MAX_BUFFER_SIZE+1);
    memset(resp_pipe_path,0,MAX_BUFFER_SIZE+1);
    memset(notif_pipe_path,0,MAX_BUFFER_SIZE+1);
    strncpy(req_pipe_path,sessionRQST.req_pipe_path,MAX_BUFFER_SIZE);
    strncpy(resp_pipe_path,sessionRQST.resp_pipe_path,MAX_BUFFER_SIZE);
    strncpy(notif_pipe_path,sessionRQST.notif_pipe_path,MAX_BUFFER_SIZE);

    if ((f_resp = open (resp_pipe_path, O_WRONLY)) < 0) exit(1);

    const char *message = "OP_CODE=1 | 1"; 
    ssize_t n = write_all(f_resp, message, strlen(message)); // Write the string to the pipe
    
    if (n != (ssize_t)strlen(message)) {
      exit(1);
    }

    if ((f_req = open (req_pipe_path, O_RDONLY)) < 0) exit(1);

    int result;
    int atending_client = 1;
    while(atending_client){

        switch(get_next(f_req)){

      
          case CMD_DISCONNECT:
              //DUVIDA: mudar estas variáveis globais!!!!?????????
              // sessionRqst buf[MAX_BUFFER_SIZE];
              // count;

              if(kvs_disconnect(key, f_resp,  f_notif, OP_CODE_SUBSCRIBE) != 0){
                result = 1;
              } else {
                result = 0;
              }
              n = write_to_resp_pipe(f_resp,OP_CODE_SUBSCRIBE,result);
              break;
          case CMD_SUBSCRIBE:

                if(kvs_subs_or_unsubs(key, f_resp,  f_notif, OP_CODE_SUBSCRIBE) != 0){
                result = 1;
              } else {
                result = 0;
              }
              n = write_to_resp_pipe(f_resp,OP_CODE_SUBSCRIBE,result);
              break;

          case CMD_UNSUBSCRIBE:
                if(kvs_subs_or_unsubs(key, f_resp,  f_notif, OP_CODE_UNSUBSCRIBE) != 0){
                result = 1;
              } else {
                result = 0;
              }
              n = write_to_resp_pipe(f_resp,OP_CODE_SUBSCRIBE,result);
              break;
        }
     }
    }
}

static int run_job(int in_fd, int out_fd, char *filename) {
  size_t file_backups = 0;
  while (1) {
    char keys[MAX_WRITE_SIZE][MAX_STRING_SIZE] = {0};
    char values[MAX_WRITE_SIZE][MAX_STRING_SIZE] = {0};
    unsigned int delay;
    size_t num_pairs;

    switch (get_next(in_fd)) {
    case CMD_WRITE:
      num_pairs =
          parse_write(in_fd, keys, values, MAX_WRITE_SIZE, MAX_STRING_SIZE);
      if (num_pairs == 0) {
        write_str(STDERR_FILENO, "Invalid command. See HELP for usage\n");
        continue;
      }

      if (kvs_write(num_pairs, keys, values)) {
        write_str(STDERR_FILENO, "Failed to write pair\n");
      }
      break;

    case CMD_READ:
      num_pairs =
          parse_read_delete(in_fd, keys, MAX_WRITE_SIZE, MAX_STRING_SIZE);

      if (num_pairs == 0) {
        write_str(STDERR_FILENO, "Invalid command. See HELP for usage\n");
        continue;
      }

      if (kvs_read(num_pairs, keys, out_fd)) {
        write_str(STDERR_FILENO, "Failed to read pair\n");
      }
      break;

    case CMD_DELETE:
      num_pairs =
          parse_read_delete(in_fd, keys, MAX_WRITE_SIZE, MAX_STRING_SIZE);

      if (num_pairs == 0) {
        write_str(STDERR_FILENO, "Invalid command. See HELP for usage\n");
        continue;
      }

      if (kvs_delete(num_pairs, keys, out_fd)) {
        write_str(STDERR_FILENO, "Failed to delete pair\n");
      }
      break;

    case CMD_SHOW:
      kvs_show(out_fd);
      break;

    case CMD_WAIT:
      if (parse_wait(in_fd, &delay, NULL) == -1) {
        write_str(STDERR_FILENO, "Invalid command. See HELP for usage\n");
        continue;
      }

      if (delay > 0) {
        printf("Waiting %d seconds\n", delay / 1000);
        kvs_wait(delay);
      }
      break;

    case CMD_BACKUP:
      pthread_mutex_lock(&n_current_backups_lock);
      if (active_backups >= max_backups) {
        wait(NULL);
      } else {
        active_backups++;
      }
      pthread_mutex_unlock(&n_current_backups_lock);
      int aux = kvs_backup(++file_backups, filename, jobs_directory);

      if (aux < 0) {
        write_str(STDERR_FILENO, "Failed to do backup\n");
      } else if (aux == 1) {
        return 1;
      }
      break;

    case CMD_INVALID:
      write_str(STDERR_FILENO, "Invalid command. See HELP for usage\n");
      break;

    case CMD_HELP:
      write_str(STDOUT_FILENO,
                "Available commands:\n"
                "  WRITE [(key,value)(key2,value2),...]\n"
                "  READ [key,key2,...]\n"
                "  DELETE [key,key2,...]\n"
                "  SHOW\n"
                "  WAIT <delay_ms>\n"
                "  BACKUP\n" // Not implemented
                "  HELP\n");

      break;

    case CMD_EMPTY:
      break;

    case EOC:
      printf("EOF\n");
      return 0;
    }
  }
}

// frees arguments
static void *get_file(void *arguments) {
  struct SharedData *thread_data = (struct SharedData *)arguments;
  DIR *dir = thread_data->dir;
  char *dir_name = thread_data->dir_name;

  if (pthread_mutex_lock(&thread_data->directory_mutex) != 0) {
    fprintf(stderr, "Thread failed to lock directory_mutex\n");
    return NULL;
  }

  struct dirent *entry;
  char in_path[MAX_JOB_FILE_NAME_SIZE], out_path[MAX_JOB_FILE_NAME_SIZE];
  while ((entry = readdir(dir)) != NULL) {
    if (entry_files(dir_name, entry, in_path, out_path)) {
      continue;
    }

    if (pthread_mutex_unlock(&thread_data->directory_mutex) != 0) {
      fprintf(stderr, "Thread failed to unlock directory_mutex\n");
      return NULL;
    }

    int in_fd = open(in_path, O_RDONLY);
    if (in_fd == -1) {
      write_str(STDERR_FILENO, "Failed to open input file: ");
      write_str(STDERR_FILENO, in_path);
      write_str(STDERR_FILENO, "\n");
      pthread_exit(NULL);
    }

    int out_fd = open(out_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (out_fd == -1) {
      write_str(STDERR_FILENO, "Failed to open output file: ");
      write_str(STDERR_FILENO, out_path);
      write_str(STDERR_FILENO, "\n");
      pthread_exit(NULL);
    }

    int out = run_job(in_fd, out_fd, entry->d_name);

    close(in_fd);
    close(out_fd);

    if (out) {
      if (closedir(dir) == -1) {
        fprintf(stderr, "Failed to close directory\n");
        return 0;
      }

      exit(0);
    }

    if (pthread_mutex_lock(&thread_data->directory_mutex) != 0) {
      fprintf(stderr, "Thread failed to lock directory_mutex\n");
      return NULL;
    }
  }

  if (pthread_mutex_unlock(&thread_data->directory_mutex) != 0) {
    fprintf(stderr, "Thread failed to unlock directory_mutex\n");
    return NULL;
  }

  pthread_exit(NULL);
}

static void dispatch_threads(DIR *dir) {
  pthread_t *threads = malloc(max_threads * sizeof(pthread_t));

  if (threads == NULL) {
    fprintf(stderr, "Failed to allocate memory for threads\n");
    return;
  }

  struct SharedData thread_data = {dir, jobs_directory,
                                   PTHREAD_MUTEX_INITIALIZER};

  for (size_t i = 0; i < max_threads; i++) {
    if (pthread_create(&threads[i], NULL, get_file, (void *)&thread_data) !=
        0) {
      fprintf(stderr, "Failed to create thread %zu\n", i);
      pthread_mutex_destroy(&thread_data.directory_mutex);
      free(threads);
      return;
    }
  }

  // ler do FIFO de registo

  for (unsigned int i = 0; i < max_threads; i++) {
    if (pthread_join(threads[i], NULL) != 0) {
      fprintf(stderr, "Failed to join thread %u\n", i);
      pthread_mutex_destroy(&thread_data.directory_mutex);
      free(threads);
      return;
    }
  }

  if (pthread_mutex_destroy(&thread_data.directory_mutex) != 0) {
    fprintf(stderr, "Failed to destroy directory_mutex\n");
  }

  free(threads);
}

int main(int argc, char **argv) {
  if (argc < 5) {
    write_str(STDERR_FILENO, "Usage: ");
    write_str(STDERR_FILENO, argv[0]);
    write_str(STDERR_FILENO, " <jobs_dir>");
    write_str(STDERR_FILENO, " <max_threads>");
    write_str(STDERR_FILENO, " <max_backups> \n");
    write_str(STDERR_FILENO, "<nome_do_FIFO_de_registo");
    return 1;
  }

  jobs_directory = argv[1];

  char *endptr;
  max_backups = strtoul(argv[3], &endptr, 10);

  if (*endptr != '\0') {
    fprintf(stderr, "Invalid max_proc value\n");
    return 1;
  }

  max_threads = strtoul(argv[2], &endptr, 10);

  if (*endptr != '\0') {
    fprintf(stderr, "Invalid max_threads value\n");
    return 1;
  }

  if (max_backups <= 0) {
    write_str(STDERR_FILENO, "Invalid number of backups\n");
    return 0;
  }

  if (max_threads <= 0) {
    write_str(STDERR_FILENO, "Invalid number of threads\n");
    return 0;
  }

  if (kvs_init()) {
    write_str(STDERR_FILENO, "Failed to initialize KVS\n");
    return 1;
  }

  DIR *dir = opendir(argv[1]);
  if (dir == NULL) {
    fprintf(stderr, "Failed to open directory: %s\n", argv[1]);
    return 0;
  }

  dispatch_threads(dir);

  if (closedir(dir) == -1) {
    fprintf(stderr, "Failed to close directory\n");
    return 0;
  }

  while (active_backups > 0) {
    wait(NULL);
    active_backups--;
  }

    unlink(argv[4]);
    if (mkfifo (argv[4], 0777) < 0)
    exit (1);

  if ((f_server = open (argv[4], O_RDWR)) < 0) exit(1);

  pthread_mutex_init(&mutexBuffer, NULL);
  sem_init(&semEmpty, 0, MAX_SESSION_COUNT);
  sem_init(&semFull, 0, 0);


  pthread_t main_thread;
  pthread_t manager_thread[MAX_SESSION_COUNT];
  pthread_t notif_thread[MAX_SESSION_COUNT];
  if (pthread_create(&main_thread, NULL, main_Thread, NULL) != 0) {
      fprintf(stderr, "Erro ao criar thread");
      exit(EXIT_FAILURE);
  }

  for (int i = 0; i< MAX_SESSION_COUNT; i++){
    if (pthread_create(&manager_thread[i], NULL, manager_thread, &i) != 0) {
      fprintf(stderr, "Erro ao criar thread");
      exit(EXIT_FAILURE);
    }
  }

  pthread_join(main_thread, NULL);

  for (int i = 0; i < MAX_SESSION_COUNT; i++){
    pthread_join(manager_thread[i], NULL);
  }

  

  kvs_terminate();

// COMEJADSFAJSDFÇKADSFÇASJDFASDJFOADSJFAJD

  return 0;
}
