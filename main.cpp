#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include "socket.h"
#include "message.h"

#include "cracker-gpu.h"
#include "tank.h"
#define MD5_DIGEST_LENGTH 16


// #define MAX_USERNAME_LENGTH 32
// #define PASSWORD_LENGTH 7
#define numBucketsAndMask 64

#define MAX_PLAYERS 32



typedef struct {
  int client_socket;
  int index;
} args_t;


void guestMain(char* addr);
void hostMain(char * type);



/**
 * This just determines whether or not they're going to host the game, and if they're hosting, if they'd like
 * to change the type of instance they're hosting (tank/passwords/both)
*/
int main(int argc, char** argv) {
    printf("Welcome to the Title-less Tank Game!! If you would like to join an existing game, \
            please type the '<hostname>:<port>' otherwise, type 'host' to start your own!\n>");

    bool done = false;
    while(!done){
        done = true;

        char* hostname;
        scanf("%s", hostname);

        if(strcmp(hostname, "host") == 0){
            char* type; // NULL, "plain", or "passwords"                //"local", 
            if(scanf("%s", type) == EOF){
                hostMain(NULL);
            }
            else{
                hostMain(type);
            }
        }
        else if(strchr(hostname, ':') != NULL){
            guestMain(hostname);
        }
        else{
            printf("Sorry, that input format wasn't recognized, try again!\n>");
            done = false;
        }
    }


}

/**
 * If we're a player in the game, we just need to:
 * 1) Prompt for username and password
 * 2) Wait for the game to start (and get assigned opponent)
 * 3) Play while cracking in the background
 * 4) Dramatically display each user's password at the end of the whole thing (i.e. wait for message from host)
 * 
 * @input: char* addr   The address of the host. Guarenteed to have a ':' character 
*/
void guestMain(char* addr){

    pthread_t tankThread;
    if (pthread_create(&tankThread, NULL, &tankMain, NULL)) {
        perror("pthread_create failed");
        exit(2);
    }


    // Make and initialize a password set
    password_set_node_t* passwords = NULL;

    // Open the password file
    FILE* password_file = fopen("./inputs/7inputs1.txt", "r");
    if(password_file == NULL) {
        perror("opening password file");
        exit(2);
    }

    // Read until we hit the end of the file
    while(!feof(password_file)) {
        // Make space to hold the username
        char username[MAX_USERNAME_LENGTH];

        // Make space to hold the MD5 string
        char md5_string[MD5_DIGEST_LENGTH * 2 + 1];

        // Make space to hold the MD5 bytes
        uint8_t password_hash[MD5_DIGEST_LENGTH];

        // Try to read. The space in the format string is required to eat the newline
        if(fscanf(password_file, "%s %s ", username, md5_string) != 2) {
        fprintf(stderr, "Error reading password file: malformed line\n");
        exit(2);
        }

        // Convert the MD5 string to MD5 bytes in our new node
        if(md5_string_to_bytes(md5_string, password_hash) != 0) {
        fprintf(stderr, "Error reading MD5\n");
        exit(2);
        }

        // Add the password to the password set
        // add_password(&passwords, username, password_hash);
        add_password_array(&passwords, username, password_hash);
    }

    // Now run the password list cracker
    crack_password_list(passwords);

    pthread_join(tankThread, NULL);

    return;
}




/******************************| HOST STUFF BELOW |******************************/

void* listen_init(void* args);


/**
 * This struct is the root of the data structure that will hold users and hashed passwords.
 * There are a number of buckets, and we'll do this in bucket availibity order: if a bucket is full, go to the next one until empty.
 * Instead of trying to be fancy, we just used a mask and a bitwise AND to get a psuedo-random assignment that's also super fast
 */
// typedef struct password_set {
//   // Our buckets
//   struct password_set_node buckets[numBucketsAndMask + 1];

//   // Keeping track of the # of passwords in this set. // Num players will handle this for now
// //   int numPasswords;
// } password_set_t;

