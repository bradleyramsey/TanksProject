// #define _GNU_SOURCE
#include "cracker-gpu.h"

#include <openssl/md5.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <stdbool.h>


#include "util.h"


// How many characters do we have to search through
#define ALPHABET_SIZE 26

//TODO: maybe have an array of all the characters in the alphabet?

// Each board will be a block
#define THREADS_PER_BLOCK (ALPHABET_SIZE * ALPHABET_SIZE)
#define PASSWORDS_PER_THREAD (ALPHABET_SIZE * ALPHABET_SIZE)
#define PASSWORDS_PER_BLOCK (PASSWORDS_PER_THREAD * THREADS_PER_BLOCK)

#define SEARCH_SPACE_SIZE (pow(26, PASSWORD_LENGTH))

#define TRUE 1
#define FALSE 0

/************************* MD5 *************************/
// Look into: https://github.com/VladX/md5-bruteforcer/blob/master/gpu.cu
/*
 * Derived from the RSA Data Security, Inc. MD5 Message-Digest Algorithm
 * and modified slightly to be functionally identical but condensed into control structures.
 * From Zunawe at https://github.com/Zunawe/md5-c/blob/main/md5.c
 */

/*
 * Constants defined by the MD5 algorithm
 */
#define A 0x67452301
#define B 0xefcdab89
#define C 0x98badcfe
#define D 0x10325476

typedef struct{
    uint64_t size;        // Size of input in bytes
    uint32_t buffer[4];   // Current accumulation of hash
    uint8_t input[64];    // Input to be used in the next step
    uint8_t digest[16];   // Result of algorithm
}MD5Context;

__device__ void md5Init(MD5Context *ctx);
__device__ void md5Update(MD5Context *ctx, uint8_t *input, size_t input_len);
__device__ void md5Finalize(MD5Context *ctx);
__device__ void md5Step(uint32_t *buffer, uint32_t *input);
 
__device__ void md5String(char *input, uint8_t *result);

__constant__ uint32_t S[64];
__constant__ uint32_t K[64];
__constant__ uint8_t PADDING[64];

static uint32_t cpuS[] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
                       5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20,
                       4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
                       6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};



static uint32_t cpuK[] = {0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
                       0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
                       0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
                       0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
                       0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
                       0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
                       0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
                       0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
                       0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
                       0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
                       0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
                       0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
                       0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
                       0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
                       0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
                       0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};

/*
 * Padding used to make the size (in bits) of the input congruent to 448 mod 512
 */
