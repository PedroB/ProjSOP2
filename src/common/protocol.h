#ifndef COMMON_PROTOCOL_H
#define COMMON_PROTOCOL_H
#define MAX_PIPE_PATH_LENGTH 40
// Opcodes for client-server communication
// estes opcodes sao usados num switch case para determinar o que fazer com a
// mensagem recebida no server usam estes opcodes tambem nos clientes quando
// enviam mensagens para o server
enum {
  OP_CODE_CONNECT = '1',
  OP_CODE_DISCONNECT = '2',
  OP_CODE_SUBSCRIBE = '3',
  OP_CODE_UNSUBSCRIBE = '4'
  // TODO mais opcodes para cada operacao
};

typedef struct {
    // int session_id;
    // char result;
    char req_pipe_path[MAX_PIPE_PATH_LENGTH];
    char resp_pipe_path[MAX_PIPE_PATH_LENGTH];
    char notif_pipe_path[MAX_PIPE_PATH_LENGTH];
    // char key[MAX_STRING_SIZE];
} sessionRqst;

typedef struct {
    char opcode;
    char req_pipe_path[MAX_PIPE_PATH_LENGTH];
    char resp_pipe_path[MAX_PIPE_PATH_LENGTH];
    char notif_pipe_path[MAX_PIPE_PATH_LENGTH];
} sessionProtoMessage;


typedef struct {
    char opcode;
    char key[MAX_STRING_SIZE];
} subRqst;




typedef struct {
    char key[MAX_STRING_SIZE];
    char value[MAX_STRING_SIZE];


} notifMessage;


#endif // COMMON_PROTOCOL_H