// Each node in each bucket keeps the username, hash, and references to make deletion faster
// typedef struct password_set_node {
//   char username [MAX_USERNAME_LENGTH];
//   uint8_t hashed_password [MD5_DIGEST_LENGTH];
// } login_pair_t;

pthread_mutex_t passwordSet_lock = PTHREAD_MUTEX_INITIALIZER;
int numPlayers;
password_set_node_t passwords[numBucketsAndMask + 1];


/**
 * If we're acting as the host for the game, then everyone will connect to us, 
 * and we'll then pair them off in groups to play. TODO: Be able to watch the games
 * In order to do this, we need to:
 * 1) Make a port and listen for connecting players
 * 2) Tell the game when to start and randomly pair off players
 * 3) Listen for their games to finish
 * 4) Pair winners and losers and repeat from the last step
 * 
 * It will also act as the central manager of the cracking software. So it will:
 * 1) Comglomerate the hashes of each user's password
 * 2) When the game starts (and thus no one else can join):
 *      a) Section off the search space depending on # of machines
 *      b) Send a copy of the list to each machines
 * 3) Listen for each machine sending its results 
*/
void hostMain(char * type){
    for(int i = 0; i < numBucketsAndMask + 1; i++){
        password_set_node empty;
        // empty.hashed_password = 0;
        passwords[i].hashed_password[0] = 0;
    }

    // Open up a socket for everyone in the class to connect to 
    unsigned short port = 0;
    int server_socket_fd = server_socket_open(&port);
    if (server_socket_fd == -1) {
        perror("Server socket was not opened");
        exit(EXIT_FAILURE);
    }

    printf("Game started! Listening for players on port %d", port);

    // This is inefficient, since connecting and disconnecting causes loss of a spot
    // but we're doing it for the sake of simplicity
    pthread_t listen_threads [MAX_PLAYERS];
    args_t args [MAX_PLAYERS];
    numPlayers = 0;

    char* userInput;
    while (strcmp(userInput, "s") != 0 && numPlayers < MAX_PLAYERS){
        scanf("%s", userInput);
        // Wait for a client to connect
        int client_socket_fd = server_socket_accept(server_socket_fd);
        // check if client socket is not connected or number of peers would exceed limit
        if (client_socket_fd == -1) {
            perror("accept failed");
            exit(EXIT_FAILURE);
        }
        // printf("Player #%d connected!\n", numPlayers + 1);

         // set index to first open index
        args[numPlayers].client_socket = client_socket_fd;
        args[numPlayers].index = numPlayers; // store client socket for thread
         // store client socket for thread
        // fds[numPlayers] = client_socket_fd; // store client socket
        // create a thread
        if (pthread_create(&listen_threads[numPlayers++], NULL, &listen_init, &(args[numPlayers]))) {
            perror("pthread_create failed");
            exit(2);
        } // if
    } // while








    // if(strcmp(type,"passwords") != 0){
    //     continue;
    // }

    // if(strcmp(type,"plain") != 0){
    //     continue;
    // }
}


void * listen_init(void* input_args){
    args_t* args = (args_t*) input_args;
    int client_socket_fd = args->client_socket;
    char* recived_username;
    init_packet_t* info;
    char* recived_hash;

    // Read a message from the client
    info = receive_init(client_socket_fd);
    if (info == NULL){
        printf("WARNING: Recieved irregular initalization message from client %d with index %d", client_socket_fd, args->index);
    }
    recived_hash = info->passwordHash; // store password from packet 
    recived_username = info->username; // store username from packet

    // We will assume that the password has been verified to meet the criteria on the sending end
    pthread_mutex_lock(&passwordSet_lock);
    int index = (recived_hash[0] & numBucketsAndMask);
    while(passwords[index].hashed_password[0] != 0){
        (index++) % (numBucketsAndMask + 1);
    }
    memcpy(&(passwords[index].hashed_password), recived_hash, MD5_DIGEST_LENGTH);
    strcpy((passwords[index].username), recived_username);
    pthread_mutex_unlock(&passwordSet_lock);

    printf("\"%s\" (#%d) is signed in and ready to play!", recived_username, args->index + 1);

    return NULL;
}