static uint8_t cpuPADDING[] = {0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

/*
 * Bit-manipulation functions defined by the MD5 algorithm
 */
#define F(X, Y, Z) ((X & Y) | (~X & Z))
#define G(X, Y, Z) ((X & Z) | (Y & ~Z))
#define H(X, Y, Z) (X ^ Y ^ Z)
#define I(X, Y, Z) (Y ^ (X | ~Z))

/*
 * Rotates a 32-bit word left by n bits
 */
__device__ uint32_t rotateLeft(uint32_t x, uint32_t n){
    return (x << n) | (x >> (32 - n));
}


/*
 * Initialize a context
 */
__device__ void md5Init(MD5Context *ctx){
    ctx->size = (uint64_t)0;

    ctx->buffer[0] = (uint32_t)A;
    ctx->buffer[1] = (uint32_t)B;
    ctx->buffer[2] = (uint32_t)C;
    ctx->buffer[3] = (uint32_t)D;
}

/*
 * Add some amount of input to the context
 *
 * If the input fills out a block of 512 bits, apply the algorithm (md5Step)
 * and save the result in the buffer. Also updates the overall size.
 */
__device__ void md5Update(MD5Context *ctx, uint8_t *input_buffer, size_t input_len){
    uint32_t input[16];
    unsigned int offset = ctx->size % 64;
    ctx->size += (uint64_t)input_len;

    // Copy each byte in input_buffer into the next space in our context input
    for(unsigned int i = 0; i < input_len; ++i){
        ctx->input[offset++] = (uint8_t)*(input_buffer + i);

        // If we've filled our context input, copy it into our local array input
        // then reset the offset to 0 and fill in a new buffer.
        // Every time we fill out a chunk, we run it through the algorithm
        // to enable some back and forth between cpu and i/o
        if(offset % 64 == 0){
            for(unsigned int j = 0; j < 16; ++j){
                // Convert to little-endian
                // The local variable `input` our 512-bit chunk separated into 32-bit words
                // we can use in calculations
                input[j] = (uint32_t)(ctx->input[(j * 4) + 3]) << 24 |
                           (uint32_t)(ctx->input[(j * 4) + 2]) << 16 |
                           (uint32_t)(ctx->input[(j * 4) + 1]) <<  8 |
                           (uint32_t)(ctx->input[(j * 4)]);
            }
            md5Step(ctx->buffer, input);
            offset = 0;
        }
    }
}

/*
 * Pad the current input to get to 448 bytes, append the size in bits to the very end,
 * and save the result of the final iteration into digest.
 */
__device__ void md5Finalize(MD5Context *ctx){
    uint32_t input[16];
    unsigned int offset = ctx->size % 64;
    unsigned int padding_length = offset < 56 ? 56 - offset : (56 + 64) - offset;

    // Fill in the padding and undo the changes to size that resulted from the update
    md5Update(ctx, PADDING, padding_length);
    ctx->size -= (uint64_t)padding_length;

    // Do a final update (internal to this function)
    // Last two 32-bit words are the two halves of the size (converted from bytes to bits)
    for(unsigned int j = 0; j < 14; ++j){
        input[j] = (uint32_t)(ctx->input[(j * 4) + 3]) << 24 |
                   (uint32_t)(ctx->input[(j * 4) + 2]) << 16 |
                   (uint32_t)(ctx->input[(j * 4) + 1]) <<  8 |
                   (uint32_t)(ctx->input[(j * 4)]);
    }
    input[14] = (uint32_t)(ctx->size * 8);
    input[15] = (uint32_t)((ctx->size * 8) >> 32);

    md5Step(ctx->buffer, input);

    // Move the result into digest (convert from little-endian)
    for(unsigned int i = 0; i < 4; ++i){
        ctx->digest[(i * 4) + 0] = (uint8_t)((ctx->buffer[i] & 0x000000FF));
        ctx->digest[(i * 4) + 1] = (uint8_t)((ctx->buffer[i] & 0x0000FF00) >>  8);
        ctx->digest[(i * 4) + 2] = (uint8_t)((ctx->buffer[i] & 0x00FF0000) >> 16);
        ctx->digest[(i * 4) + 3] = (uint8_t)((ctx->buffer[i] & 0xFF000000) >> 24);
    }
}

/*
 * Step on 512 bits of input with the main MD5 algorithm.
 */
__device__ void md5Step(uint32_t *buffer, uint32_t *input){
    uint32_t AA = buffer[0];
    uint32_t BB = buffer[1];
    uint32_t CC = buffer[2];
    uint32_t DD = buffer[3];

    uint32_t E;

    unsigned int j;

    for(unsigned int i = 0; i < 64; ++i){
        switch(i / 16){
            case 0:
                E = F(BB, CC, DD);
                j = i;
                break;
            case 1:
                E = G(BB, CC, DD);
                j = ((i * 5) + 1) % 16;
                break;
            case 2:
                E = H(BB, CC, DD);
                j = ((i * 3) + 5) % 16;
                break;
            default:
                E = I(BB, CC, DD);
                j = (i * 7) % 16;
                break;
        }

        uint32_t temp = DD;
        DD = CC;
        CC = BB;
        BB = BB + rotateLeft(AA + E + K[i] + input[j], S[i]);
        AA = temp;
    }

    buffer[0] += AA;
    buffer[1] += BB;
    buffer[2] += CC;
    buffer[3] += DD;
}

/*
 * Functions that run the algorithm on the provided input and put the digest into result.
 * result should be able to store 16 bytes.
 */
__device__ void md5String(char *input, size_t length, uint8_t *result){
    MD5Context ctx;
    md5Init(&ctx);
    md5Update(&ctx, (uint8_t *)input, length);
    md5Finalize(&ctx);

    memcpy(result, ctx.digest, 16);
}


__device__ bool hashcmp(uint8_t * one, uint8_t * theOther){
  for(int i = 0; i < MD5_DIGEST_LENGTH; i++){
    if(one[i] != theOther[i]){
      return false;
    }
  }
  return true;
}

__device__ void pwdcpy(char * from, char * to){
  for(int i = 0; i < PASSWORD_LENGTH; i++){
    to[i] = from[i];
  }
}

void usrnmcpy(char * from, char * to){
  for(int i = 0; i < PASSWORD_LENGTH; i++){
    to[i] = from[i];
  }
}




/********************* Parts B & C ************************/



__constant__ int numPasswordsGPU;
__constant__ int numUsersGPU;
__constant__ int offsetGPU;
int numPasswords;





/**
 * Add a password to a password set
 * \param passwords   A pointer to a password set initialized with the function above.
 * \param username    The name of the user being added. The memory that holds this string's
 *                    characters will be reused, so if you keep a copy you must duplicate the
 *                    string. I recommend calling strdup().
 * \param password_hash   An array of MD5_DIGEST_LENGTH bytes that holds the hash of this user's
 *                        password. The memory that holds this array will be reused, so you must
 *                        make a copy of this value if you retain it in your data structure.
 */
// void add_password(password_set_t* passwords, char* username, uint8_t* password_hash) {
//   // Malloc space for the node
//   password_set_node_t* node = (password_set_node_t*) malloc(sizeof(password_set_node_t));

//   // Then assign all the fields
//   node->username = strdup(username);
//   memcpy(node->hashed_password, password_hash, MD5_DIGEST_LENGTH);
//   node->next = passwords->buckets[password_hash[0] & numBucketsAndMask];
//   node->prev = NULL;

//   // And link the next's node's prev to this ones
//   if(node->next != NULL){
//     node->next->prev = node;
//   }

//   // Then update the bucket reference
//   passwords->buckets[password_hash[0] & numBucketsAndMask] = node;

//   // And the # of passwords
//   // passwords->numPasswords++;
// }

// This is the fuction that runs on each thread
__global__ void cracker_thread(password_set_node_t* passwords){
  // printf("test");
  // And declare local
  uint8_t candidate_hash[MD5_DIGEST_LENGTH]; //< This will hold the hash of the candidate password

  // Same as the individual thread, but now we have each start on offset, and inc by the # of threads
  char candidate_passwd[] = "aaaaaaa";
  candidate_passwd[0]+= offsetGPU;
  candidate_passwd[2]+= threadIdx.x;
  candidate_passwd[3]+= threadIdx.y;
  // candidate_passwd[2]+= threadIdx.z;
  candidate_passwd[4]+= blockIdx.x % 26;
  candidate_passwd[5]+= (blockIdx.x / 26) % 26;
  candidate_passwd[6]+= ((blockIdx.x / 26) / 26) % 26;
  // candidate_passwd[6]+= (((blockIdx.x / 26) / 26) / 26) % 26;
  // MD5((unsigned char*)candidate_passwd, PASSWORD_LENGTH, candidate_hash); //< Do the hash (this is the slowest part of this implementation)
  int hash_index;

  for(int i = 0; i < ALPHABET_SIZE; i++){
    for(int j = 0; j < ALPHABET_SIZE; j++){
      md5String(candidate_passwd, PASSWORD_LENGTH, candidate_hash);

  // Get the bucket corresponding to the hash
  // // password = passwords->buckets[candidate_hash[0] & numBucketsAndMask];
  // if(threadIdx.x == 0 && threadIdx.y == 0 && threadIdx.z == 0 && blockIdx.x == 0){
  //   // passwords[0].username;
  //   printf("%s %x", candidate_passwd, passwords[0].hashed_password);
  //   // cuPrintf("test!!!");
  // }
      hash_index = (candidate_passwd[0] & numBucketsAndMask);

      // Now check if the hash of the candidate password matches any of the hashs in the bucket, 
      // going along till we get to an empty one. Since they needed to be all sent together, an array was best
      while(passwords[hash_index].hashed_password[0] != 0){
        if(hashcmp(candidate_hash, passwords[hash_index].hashed_password)){
          // cudaMemcpy(&(passwords[i].solved_password), candidate_passwd, sizeof(char) * PASSWORD_LENGTH, cudaMemcpyDeviceToDevice);
          pwdcpy(candidate_passwd, passwords[hash_index].solved_password);
          // printf("%s", candidate_passwd);
          break;
        }
        hash_index = (hash_index + 1) % (numBucketsAndMask + 1);
    }
      // while(int i = 0; i < numPasswordsGPU; i++){
      //   // if(memcmp(candidate_hash, &(passwords[i].hashed_password), MD5_DIGEST_LENGTH) == 0) {
        
      // }
      candidate_passwd[0] += numUsersGPU;
    }
    candidate_passwd[0] = 'a';
    candidate_passwd[1]++;
  }
  // Potential TODO: Add check somewhere if we've cracked all passwords? This would be tough among all 
  //                  the different computers and threads.
}

/**
 * Crack all of the passwords in a set of passwords. The function should print the username
 * and cracked password for each user listed in passwords, separated by a space character.
 * Complete this implementation for part B of the lab.
 *
 * \returns The number of passwords cracked in the list
 */
void crack_password_list_num(password_set_node_t* argsPasswords, size_t numPasswordsArg, int index, int numUsers) {
  // Change the buffer so we don't waste time on constant system calls and context switches
  // char buffer[2048];
  // setvbuf(stdout, buffer, _IOFBF, 2048);
  numPasswords = numPasswordsArg;


  // if (cudaMalloc(&K, sizeof(uint32_t) * 64) != cudaSuccess) {
  //   fprintf(stderr, "Failed to allocate K array on GPU\n");
  //   exit(2);
  // }
  if (cudaMemcpyToSymbol(K, cpuK, sizeof(uint32_t) * 64, 0, cudaMemcpyHostToDevice) !=
      cudaSuccess) {
    fprintf(stderr, "Failed to copy K to the GPU\n");
  }

  if (cudaMemcpyToSymbol(S, cpuS, sizeof(uint32_t) * 64, 0, cudaMemcpyHostToDevice) !=
      cudaSuccess) {
    fprintf(stderr, "Failed to copy S to the GPU\n");
  }

  if (cudaMemcpyToSymbol(numUsersGPU, &numUsers, sizeof(int), 0, cudaMemcpyHostToDevice) !=
      cudaSuccess) {
    fprintf(stderr, "Failed to copy numUsers to the GPU\n");
  }

  if (cudaMemcpyToSymbol(offsetGPU, &index, sizeof(int), 0, cudaMemcpyHostToDevice) !=
      cudaSuccess) {
    fprintf(stderr, "Failed to copy index to the GPU\n");
  }

  if (cudaMemcpyToSymbol(PADDING, cpuPADDING, sizeof(uint8_t) * 64, 0, cudaMemcpyHostToDevice) !=
      cudaSuccess) {
    fprintf(stderr, "Failed to copy PADDING to the GPU\n");
  }

  // Just do array for now will need to change to accomodate HASH
  password_set_node_t* GPUpasswords;

  // Allocate space for the boards on the GPU
  if (cudaMalloc(&GPUpasswords, sizeof(password_set_node_t) * (numBucketsAndMask + 1)) != cudaSuccess) {
    fprintf(stderr, "Failed to allocate passwords array on GPU\n");
    exit(2);
  }

  // Copy the cpu's x array to the gpu with cudaMemcpy
  if (cudaMemcpy(GPUpasswords, argsPasswords, sizeof(password_set_node_t) * (numBucketsAndMask + 1), cudaMemcpyHostToDevice) !=
      cudaSuccess) {
    fprintf(stderr, "Failed to copy password to the GPU\n");
  }

  
  // dim3 layout(ALPHABET_SIZE, ALPHABET_SIZE, ALPHABET_SIZE);
  size_t blocks = (SEARCH_SPACE_SIZE + PASSWORDS_PER_BLOCK - 1) / PASSWORDS_PER_BLOCK;
  cracker_thread<<<blocks, dim3(26, 26)>>>(GPUpasswords); // Actually run the solver on each thread
  // cracker_thread<<<blocks, layout>>>(GPUpasswords); // Actually run the solver on each thread

  // Wait for all the threads to finish
  if (cudaDeviceSynchronize() != cudaSuccess) {
    fprintf(stderr, "CUDA Error: %s\n", cudaGetErrorString(cudaPeekAtLastError()));
  }

  // Copy the solved array back from the gpu to the cpu
  if(cudaMemcpy(argsPasswords, GPUpasswords, sizeof(password_set_node_t) * (numBucketsAndMask + 1), cudaMemcpyDeviceToHost) != cudaSuccess) {
    fprintf(stderr, "Failed to copy back from the GPU\n");
  }

  // Free the board memory on the GPU
  cudaFree(GPUpasswords);
  cudaFree(K);
  cudaFree(S);
  cudaFree(PADDING);

  for(int i = 0; i < (numBucketsAndMask + 1); i++){
    if(argsPasswords[i].hashed_password[0] != 0){
      printf("%s %.*s\n", argsPasswords[i].username, PASSWORD_LENGTH, argsPasswords[i].solved_password);
    }
  }
}


void crack_password_list(password_set_node_t* passwords) {
  crack_password_list_num(passwords, 256, 1, 1);
}

/******************** Provided Code ***********************/

/**
 * Convert a string representation of an MD5 hash to a sequence
 * of bytes. The input md5_string must be 32 characters long, and
 * the output buffer bytes must have room for MD5_DIGEST_LENGTH
 * bytes.
 *
 * \param md5_string  The md5 string representation
 * \param bytes       The destination buffer for the converted md5 hash
 * \returns           0 on success, -1 otherwise
 */
int md5_string_to_bytes(const char* md5_string, uint8_t* bytes) {
  // Check for a valid MD5 string
  if(strlen(md5_string) != 2 * MD5_DIGEST_LENGTH) return -1;

  // Start our "cursor" at the start of the string
  const char* pos = md5_string;

  // Loop until we've read enough bytes
  for(size_t i=0; i<MD5_DIGEST_LENGTH; i++) {
    // Read one byte (two characters)
    int rc = sscanf(pos, "%2hhx", &bytes[i]);
    if(rc != 1) return -1;

    // Move the "cursor" to the next hexadecimal byte
    pos += 2;
  }

  return 0;
}

void print_usage(const char* exec_name) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "  %s single <MD5 hash>\n", exec_name);
  fprintf(stderr, "  %s list <password file name>\n", exec_name);
}

