#include "message.h"
#include "cracker-gpu.h"
#include "tank.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>

// Send a across a socket with a header that includes the message length.
int send_init(int fd, const char* username, const uint8_t* passwordHash) {
  // If the message is NULL, set errno to EINVAL and return an error
  if (passwordHash == NULL) {
    errno = EINVAL;
    return -1;
  }

  // First, send the length of the username in a size_t
  size_t len = strlen(username);
  if (write(fd, &len, sizeof(size_t)) != sizeof(size_t)) {
    // Writing failed, so return an error
    return -1;
  }

  // Now we can send the username. Loop until the entire message has been written.
  size_t bytes_written = 0;
  while (bytes_written < len) {
    // Try to write the entire remaining username
    ssize_t rc = write(fd, username + bytes_written, len - bytes_written);

    // Did the write fail? If so, return an error
    if (rc <= 0) return -1;

    // If there was no error, write returned the number of bytes written
    bytes_written += rc;
  }


  // Now we can send password hash. Loop until the entire message has been written.
  bytes_written = 0;
  while (bytes_written < MD5_DIGEST_LENGTH) {
    // Try to write the entire remaining message
    ssize_t rc = write(fd, passwordHash + bytes_written, MD5_DIGEST_LENGTH - bytes_written);

    // Did the write fail? If so, return an error
    if (rc <= 0) return -1;

    // If there was no error, write returned the number of bytes written
    bytes_written += rc;
  }



  return 0;
}

// Receive a message from a socket and return the message string (which must be freed later)
init_packet_t* receive_init(int fd) {
  init_packet_t* received_info = (init_packet_t*) (malloc(sizeof(init_packet_t)));
  size_t len;
  if (read(fd, &len, sizeof(size_t)) != sizeof(size_t)) {
    // Reading failed. Return an error
    return NULL;
  }

  // Now make sure the message length is reasonable
  if (len > MAX_MESSAGE_LENGTH) {
    errno = EINVAL;
    return NULL;
  }

  // Allocate space for the message and a null terminator
  char* username = (char*) malloc(len + 1);

  // Try to read the message. Loop until the entire message has been read.
  size_t bytes_read = 0;
  while (bytes_read < len) {
    // Try to read the entire remaining message
    ssize_t rc = read(fd, username + bytes_read, len - bytes_read);

    // Did the read fail? If so, return an error
    if (rc <= 0) {
      free(username);
      return NULL;
    }

    // Update the number of bytes read
    bytes_read += rc;
  }

  username[len] = '\0';
  received_info->username = username;


  len = MD5_DIGEST_LENGTH;

  // Allocate space for the hashed password
  uint8_t* passwordHash = (uint8_t*) malloc(sizeof(uint8_t) * len);

  // Try to read the message. Loop until the entire message has been read.
  bytes_read = 0;
  while (bytes_read < len) {
    // Try to read the entire remaining message
    ssize_t rc = read(fd, passwordHash + bytes_read, len - bytes_read);

    // Did the read fail? If so, return an error
    if (rc <= 0) {
      free(passwordHash);
      return NULL;
    }

    // Update the number of bytes read
    bytes_read += rc;
  }

  // Add a null terminator to the message
  passwordHash[len] = '\0';
  received_info->passwordHash = passwordHash;
  return received_info;
}


// Send a across a socket with a header that includes the message length.
int send_start(int fd, const int playerNum, char* hostname, int port) {
  // If the message is NULL, set errno to EINVAL and return an error
  if (playerNum != 1 && playerNum != 2) {
    errno = EINVAL;
    return -1;
  }

  
  // We need to send the player num either
  if (write(fd, &playerNum, sizeof(int)) != sizeof(int)) {
    // Writing failed, so return an error
    return -1;
  }

  // But if we're sending to player 1, we don't have the port yet, so we'll just have them spin up their socket
  if(playerNum == 1){
    return 0;
  }

  // First, send the length of the hostname in a size_t
  size_t len = strlen(hostname);
  if (write(fd, &len, sizeof(size_t)) != sizeof(size_t)) {
    // Writing failed, so return an error
    return -1;
  }

  // Now we can send the hostname. Loop until the entire message has been written.
  size_t bytes_written = 0;
  while (bytes_written < len) {
    // Try to write the entire remaining hostname
    ssize_t rc = write(fd, hostname + bytes_written, len - bytes_written);

    // Did the write fail? If so, return an error
    if (rc <= 0) return -1;

    // If there was no error, write returned the number of bytes written
    bytes_written += rc;
  }

  // Lastly, we need to send the port so that player 2 knows what to connect to.
  if (write(fd, &port, sizeof(int)) != sizeof(int)) {
    // Writing failed, so return an error
    return -1;
  }

  return 0;
}

