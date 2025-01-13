#include "kvs.h"
#include "constants.h"

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <src/common/io.h>
#include <unistd.h>

#include "string.h"
#include <sys/fcntl.h>
#include <src/common/protocol.h>


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
int write_to_notif_pipe(int f_notif, int is_deleted, const char *key, const char *value) {
    
    notifMessage notification;
    strcpy(notification.key, key);

    if (is_deleted == DELETED) {
     
    memset(notification.value, 0, sizeof(notification.value)); // Limpar o campo 'val'
    // Copiar "DELETED" para notif.val, garantindo o limite do buffer
    strncpy(notification.value, "DELETED", sizeof(notification.value) - 1);

        
    } else {
        strcpy(notification.value, value);
    }

      write_all(f_notif, (void *)&notification, sizeof(notification));
      
    return 0;
}

///////////////////////////////////////////////////////////////////////////////////

void notify_all_pipes(int is_deleted, NotifPipeNode *head, const char *key, const char *value) {
    NotifPipeNode *current = head;


    while (current) {
        if (is_deleted == 0) {

            write_to_notif_pipe(current->notif_pipe, DELETED, key, value);
        } else if (is_deleted == 1) {
            write_to_notif_pipe(current->notif_pipe, NOT_DELETED, key, value);
        }
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

      notify_all_pipes(1, keyNode->notif_pipes_head, keyNode->key, keyNode->value);

      return 0;
    }
    previousNode = keyNode;
    keyNode = previousNode->next; // Move to the next node
  }
  // Key not found, create a new key node
  keyNode = malloc(sizeof(KeyNode));
  keyNode->key = strdup(key);       // Allocate memory for the key
  keyNode->value = strdup(value);   // Allocate memory for the value
  
  keyNode->notif_pipes_head = NULL;    // Head of the linked list for notification pipes

  keyNode->next = ht->table[index]; // Link to existing nodes
  ht->table[index] = keyNode; // Place new key node at the start of the list

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

      notify_all_pipes(0, keyNode->notif_pipes_head, keyNode->key, keyNode->value);

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

////////////////////////////////////////////////////////
int add_notif_pipe(NotifPipeNode **head_node, int notif_pipe) {

    // Create a new node
    NotifPipeNode *new_node = malloc(sizeof(NotifPipeNode));
    if (!new_node) {
        // Error in memory allocation
        return -1;
    }

    // Assign the notif_pipe value to the new node
    new_node->notif_pipe = notif_pipe;

    // Insert the new node at the beginning of the list
    new_node->next = *head_node;
    *head_node = new_node;


    return 0;
}

///////////////////////////////////////////////////////////////////////////
int remove_notif_pipe(NotifPipeNode **head_node, int notif_pipe) {
    if (!head_node || !*head_node) {
        // If the head_node or the list is NULL, nothing to remove
        return -1;
    }
    
    // Pointer to the current node
    NotifPipeNode *current = *head_node;

    // Check if the first node matches
    if (current->notif_pipe == notif_pipe) {
        // Remove the first node
        *head_node = current->next;
        free(current); // Free memory
        return 0; // Exit immediately after freeing
    }

    // Traverse the list to find the matching node
    while (current->next != NULL) {
        if (current->next->notif_pipe == notif_pipe) {
            // Found the node to remove
            NotifPipeNode *to_remove = current->next;
            current->next = to_remove->next; // Bypass the node
            free(to_remove); // Free memory
            return 0; // Exit after freeing
        }
        current = current->next; // Move to the next node
    }

    // If we reach here, the pipe wasn't found in the list
    return 1; // Failure
}
///////////////////////////////////////////////////////////////////////////////
int execute_subscribe(HashTable *ht, const char *key, const int notif_pipe) {
   
    if (!key) {
            fprintf(stderr, "Erro: Parâmetros inválidos.\n");
            return 1;
        }

    int index = hash(key); // Calcular o índice na tabela hash
    KeyNode *keyNode = ht->table[index];

    // Buscar pela chave no hash table
    while (keyNode != NULL) {
        if (strcmp(keyNode->key, key) == 0) {
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

    int index = hash(key); 
    KeyNode *keyNode = ht->table[index];
    // Percorrer a lista de KeyNode
    while (keyNode != NULL) {
        if (strcmp(keyNode->key, key) == 0) {
            // Encontrou a chave, busca pelos pipes de notificação
            if (remove_notif_pipe(&keyNode->notif_pipes_head, notif_pipe) != 0) {
                return 1;
            }

            return 0;
        }

        keyNode = keyNode->next; 
    }

    return 1;
}

//////////////////////////////////////////////////////////////////////////////////
int execute_disconnect( HashTable *ht,const int notif_pipe) {
  if (!notif_pipe || !ht) {
      return 1;
  }

  // Iterar por todas as entradas na tabela hash
  for (int i = 0; i < TABLE_SIZE; i++) {
      KeyNode *keyNode = ht->table[i];
      // Iterar por cada KeyNode na lista 
      if(keyNode == NULL) {
        break;
      }
          int result = remove_notif_pipe(&keyNode->notif_pipes_head, notif_pipe);

          if (result == 0) {
            keyNode = keyNode->next; // Avançar para o próximo KeyNode na lista
          } else {
            return 1;
          }
      }
      return 0;
}               
