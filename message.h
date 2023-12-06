#pragma once

#define MAX_MESSAGE_LENGTH 2048

#include <stdint.h>
#include "tank.h"

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
} start_packet_t;

typedef struct {
  int* status;
  int* board;
} screen_packet_t;

// Send a across a socket with a header that includes the message length. Returns non-zero value if
// an error occurs.
int send_init(int fd, const char* username, const uint8_t* passwordHash);
int send_assignment(int fd, const int* playerNum, const int* partnerPort);
int send_start(int fd, const int playerNum, char* hostname, int port);
int send_greeting(int fd, char* username);
int send_screen(int fd, const int status, const int board [][BOARD_WIDTH]);
// TODO: Move board width into a header file

// Receive a message from a socket and return the message string (which must be freed later).
// Returns NULL when an error occurs.
init_packet_t* receive_init(int fd);
start_packet_t* receive_start(int fd);
char* receive_greeting(int fd);
int receive_and_update_screen(int fd, int board [][BOARD_WIDTH]);


