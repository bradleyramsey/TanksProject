// #define _GNU_SOURCE
#include "cracker-gpu.h"
#include "message.h"

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


/* The code is set up to work on any arbitrary array of characters. You just need to replace '0' with the 
  lowest ASCII-valued character. This is because the alphabet acts as an offset. The values are hardcoded 
  instead of using a universal variable to save time, but it could also be DEFINEd, I just figured it was
  working and wouldn't be too hard to do a quick find and replace to change if need be. Double check that
  the alphabet is in ascending ASCII order. */

    //Different examples of alphabet arrays

// __device__ int alphabet[] = {'0'-'0','1'-'0','2'-'0','3'-'0','4'-'0','5'-'0','6'-'0','7'-'0','8'-'0','9'-'0'}
//                          '0'-'0','B'-'0','C'-'0','D'-'0','E'-'0','F'-'0','G'-'0','H'-'0','I'-'0','J'-'0','K'-'0','L'-'0','M'-'0','N'-'0','O'-'0','P'-'0','Q'-'0','R'-'0','S'-'0','T'-'0','U'-'0','V'-'0','W'-'0','X'-'0','Y'-'0','Z'-'0',
//                          'a'-'0','b'-'0','c'-'0','d'-'0','e'-'0','f'-'0','g'-'0','h'-'0','i'-'0','j'-'0','k'-'0','l'-'0','m'-'0','n'-'0','o'-'0','p'-'0','q'-'0','r'-'0','s'-'0','t'-'0','u'-'0','v'-'0','w'-'0','x'-'0','y'-'0','z'-'0',

__device__ int alphabet[] = {'0'-'0','1'-'0','2'-'0','3'-'0','4'-'0','5'-'0','6'-'0','7'-'0','8'-'0','9'-'0',
                              'a'-'0','b'-'0','c'-'0','d'-'0','e'-'0','f'-'0','g'-'0','h'-'0','i'-'0','j'-'0','k'-'0','l'-'0','m'-'0','n'-'0','o'-'0','p'-'0','q'-'0','r'-'0','s'-'0','t'-'0','u'-'0','v'-'0','w'-'0','x'-'0','y'-'0','z'-'0'};


#define THREADS_PER_BLOCK (ALPHABET_SIZE)
#define PASSWORDS_PER_THREAD (ALPHABET_SIZE * ALPHABET_SIZE)
#define PASSWORDS_PER_BLOCK (PASSWORDS_PER_THREAD * THREADS_PER_BLOCK)

#define SEARCH_SPACE_SIZE (pow(ALPHABET_SIZE, PASSWORD_LENGTH))

#define TRUE 1
#define FALSE 0

/************************* MD5 *************************/
// I didn't have enough time, but if revisited, look into: https://github.com/VladX/md5-bruteforcer/blob/master/gpu.cu


// I didn't write the MD5 code myself. Here's the attribution. It would have been insanely time consuming to do it
// so I really appreciate this open-souce implemention which let me focus on the more interesting parts of the project
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




/********************* Ok, back to our code ************************/
__constant__ int numPasswordsGPU;
__constant__ int numUsersGPU;
__constant__ int offsetGPU;
int numPasswords;




