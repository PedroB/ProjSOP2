#include "kvs.h"
#include "constants.h"

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <src/common/io.h>
#include <unistd.h>

#include "string.h"
#include <sys/fcntl.h>

extern char buf[MAX_BUFFER_SIZE]; // Reference to global variable from main.c
extern 

// Hash function based on key initial.
// @param key Lowercase alphabetical string.
// @return hash.
// NOTE: This is not an ideal hash function, but is useful for test purposes of
// the project
int hash(const char *key) {
  int firstLetter = tolower(key[0]);
  if (firstLetter >= 'a' && firstLetter <= 'z') {
    return firstLetter - 'a';
  } else if (firstLetter >= '0' && firstLetter <= '9') {
    return firstLetter - '0';
  }
  return -1; // Invalid index for non-alphabetic or number strings
}

struct HashTable *create_hash_table() {
  HashTable *ht = malloc(sizeof(HashTable));
  if (!ht)
    return NULL;
  for (int i = 0; i < TABLE_SIZE; i++) {
    ht->table[i] = NULL;
  }
  pthread_rwlock_init(&ht->tablelock, NULL);
  return ht;
}


///////////////////////////////////////////////////////////////////////////////////
int write_to_notif_pipe(int f_pipe, int is_deleted, const char *key, const char *value) {
    
    char msg[MAX_STRING_SIZE];
    int msg_len = -1; // Inicialização para evitar avisos

    if (is_deleted == DELETED) {
        msg_len = snprintf(msg, sizeof(msg), "(%s, DELETED)", key);
    } else {
        msg_len = snprintf(msg, sizeof(msg), "(%s, %s)", key, value);
    }

    // Check if snprintf succeeded
    // if (msg_len < 0 || (size_t)msg_len >= sizeof(msg)) {
    //     fprintf(stderr, "Error formatting message\n");
    //     close(f_pipe);
    //     exit(1);
    // }

    // ssize_t n = write_all(f_pipe, msg, 83); // Conversão explícita
    write_all(f_pipe, msg, 83); // Conversão explícita

    // if (n != msg_len) {
    //     fprintf(stderr, "Failed to write to pipe\n");
    //     close(f_pipe);
    //     exit(1);
    // }

    close(f_pipe);

    return 0;
}

///////////////////////////////////////////////////////////////////////////////////

void notify_all_pipes(int is_deleted, NotifPipeNode *head, const char *key, const char *value) {
    NotifPipeNode *current = head;

    while (current != NULL) {
        if (is_deleted == DELETED) {
            write_to_notif_pipe(current->notif_pipe, DELETED, key, value);
        } else if (is_deleted == NOT_DELETED) {
            write_to_notif_pipe(current->notif_pipe, NOT_DELETED, key, value);
        }

        // Move to the next notification pipe
        current = current->next;
    }
}



///////////////////////////////////////////////////////////////////////////////////
int write_pair(HashTable *ht, const char *key, const char *value) {
  int index = hash(key);

  // Search for the key node
  KeyNode *keyNode = ht->table[index];
  KeyNode *previousNode;

  while (keyNode != NULL) {
    if (strcmp(keyNode->key, key) == 0) {
      // overwrite value
      free(keyNode->value);
      keyNode->value = strdup(value);
      return 0;
    }
    previousNode = keyNode;
    keyNode = previousNode->next; // Move to the next node
  }
  // Key not found, create a new key node
  keyNode = malloc(sizeof(KeyNode));
  keyNode->key = strdup(key);       // Allocate memory for the key
  keyNode->value = strdup(value);   // Allocate memory for the value
  

  keyNode->next = ht->table[index]; // Link to existing nodes
  ht->table[index] = keyNode; // Place new key node at the start of the list

  //iterar pela notif_pipes list e fazer
  notify_all_pipes(NOT_DELETED, keyNode->notif_pipes_head, keyNode->key, keyNode->value);

  // write_to_notif_pipe(pipe_path, NOT_DELETED, int key, int value);

  return 0;
}

char *read_pair(HashTable *ht, const char *key) {
  int index = hash(key);

  KeyNode *keyNode = ht->table[index];
  KeyNode *previousNode;
  char *value;

  while (keyNode != NULL) {
    if (strcmp(keyNode->key, key) == 0) {
      value = strdup(keyNode->value);
      return value; // Return the value if found
    }
    previousNode = keyNode;
    keyNode = previousNode->next; // Move to the next node
  }

  return NULL; // Key not found
}


void free_notif_pipes(NotifPipeNode *head) {
    NotifPipeNode *current = head;
    while (current != NULL) {
        NotifPipeNode *temp = current;
        current = current->next;
        free(temp); // Free the current node
    }
}

int delete_pair(HashTable *ht, const char *key) {
  int index = hash(key);

  // Search for the key node
  KeyNode *keyNode = ht->table[index];
  KeyNode *prevNode = NULL;

  while (keyNode != NULL) {
    if (strcmp(keyNode->key, key) == 0) {
      // Key found; delete this node
      if (prevNode == NULL) {
        // Node to delete is the first node in the list
        ht->table[index] =
            keyNode->next; // Update the table to point to the next node
      } else {
        // Node to delete is not the first; bypass it
        prevNode->next =
            keyNode->next; // Link the previous node to the next node
      }

      notify_all_pipes(DELETED, keyNode->notif_pipes_head, keyNode->key, keyNode->value);

      // Free the memory allocated for the key and value
      free(keyNode->key);
      free(keyNode->value);
      free_notif_pipes(keyNode->notif_pipes_head);
      free(keyNode); // Free the key node itself
      return 0;      // Exit the function
    }
    prevNode = keyNode;      // Move prevNode to current node
    keyNode = keyNode->next; // Move to the next node
  }

  return 1;
}

