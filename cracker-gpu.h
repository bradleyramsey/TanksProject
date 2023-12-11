#ifndef CGPU_H
#define CGPU_H
#define MD5_DIGEST_LENGTH 16
#include <string.h>
#include <stdint.h>


#define MAX_USERNAME_LENGTH 64
#define PASSWORD_LENGTH 7

#define numBucketsAndMask 127 // Needs to be at least as large as the max # of players


// Each node in each bucket keeps the username, hash, and references to make deletion faster
typedef struct password_set_node {
  char username[MAX_USERNAME_LENGTH];
  uint8_t hashed_password [MD5_DIGEST_LENGTH];
  char solved_password[PASSWORD_LENGTH];
} password_set_node_t;


void crack_password_list(password_set_node_t* argsPasswords, size_t numPasswordsArg, int index, int numUsers, int host_fd);

#endif