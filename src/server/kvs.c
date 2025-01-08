#include "kvs.h"

#include <ctype.h>
#include <stdlib.h>

#include "string.h"

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

int write_pair(HashTable *ht, const char *key, const char *value, const char *notif_pipe) {
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
  
  keyNode->notif_pipes[count] = strdup(notif_pipe);

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
      // Free the memory allocated for the key and value
      free(keyNode->key);
      free(keyNode->value);
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

///////////////////////////////////////////////////////////////////////////////
NotifPipeNode find_notif_pipe(NotifPipeNode head_node, const char *notif_pipe) {


  //give the sessionRQST.notif_pipe_head, finds the node that corresponds to that sessionRQST_pipe_path
  // to later give to the remove_notif_pipe, so that it removes the specific node

  return node;
}


///////////////////////////////////////////////////////////////////////////////
int add_notif_pipe(NotifPipeNode *node, const char *notif_pipe) {
    if (!node || !notif_pipe) {
        return -1; // Error: Invalid input
    }

    // Create a new NotifPipeNode
    NotifPipeNode *new_node = malloc(sizeof(NotifPipeNode));
    if (!new_node) {
        perror("Error allocating memory for NotifPipeNode");
        return -1;
    }

    // Allocate and copy the notif_pipe string
    new_node->notif_pipe = strdup(notif_pipe);
    if (!new_node->notif_pipe) {
        perror("Error allocating memory for notif_pipe string");
        free(new_node);
        return -1;
    }

    // Insert the new node at the head of the list
    new_node->next = node->notif_pipes_head;
    node->notif_pipes_head = new_node;

    return 0; // Success
}

///////////////////////////////////////////////////////////////////////////////
int remove_notif_pipe(NotifPipeNode *node, const char *notif_pipe) {
    if (!node || !notif_pipe) {
        return -1;
    }

    NotifPipeNode *current = node->notif_pipes_head;
    NotifPipeNode *previous = NULL;

    while (current) {
        if (strcmp(current->notif_pipe, notif_pipe) == 0) {
            // Match found: remove the node
            if (previous) {
                previous->next = current->next;
            } else {
                node->notif_pipes_head = current->next;
            }

            free(current->notif_pipe);
            free(current);
            return 0; 
        }

        // Move to the next node
        previous = current;
        current = current->next;
    }

    return -1;
}
///////////////////////////////////////////////////////////////////////////////
int write_to_named_pipe(const char *pipe_path, int op_code, const char *key) {
    int f_pipe;

    if ((f_pipe = open(pipe_path, O_WRONLY)) < 0) exit(1);

    char msg[MAX_STRING_SIZE];
    int msg_len = snprintf(msg, sizeof(msg), "%d|%s", op_code, key);

    // Check if snprintf succeeded
    if (msg_len < 0 || (size_t)msg_len >= sizeof(msg)) {
        fprintf(stderr, "Error: message too large or formatting failed\n");
        close(f_pipe);
        return -1;
    }

    ssize_t n = write(f_pipe, msg, msg_len);
    if (n != msg_len) {
        perror("Error writing to named pipe");
        close(f_pipe);
        return -1;
    }

    close(f_pipe);

    return 0;
}
///////////////////////////////////////////////////////////////////////////////
int execute_subscribe(const char key) {

  KeyNode *keyNode = ht->table[index];
  KeyNode *previousNode;

  while (keyNode != NULL) {
    if (strcmp(keyNode->key, key) == 0) {
     
      notif_head = keyNode->notif_pipes_head;

      NotifPipeNode node = find_notif_pipe(notif_head);
      if(add_notif_pipe(node, notif_pipe) != 0) {
        //error
      };

      return 0;
    }
    previousNode = keyNode;
    keyNode = previousNode->next;
  }

  write_to_named_pipe(sessionRQST.resp_pipe_path, OP_CODE_SUBSCRIBE, key);

  return 0;
}

///////////////////////////////////////////////////////////////////////////////
int execute_unsubscribe() {

  KeyNode *keyNode = ht->table[index];
  KeyNode *previousNode;

  while (keyNode != NULL) {
    if (strcmp(keyNode->key, key) == 0) {
     
      notif_head = keyNode->notif_pipes_head;
      KeyNode node = find_notif_pipe();

      if(remove_notif_pipe(node, notif_pipe) != 0) {
        //error
      };

      return 0;
    }
    previousNode = keyNode;
    keyNode = previousNode->next;
  }

  write_to_named_pipe(sessionRQST.resp_pipe_path, OP_CODE_UNSUBSCRIBE, key);

  return 0;
}

///////////////////////////////////////////////////////////////////////////////////
int execute_disconnect() {

  //the server's job is just to remove the client subscription (the notif pipe) in every key in the table it is in

  //iterate through the kvs table and if the notif_pipe == keyNNotifPipeNode, then  remove_node 

}