void free_table(HashTable *ht) {
  for (int i = 0; i < TABLE_SIZE; i++) {
    KeyNode *keyNode = ht->table[i];
    while (keyNode != NULL) {
      KeyNode *temp = keyNode;
      keyNode = keyNode->next;
      free(temp->key);
      free(temp->value);
      free(temp);
    }
  }
  pthread_rwlock_destroy(&ht->tablelock);
  free(ht);
}

// NotifPipeNode *find_notif_pipe(NotifPipeNode *head_node, const char *notif_pipe) {
//     if (!head_node || !notif_pipe) {
//         return NULL; // Retorna NULL se o head_node ou o notif_pipe forem inválidos
//     }

//     NotifPipeNode *current = head_node;

//     // Iterar pela lista encadeada para localizar o pipe
//     while (current != NULL) {
//         if (current->notif_pipe == notif_pipe) {
//             return current; // Retorna o nó se encontrar o pipe correspondente
//         }
//         current = current->next; // Avançar para o próximo nó
//     }

//     return NULL; // Retorna NULL se não encontrar o pipe
// }



int add_notif_pipe(NotifPipeNode **head_node, int notif_pipe) {
    if (!head_node || !notif_pipe) {
        return 1; // Erro: Entrada inválida
    }

    // Criar um novo nó
    NotifPipeNode *new_node = malloc(sizeof(NotifPipeNode));
    if (!new_node) {
        return -1;
    }

    // Alocar e copiar a string do pipe de notificação
    new_node->notif_pipe = notif_pipe;
    if (!new_node->notif_pipe) {
        free(new_node);
        return 1;
    }

    // Inserir o novo nó no início da lista
    new_node->next = *head_node;
    *head_node = new_node;

    return 0;
}


///////////////////////////////////////////////////////////////////////////////
int remove_notif_pipe(NotifPipeNode **head_node, int notif_pipe) {
    if (!head_node || !notif_pipe) {
        return 1; 
    }

    NotifPipeNode *current = *head_node;
    NotifPipeNode *previous = NULL;

    // Iterar pela lista
    while (current) {
        if (current->notif_pipe == notif_pipe) {
            // Atualizar os ponteiros para remover o nó
            if (previous) {
                previous->next = current->next;
            } else {
                *head_node = current->next;
            }

            // Liberar memória do nó removido
            free(current); // Apenas o nó é liberado
            return 0; // Sucesso
        }

        // Avançar para o próximo nó
        previous = current;
        current = current->next;
    }

    return 1; // Pipe não encontrado
}



///////////////////////////////////////////////////////////////////////////////
int execute_subscribe(HashTable *ht, const char *key, const int notif_pipe) {
    if (!key || !notif_pipe || !ht) {
        fprintf(stderr, "Erro: Parâmetros inválidos.\n");
        return 1;
    }

    int index = hash(key); // Calcular o índice na tabela hash
    KeyNode *keyNode = ht->table[index];

    // Buscar pela chave no hash table
    while (keyNode != NULL) {
        if (strcmp(keyNode->key, key) == 0) {
            // Encontrou a chave, verifica se o pipe já está registrado
            

            // Adicionar o pipe à lista de notificação
            if (add_notif_pipe(&keyNode->notif_pipes_head, notif_pipe) != 0) {
               
                return 1;
            }

            return 0;
        }

        keyNode = keyNode->next; // Avançar para o próximo KeyNode
    }

    return 1;
}


///////////////////////////////////////////////////////////////////////////////
int execute_unsubscribe(HashTable *ht, const char *key, const int notif_pipe) {
    if (!key || !notif_pipe || !ht) {
        fprintf(stderr, "Erro: Parâmetros inválidos.\n");
        return 1;
    }

    int index = hash(key); // Calcular o índice na tabela hash
    KeyNode *keyNode = ht->table[index];

    // Percorrer a lista de KeyNode
    while (keyNode != NULL) {
        if (strcmp(keyNode->key, key) == 0) {
            // Encontrou a chave, busca pelos pipes de notificação

            // Remover o pipe de notificação
            if (remove_notif_pipe(&keyNode->notif_pipes_head, notif_pipe) != 0) {
                return 1;
            }

            return 0;
        }

        keyNode = keyNode->next; // Avançar para o próximo KeyNode na lista
    }


    return 1;
}


///////////////////////////////////////////////////////////////////////////////////
int execute_disconnect( HashTable *ht,const int notif_pipe) {
    if (!notif_pipe || !ht) {
        return 1;
    }

    // Iterar por todas as entradas na tabela hash
    for (int i = 0; i < TABLE_SIZE; i++) {
        KeyNode *keyNode = ht->table[i];

        // Iterar por cada KeyNode na lista 
        while (keyNode != NULL) {
            NotifPipeNode **current = &keyNode->notif_pipes_head;

            // Iterar pela lista de NotifPipeNode
            while (*current != NULL) {
                if ((*current)->notif_pipe == notif_pipe) {
                    // Pipe encontrado, remover o nó
                    NotifPipeNode *to_remove = *current;
                    *current = to_remove->next; // Atualizar o ponteiro para pular o nó
                    
                    free(to_remove);

                    
                } else {
                    current = &((*current)->next); // Avançar para o próximo nó
                }
            }

            keyNode = keyNode->next; // Avançar para o próximo KeyNode na lista
        }
    }

    return 0;
}
