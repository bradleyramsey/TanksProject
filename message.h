#pragma once

#define MAX_MESSAGE_LENGTH 2048

#include <stdint.h>
#include "tank.h"
#include "cracker-gpu.h"

// typedef struct {
//   char* username;
//   char* message;
// } packet_t;

typedef struct {
  char* username;
  uint8_t* passwordHash;
} init_packet_t;

typedef struct {
  int playerNum;
  char* hostname;
  int port;
  int index;
  int numUsers;
} start_packet_t;

typedef struct {
  int* status;
  int* board;
} screen_packet_t;

// Send a across a socket with a header that includes the message length. Returns non-zero value if
// an error occurs.
int send_init(int fd, const char* username, const uint8_t* passwordHash);
int send_assignment(int fd, const int* playerNum, const int* partnerPort);
int send_start(int fd, const int playerNum, char* hostname, int port, int index, int numUsers);
int send_greeting(int fd, char* username);
int send_screen(int fd, const int status, const int board [][BOARD_WIDTH], int opponentDir);
int send_check(int fd, bool status);

// Receive a message from a socket and return the message string (which must be freed later).
// Returns NULL when an error occurs.
init_packet_t* receive_init(int fd);
start_packet_t* receive_start(int fd);
char* receive_greeting(int fd);
int receive_and_update_screen(int fd, int board[][BOARD_WIDTH], int* opponentDir);
bool receive_check(int fd);



int send_password_list(int fd, const password_set_node_t* passwords, size_t numPasswords);
size_t receive_and_update_password_list(int fd, password_set_node_t** passwordList);
int send_password_match(int fd, int index, char* password);
int receive_and_update_password_match(int fd, password_set_node* passwordList);