// int main(int argc, char** argv) {
//   if(argc != 3) {
//     print_usage(argv[0]);
//     exit(1);
//   }

//   if(strcmp(argv[1], "list") == 0) {
//     // Make and initialize a password set
//     password_set_node_t* passwords = NULL;
//     // init_password_set(&passwords);

//     // Open the password file
//     FILE* password_file = fopen(argv[2], "r");
//     if(password_file == NULL) {
//       perror("opening password file");
//       exit(2);
//     }

//     // Read until we hit the end of the file
//     while(!feof(password_file)) {
//       // Make space to hold the username
//       char username[MAX_USERNAME_LENGTH];

//       // Make space to hold the MD5 string
//       char md5_string[MD5_DIGEST_LENGTH * 2 + 1];

//       // Make space to hold the MD5 bytes
//       uint8_t password_hash[MD5_DIGEST_LENGTH];

//       // Try to read. The space in the format string is required to eat the newline
//       if(fscanf(password_file, "%s %s ", username, md5_string) != 2) {
//         fprintf(stderr, "Error reading password file: malformed line\n");
//         exit(2);
//       }

//       // Convert the MD5 string to MD5 bytes in our new node
//       if(md5_string_to_bytes(md5_string, password_hash) != 0) {
//         fprintf(stderr, "Error reading MD5\n");
//         exit(2);
//       }

//       // Add the password to the password set
//       // add_password(&passwords, username, password_hash);
//       add_password_array(&passwords, username, password_hash);
//     }

//     // Now run the password list cracker
//     crack_password_list(passwords);

//   } else {
//     print_usage(argv[0]);
//     exit(1);
//   }

//   return 0;
// }