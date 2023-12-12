#ifndef MAIN_H
#define MAIN_H
#include "cracker-gpu.h"


#define MAX_PLAYERS 32

// This is how we'll check for repeat usernames and handle duplicated passwords
typedef struct login_pair_t {
    char username [MAX_USERNAME_LENGTH];
    // char* username;
    uint8_t hashed_password [MD5_DIGEST_LENGTH];
    login_pair_t* passwordBuddy;
    char solvedPassword[PASSWORD_LENGTH];
    int playerNum;
    int opponent;
    bool winner;
} login_pair_t;


#endif