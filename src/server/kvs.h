#ifndef KEY_VALUE_STORE_H
#define KEY_VALUE_STORE_H
#define TABLE_SIZE 26

#include <pthread.h>
#include <stddef.h>
#include "constants.h"

typedef struct NotifPipeNode {
    int notif_pipe;                   // The notification pipe string
    struct NotifPipeNode *next;         // Pointer to the next node in the list
} NotifPipeNode;

typedef struct KeyNode {
    char *key;                          // Key string
    char *value;                        // Value string
    NotifPipeNode *notif_pipes_head;    // Head of the linked list for notification pipes
    struct KeyNode *next;               // Pointer to the next KeyNode in the list
} KeyNode;

typedef struct HashTable {
  KeyNode *table[TABLE_SIZE];
  pthread_rwlock_t tablelock;
} HashTable;

/// Creates a new KVS hash table.
/// @return Newly created hash table, NULL on failure
struct HashTable *create_hash_table();

int hash(const char *key);

// Writes a key value pair in the hash table.
// @param ht The hash table.
// @param key The key.
// @param value The value.
// @return 0 if successful.
int write_pair(HashTable *ht, const char *key, const char *value);

// Reads the value of a given key.
// @param ht The hash table.
// @param key The key.
// return the value if found, NULL otherwise.
char *read_pair(HashTable *ht, const char *key);

/// Deletes a pair from the table.
/// @param ht Hash table to read from.
/// @param key Key of the pair to be deleted.
/// @return 0 if the node was deleted successfully, 1 otherwise.
int delete_pair(HashTable *ht, const char *key);

/// Frees the hashtable.
/// @param ht Hash table to be deleted.
void free_table(HashTable *ht);

int execute_disconnect(HashTable *ht,const int notif_pipe);

int execute_unsubscribe(HashTable *ht,const char *key, const int notif_pipe);

int execute_subscribe(HashTable *ht, const char *key, const int notif_pipe);
#endif // KVS_H