// This is the fuction that runs on each thread
__global__ void cracker_thread(password_set_node_t* passwords){
  uint8_t candidate_hash[MD5_DIGEST_LENGTH]; //< This will hold the hash of the candidate password

  // Same as the individual thread, but now we have each start on offset, and inc by the # of threads
  char candidate_passwd[] = "0000000";
  candidate_passwd[0]+= alphabet[offsetGPU];
  candidate_passwd[2]+= alphabet[threadIdx.x];
  candidate_passwd[3]+= alphabet[blockIdx.x % ALPHABET_SIZE];
  candidate_passwd[4]+= alphabet[(blockIdx.x / ALPHABET_SIZE) % ALPHABET_SIZE];
  candidate_passwd[5]+= alphabet[((blockIdx.x / ALPHABET_SIZE) / ALPHABET_SIZE) % ALPHABET_SIZE];
  candidate_passwd[6]+= alphabet[(((blockIdx.x / ALPHABET_SIZE) / ALPHABET_SIZE) / ALPHABET_SIZE) % ALPHABET_SIZE];

  int hash_index;

  // Loop for the second character
  for(int i = 1; i <= ALPHABET_SIZE; i++){
    while(candidate_passwd[0] < '0' + alphabet[ALPHABET_SIZE - 1]){ // Loop for the first character - will be less than alphabet size if there's more than one computer connected
      md5String(candidate_passwd, PASSWORD_LENGTH, candidate_hash);

      // Get the bucket corresponding to the hash
      hash_index = (candidate_hash[0] & numBucketsAndMask);

      // Now check if the hash of the candidate password matches any of the hashs in the bucket, 
      // going along till we get to an empty one. Since they needed to be all sent together, an 
      // array was best. The goal is that this while statement usually fails the first time
      while(passwords[hash_index].hashed_password[0] != 0){
        if(hashcmp(candidate_hash, passwords[hash_index].hashed_password)){
          pwdcpy(candidate_passwd, passwords[hash_index].solved_password); // If we match, copy it over
          break;
        }
        hash_index = (hash_index + 1) % (numBucketsAndMask + 1); // If there was something in the bucket
                          // that didn't match, we have to go to the next one, but make sure to roll over
      }
      // Inc the last charcter. We'll use the number of users so that none of the computers check the same
      // passwords without having to coordinate across machines beyond the inital offset
      candidate_passwd[0] += numUsersGPU;
    }
    // Reset the first character for the next loop, and set the second charater to the next one in alphabet
    candidate_passwd[0] = '0' + alphabet[offsetGPU];
    candidate_passwd[1] = '0' + alphabet[i];
  }
  // Really hard but potential future TODO: Add check somewhere if we've cracked all passwords? 
  //  This would be super tough among all the different computers and threads.
}

/**
 * Crack all of the passwords in a set of passwords. The function should print the username
 * and cracked password for each user listed in passwords, separated by a space character.
 * Complete this implementation for part B of the lab.
 *
 * \returns The number of passwords cracked in the list
 */
void* crack_password_list(void* tempArgs) {
  // Unpack
  list_cracker_args_t* args = (list_cracker_args_t*) tempArgs;
  password_set_node_t* argsPasswords = args->argsPasswords;
  numPasswords = args->numPasswords;
  int index = args->index;
  int numUsers = args->numUsers;
  int host_fd = args->host_fd;

  // Copy some preliminary constants we need to run MD5 on the GPU
  if (cudaMemcpyToSymbol(K, cpuK, sizeof(uint32_t) * 64, 0, cudaMemcpyHostToDevice) !=
      cudaSuccess) {
    fprintf(stderr, "Failed to copy K to the GPU\n");
  }

  if (cudaMemcpyToSymbol(S, cpuS, sizeof(uint32_t) * 64, 0, cudaMemcpyHostToDevice) !=
      cudaSuccess) {
    fprintf(stderr, "Failed to copy S to the GPU\n");
  }

  // Now copy some specific parameters we need to crack in parallel
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

  // Now, send over the password Hash Map (in array-style)
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

  
  // Calculate the number of blocks we need to cover the search space, and run the solver
  size_t blocks = (SEARCH_SPACE_SIZE + PASSWORDS_PER_BLOCK - 1) / PASSWORDS_PER_BLOCK;
  cracker_thread<<<blocks, dim3(ALPHABET_SIZE)>>>(GPUpasswords); // Actually run the solver on each thread
              // Note: this is definitely not an optimal allocation of threads and blocks, but 36x36 was too many threads per block
              // the code could be made more efficient with a different manner of dividing the search space 

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

  // Now go through the buckets and send back any solved passwords
  for(int i = 0; i < (numBucketsAndMask + 1); i++){
    if(argsPasswords[i].solved_password[0] != 0){ 
      // printf("%s %.*s\n", argsPasswords[i].username, PASSWORD_LENGTH, argsPasswords[i].solved_password);
      multi_send_password_and_end(host_fd, 0, i, argsPasswords[i].solved_password, 0);
    }
  }
  return NULL;
}
