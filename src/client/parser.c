#include "parser.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <src/common/protocol.h>

#include "src/common/constants.h"

// Reads a string and indicates the position from where it was
// extracted, based on the KVS specification.
// @param fd File to read from.
// @param buffer To write the string in.
// @param max Maximum string size.
static int read_string(int fd, char *buffer, size_t max) {
  ssize_t bytes_read;
  char ch;
  size_t i = 0;
  int value = -1;

  while (i < max) {
    bytes_read = read(fd, &ch, 1);

    if (bytes_read <= 0) {
      return -1;
    }

    if (ch == ' ') {
      return -1;
    }

    if (ch == ',') {
      value = 0;
      break;
    } else if (ch == ')') {
      value = 1;
      break;
    } else if (ch == ']') {
      value = 2;
      break;
    }

    buffer[i++] = ch;
  }

  buffer[i] = '\0';

  return value;
}

// Reads a number and stores it in an unsigned integer
// variable.
// @param fd File to read from.
// @param value To store the number in.
// @param next Will point to the character succeding the number.
static int read_uint(int fd, unsigned int *value, char *next) {
  char buf[16];

  int i = 0;
  while (1) {
    if (read(fd, buf + i, 1) == 0) {
      *next = '\0';
      break;
    }

    *next = buf[i];

    if (buf[i] > '9' || buf[i] < '0') {
      buf[i] = '\0';
      break;
    }

    i++;
  }

  unsigned long ul = strtoul(buf, NULL, 10);

  if (ul > UINT_MAX) {
    return 1;
  }

  *value = (unsigned int)ul;

  return 0;
}

// Jumps file descriptor to next line.
// @param fd File descriptor.
static void cleanup(int fd) {
  char ch;
  while (read(fd, &ch, 1) == 1 && ch != '\n')
    ;
}

enum Command get_next(int fd) {
  char buf[16];
  if (read(fd, buf, 1) != 1) {
    return EOC;
  }

  switch (buf[0]) {
  case 'S':
    if (read(fd, buf + 1, 9) != 9 || strncmp(buf, "SUBSCRIBE ", 10) != 0) {
      cleanup(fd);
      return CMD_INVALID;
    }

    return CMD_SUBSCRIBE;

  case 'U':
    if (read(fd, buf + 1, 11) != 11 || strncmp(buf, "UNSUBSCRIBE ", 12) != 0) {
      cleanup(fd);
      return CMD_INVALID;
    }

    return CMD_UNSUBSCRIBE;

  case 'D':
    if (read(fd, buf + 1, 5) != 5 || strncmp(buf, "DELAY ", 6) != 0) {
      if (read(fd, buf + 6, 4) != 4 || strncmp(buf, "DISCONNECT", 10) != 0) {
        cleanup(fd);
        return CMD_INVALID;
      }
      if (read(fd, buf + 10, 1) != 0 && buf[10] != '\n') {
        cleanup(fd);
        return CMD_INVALID;
      }
      return CMD_DISCONNECT;
    }

    return CMD_DELAY;

  case '#':
    cleanup(fd);
    return CMD_EMPTY;

  case '\n':
    return CMD_EMPTY;

  default:
    cleanup(fd);
    return CMD_INVALID;
  }
}

size_t parse_list(int fd, char keys[][MAX_STRING_SIZE], size_t max_keys,
                  size_t max_string_size) {
  char ch;

  if (read(fd, &ch, 1) != 1 || ch != '[') {
    cleanup(fd);
    return 0;
  }

  size_t num_keys = 0;
  int output = 2;
  char key[max_string_size];
  while (num_keys < max_keys) {
    output = read_string(fd, key, max_string_size);
    if (output < 0 || output == 1) {
      cleanup(fd);
      return 0;
    }

    strcpy(keys[num_keys++], key);

    if (output == 2) {
      break;
    }
  }

  if (num_keys == max_keys && output != 2) {
    cleanup(fd);
    return 0;
  }

  if (read(fd, &ch, 1) != 1 || (ch != '\n' && ch != '\0')) {
    cleanup(fd);
    return 0;
  }

  return num_keys;
}

int parse_delay(int fd, unsigned int *delay) {
  char ch;

  if (read_uint(fd, delay, &ch) != 0) {
    cleanup(fd);
    return -1;
  }

  return 0;
}

// int parse_key(int fd, char *key, size_t max_size) {
//     char buffer[max_size];

//     // Ler do descritor de arquivo
//     ssize_t bytes_read = read(fd, buffer, max_size - 1);
//     if (bytes_read <= 0) {
//         return 1; // Erro ou nada lido
//     }

//     buffer[bytes_read] = '\0'; // Garantir terminação da string

//     // Procurar por parênteses e extrair a chave
//     char *start = strchr(buffer, '[');
//     char *end = strchr(buffer, ']');

//     if (!start || !end || start >= end) {
//         return 1; // Parênteses inválidos ou chave ausente
//     }

//     // Copiar apenas a chave entre os parênteses
//     size_t key_length = end - start - 1;
//     if (key_length >= max_size) {
//         return 1; // A chave excede o tamanho permitido
//     }

//     strncpy(key, start + 1, key_length);
//     key[key_length] = '\0'; // Adicionar terminação da string

//     return 0; // Sucesso
// }



/////////////////////////////////
int parse_key(int fd, char *key, size_t max_size) {
    char buffer[max_size];

    // Read from the file descriptor
    ssize_t bytes_read = read(fd, buffer, max_size - 1);
    if (bytes_read <= 0) {
        return 1; // Error or nothing read
    }

    buffer[bytes_read] = '\0'; // Null-terminate the string

    // Look for brackets and extract the key
    char *start = strchr(buffer, '[');
    char *end = strchr(buffer, ']');

    if (!start || !end || start >= end) {
        return 1; // Invalid brackets or missing key
    }

    // Calculate key length
    ptrdiff_t key_length_signed = end - start - 1;
    if (key_length_signed < 0) {
        return 1; // Invalid key length
    }
    size_t key_length = (size_t)key_length_signed;

    // Ensure the key fits within max_size
    if (key_length >= max_size) {
        return 1; // Key exceeds allowed size
    }

    // Copy the key and null-terminate it
    strncpy(key, start + 1, key_length);
    key[key_length] = '\0';

    return 0; // Success
}
