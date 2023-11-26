#pragma once

#define MAX_MESSAGE_LENGTH 2048

// typedef struct {
//   char* username;
//   char* message;
// } packet_t;

typedef struct {
  char* username;
  char* passwordHash;
} init_packet_t;


// Send a across a socket with a header that includes the message length. Returns non-zero value if
// an error occurs.
int send_message(int fd, const char* username, const char* message);

// Receive a message from a socket and return the message string (which must be freed later).
// Returns NULL when an error occurs.
init_packet_t* receive_init(int fd);


