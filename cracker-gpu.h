#ifndef CGPU_H
#define CGPU_H
#define MD5_DIGEST_LENGTH 16
#include <string.h>
#include <stdint.h>



#define MAX_USERNAME_LENGTH 64
#define PASSWORD_LENGTH 7

#define numBucketsAndMask 128 // Needs to be at least as large as the max # of players

// /**
//  * This struct is the root of the data structure that will hold users and hashed passwords.
//  * There are a number of buckets, each with a linked list.
//  * Instead of trying to be fancy, we just used a mask and a bitwise AND to get a psuedo-random assignment that's also super fast
//  */
// typedef struct password_set {
//   // Our buckets
//   struct password_set_node* buckets[numBucketsAndMask + 1];

//   // Keeping track of the # of passwords in this set.
//   int numPasswords;
// } password_set_t;

// Each node in each bucket keeps the username, hash, and references to make deletion faster
typedef struct password_set_node {
  char username[MAX_USERNAME_LENGTH];
  uint8_t hashed_password [MD5_DIGEST_LENGTH];
  char solved_password[PASSWORD_LENGTH];
} password_set_node_t;

int md5_string_to_bytes(const char* md5_string, uint8_t* bytes);

void crack_password_list(password_set_node_t* argsPasswords, size_t numPasswordsArg, int index, int numUsers, int host_fd);

#endif