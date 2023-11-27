#include "message.h"
#include "cracker-gpu.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Send a across a socket with a header that includes the message length.
int send_init(int fd, const char* username, const uint8_t* passwordHash) {
  // If the message is NULL, set errno to EINVAL and return an error
  if (passwordHash == NULL) {
    errno = EINVAL;
    return -1;
  }

  // First, send the length of the message in a size_t
  size_t len = strlen(username);
  if (write(fd, &len, sizeof(size_t)) != sizeof(size_t)) {
    // Writing failed, so return an error
    return -1;
  }

  // Now we can send the message. Loop until the entire message has been written.
  size_t bytes_written = 0;
  while (bytes_written < len) {
    // Try to write the entire remaining message
    ssize_t rc = write(fd, username + bytes_written, len - bytes_written);

    // Did the write fail? If so, return an error
    if (rc <= 0) return -1;

    // If there was no error, write returned the number of bytes written
    bytes_written += rc;
  }

  // First, send the length of the message in a size_t


  // Now we can send the message. Loop until the entire message has been written.
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
    username[len] = '\0';
    received_info->username = username;
  }


  len = MD5_DIGEST_LENGTH;

  // Allocate space for the message and a null terminator
  uint8_t* message = (uint8_t*) malloc(len + 1);

  // Try to read the message. Loop until the entire message has been read.
  bytes_read = 0;
  while (bytes_read < len) {
    // Try to read the entire remaining message
    ssize_t rc = read(fd, message + bytes_read, len - bytes_read);

    // Did the read fail? If so, return an error
    if (rc <= 0) {
      free(message);
      return NULL;
    }

    // Update the number of bytes read
    bytes_read += rc;
  }

  // Add a null terminator to the message
  message[len] = '\0';
  received_info->passwordHash = message;
  return received_info;
}