// Receive a message from a socket and return the message string (which must be freed later)
start_packet_t* receive_start(int fd) {
  start_packet_t* received_info = (start_packet_t*) (malloc(sizeof(start_packet_t)));
  int playerNum;
  if (read(fd, &playerNum, sizeof(int)) != sizeof(int)) {
    // Reading failed. Return null
    return NULL;
  }

  received_info->playerNum = playerNum;

  // Now see if we're player 1 or if we need to keep trying to read this message
  if (playerNum == 1) {
    received_info->hostname = NULL;
    received_info->port = 0;
    return received_info;
  }


  // Read the size of the hostname
  size_t len;
  if (read(fd, &len, sizeof(size_t)) != sizeof(size_t)) {
    // Reading failed. Return an error
    return NULL;
  }

  // Now make sure the message length is reasonable
  if (len > MAX_MESSAGE_LENGTH) {
    errno = EINVAL;
    return NULL;
  }

  // Allocate space for the hostname and a null terminator
  char* hostname = (char*) malloc(len + 1);

  // Try to read the hostname. Loop until the entire hostname has been read.
  size_t bytes_read = 0;
  while (bytes_read < len) {
    // Try to read the entire remaining hostname
    ssize_t rc = read(fd, hostname + bytes_read, len - bytes_read);

    // Did the read fail? If so, return an error
    if (rc <= 0) {
      free(hostname);
      return NULL;
    }

    // Update the number of bytes read
    bytes_read += rc;
  }
  hostname[len] = '\0';
  received_info->hostname = hostname;


  //And read the port
  int port;
  if (read(fd, &port, sizeof(int)) != sizeof(int)) {
    // Reading failed. Return an error
    return NULL;
  }
  received_info->port = port;

  return received_info;
}

// Send a across a socket with a header that includes the message length.
int send_greeting(int fd, char* username) {
  // If the message is NULL, set errno to EINVAL and return an error
  if (username == NULL) {
    errno = EINVAL;
    return -1;
  }

  // First, send the length of the username in a size_t
  size_t len = strlen(username);
  if (write(fd, &len, sizeof(size_t)) != sizeof(size_t)) {
    // Writing failed, so return an error
    return -1;
  }

  // Now we can send the username. Loop until the entire username has been written.
  size_t bytes_written = 0;
  while (bytes_written < len) {
    // Try to write the entire remaining username
    ssize_t rc = write(fd, username + bytes_written, len - bytes_written);

    // Did the write fail? If so, return an error
    if (rc <= 0) return -1;

    // If there was no error, write returned the number of bytes written
    bytes_written += rc;
  }

  return 0;
}

// Receive a message from a socket and return the message string (which must be freed later)
char* receive_greeting(int fd) {

  // Read the size of the hostname
  size_t len;
  if (read(fd, &len, sizeof(size_t)) != sizeof(size_t)) {
    // Reading failed. Return an error
    return NULL;
  }

  // Now make sure the message length is reasonable
  if (len > MAX_MESSAGE_LENGTH) {
    errno = EINVAL;
    return NULL;
  }

  // Allocate space for the username and a null terminator
  char* username = (char*) malloc(len + 1);

  // Try to read the username. Loop until the entire username has been read.
  size_t bytes_read = 0;
  while (bytes_read < len) {
    // Try to read the entire remaining username
    ssize_t rc = read(fd, username + bytes_read, len - bytes_read);

    // Did the read fail? If so, return an error
    if (rc <= 0) {
      free(username);
      return NULL;
    }

    // Update the number of bytes read
    bytes_read += rc;
  }
  username[len] = '\0';
  return username;
}

/**
 * NOTE: the game statuses are:
 *    0 - start
 *    1 - gameplay
 *    2 - game over
 * 
 *   -1 - ERROR
*/

// Send a screen across a socket with a header that includes the game status.
int send_screen(int fd, const int status, const int board [][BOARD_WIDTH]) {
  // If the message is NULL, set errno to EINVAL and return an error
  if (board == NULL || status == -1) {
    errno = EINVAL;
    return -1;
  }

  // First, send the length of the message in a size_t
  if (write(fd, &status, sizeof(int)) != sizeof(int)) {
    // Writing failed, so return an error
    return -1;
  }

  size_t bytesToWrite = sizeof(int) * BOARD_HEIGHT * BOARD_WIDTH;
  // if (write(fd, board, bytesToWrite) != bytesToWrite) {
  //   // Writing failed, so return an error
  //   return -1;
  // }

  size_t bytes_written = 0;
  while (bytes_written < bytesToWrite) {
    // Try to write the entire remaining username
    ssize_t rc = send(fd, board + bytes_written, bytesToWrite - bytes_written, 0);

    // Did the write fail? If so, return an error
    if (rc <= 0) return -1;

    // If there was no error, write returned the number of bytes written
    bytes_written += rc;
  }

  return 0;
}

// Receive a message from a socket and update the board state. Returns the game status
int receive_and_update_screen(int fd, int board[][BOARD_WIDTH]) {
  // Allocate space for the message and a null terminator
  int status;
  if (read(fd, &status, sizeof(int)) != sizeof(int)) {
    // Reading failed. Return an error
    return -1;
  }

  if(board == NULL){
    if(status == 0){
      return 0;
    }
    return -1;
  } 

  size_t bytesToRead = sizeof(int) * BOARD_HEIGHT * BOARD_WIDTH;

  // Read the whole board, thanks to the big man above... Charlie Curtsinger
  recv(fd, board, bytesToRead, MSG_WAITALL);

  return status;
}
