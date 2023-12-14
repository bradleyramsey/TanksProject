#include "main.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <pthread.h>
#include <math.h>
#include <openssl/md5.h>
#include "socket.h"
#include "message.h"

#include "cracker-gpu.h"
#include "tank.h"
#include "util.h"

typedef struct {
  int client_socket;
  int index;
} args_t;


void guestMain(char* addr);
void hostMain(char * type);

int GameState;


/**
 * This just determines whether or not they're going to host the game, and if they're hosting, if they'd like
 * to change the type of instance they're hosting (tank/passwords/both)
*/
int main(int argc, char** argv) {
    printf("Welcome to the Title-less Tank Game!! If you would like to join an existing game, "
    "please type '<hostname>:<port>' otherwise, type 'host' to start your own!\n> ");

    // Game state coordinates between the threads on the host - 0 means the game hasn't started yet, 1 means it has.
    GameState = 0;

    bool done = false;
    while(!done){
        done = true;

        char hostname[32];
        char type[20];
        int args = scanf("%s", hostname);
        
        // Decide what roll this user is going to play.
        if(strcmp(hostname, "host") == 0){
            if(getchar() != ' '){
                hostMain(NULL);
            }
            else{
                hostMain(type);
            }
        }
        else if(strcmp(hostname, "tank") == 0){ // Can just play the tank. Was used for testing but I guess could be fun?
            tankMain(NULL);
        }
        else if(strchr(hostname, ':') != NULL){
            guestMain(hostname);
        }
        else{
            printf("Sorry, that input format wasn't recognized, try again!\n> ");
            done = false;
        }
    }


}

















/******************************| CLIENT STUFF |******************************/

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

    char hostname[64];
    long port;
    char delim[1] = {':'};
    
    // Parse the address of the central hub we're connecting too
    if(strchr(addr, ':') == addr){ // Let you just type ":<port>" to connect on localhost - made testing faster
        strcpy(hostname, "localhost");
        port = atoi(addr + 1);
    }
    else{ // Otherwise, get the hostname and port to connect later
        char* temp = strtok(addr, delim);
        temp = strtok(NULL, delim);
        strcpy(hostname, addr);
        port = atoi(temp);
    }
    

    // connect to host
    int host_socket_fd = socket_connect(hostname, port);
    if (host_socket_fd == -1) {
        printf("Connection failed. Restarting...\n");
        main(0, NULL); // Just loop them back to main if they didn't connect
    }
    bool acceptedUsername;
    char username[MAX_USERNAME_LENGTH];
    printf("You're connected!\nPlease enter a username so we know who you are: ");
    do{ // Loop until we have a valid username
        scanf("%s", username);

        bool valid = false;
        char password[MAX_USERNAME_LENGTH];
        
        do{
            printf("Now choose a 7 character password: "); 
            scanf("%s", password);
            if(strlen(password) != PASSWORD_LENGTH) // Check that it's exactly the right length
                continue;
            valid = true;
            for(int i = 0; i < PASSWORD_LENGTH; i++){
                if(!((password[i] >= 'a' && password[i] <= 'z') // Make sure it fits whatever the alphabet for the cracker is 
                    ||(password[i] >= '0' && password[i] <= '9')
//                     ||(password[i] >= 'A' && password[i] <= 'Z')
                    )){
                    valid = false;
                    continue;
                }
            }
        }while(!valid);

        uint8_t passwordHash[MD5_DIGEST_LENGTH];
        MD5((unsigned char*)password, strlen(password), passwordHash); // Their plaintext password never leaves the local machine
        
        send_init(host_socket_fd, username, passwordHash); // Send the login info to the central hub
        acceptedUsername = receive_check(host_socket_fd); // Wait to see if the username is unique
        if(!acceptedUsername){
            printf("Unfortunately, someone already has that username, please enter a new one: ");
        }
    } while(!acceptedUsername);

    pthread_t tankThread;
    pthread_t crackerThread;
    tank_main_args_t* tankArgs;

    char * opponentsUsername;
    int opponent_socket_fd;
    int games = 0; // How many times you've played
    bool competitionStillGoing = true;
    WINDOW * windowPointer = NULL; // We need to keep track of the window pointer so we can reopen it for the next rounds
    do{
        start_packet_t* startInfo = receive_start(host_socket_fd); // Get partner info from the hub.
                // Important nuance here: the hub threads only send start out to the player one users at first,
                // then they run the code below to start their listening, and send the info back to the hub.
                // At that point, it sends the start to the player 2s, and they connect to the new servers.


        if(startInfo->playerNum == 1 || startInfo->playerNum == 2){
            if(startInfo->playerNum == 1){
                // Start a listening socket for your opponent
                unsigned short our_port = 0;
                int server_socket_fd = server_socket_open(&our_port);
                if (server_socket_fd == -1) {
                    perror("Server socket was not opened");
                    exit(EXIT_FAILURE);
                }
                
                // Send the info of your new socket back to the host so they can forward it to your opponent
                char our_hostname[64];
                gethostname(our_hostname, 64); 
                send_start(host_socket_fd, 2, our_hostname, our_port, 0, 0); // Doesn't matter that these are 0s since we're sending to the host and it will overwrite before sending to the other client

                // Start listening for a connection
                if (listen(server_socket_fd, MAX_PLAYERS)) {
                    perror("listen failed");
                    exit(EXIT_FAILURE);
                }

                opponent_socket_fd = server_socket_accept(server_socket_fd);

                // check if client socket is not connected
                if (opponent_socket_fd == -1) {
                    perror("accept failed");
                    exit(EXIT_FAILURE);
                }

                // Once they connect, exchange greetings and then send the starting board
                send_greeting(opponent_socket_fd, username);
                opponentsUsername = receive_greeting(opponent_socket_fd);
                printf("You're battling %s, Get ready!", opponentsUsername);
            }
            else{
                // Connect to the provided socket
                opponent_socket_fd = socket_connect(startInfo->hostname, startInfo->port);
                if (opponent_socket_fd == -1) {
                    perror("Failed to connect");
                    exit(EXIT_FAILURE);
                }

                // Exchange greetings and start listening for the first board state
                opponentsUsername = receive_greeting(opponent_socket_fd);
                send_greeting(opponent_socket_fd, username);

                printf("You're battling %s, Get ready!\n", opponentsUsername);
                
                sleep(.5);
            }
            fflush(stdout); // We were getting issues with the threads not printing, so this makes sure.

            tankArgs = (tank_main_args_t*) malloc(sizeof(tank_main_args_t));

            // Get all the args ready for the tank game
            tankArgs->player_num = startInfo->playerNum;
            tankArgs->partner_fd = opponent_socket_fd;
            tankArgs->opponentUsername = opponentsUsername;
            tankArgs->myUsername = username;
            tankArgs->gameState = windowPointer;
            tankArgs->numGames = games;

            if (pthread_create(&tankThread, NULL, &tankMain, (void*) tankArgs)) {
                perror("pthread_create failed");
                exit(2);
            }
        }
        else if(startInfo->playerNum == 0){ // If we have an odd number of players the extra will run this
            printf("You're the odd player out, please wait for the next round");
            fflush(stdout);
        }
        else if(startInfo->playerNum == 3){ // This tells them that the tournament is over
            competitionStillGoing = false;
            multi_send_password_and_end(host_socket_fd, 1, 0, NULL, 3);
        }
        else{
            perror("Hmmm something fucked up");
        }
        
        // On the first time, set up the password cracking
        if(games == 0){
            // Receive passwords from host
            password_set_node_t* passwords;
            size_t numPasswordsToCrack = receive_and_update_password_list(host_socket_fd, &passwords);
        
            // Now run the password list cracker
            list_cracker_args_t* crackerArgs = (list_cracker_args_t*) malloc(sizeof(list_cracker_args_t));
            crackerArgs->argsPasswords = passwords;
            crackerArgs->numPasswords = numPasswordsToCrack;
            crackerArgs->index = startInfo->index;
            crackerArgs->numUsers = startInfo->numUsers;
            crackerArgs->host_fd = host_socket_fd;
            if (pthread_create(&crackerThread, NULL, &crack_password_list, (void*) crackerArgs)) {
                perror("pthread_create failed");
                exit(2);
            }
        }
        

        pthread_join(tankThread, NULL);
        multi_send_password_and_end(host_socket_fd, 1, 0, NULL, tankArgs->winnerResult); // Send that the game is over and who won
        windowPointer = tankArgs->gameState; // Save a reference to the initalized window so that we can reopen it
        games++;
        //TODO: Close old sockets?
    } while(competitionStillGoing);

    pthread_join(crackerThread, NULL);

    return;
}




























/******************************| HOST STUFF |******************************/

void* listen_connect(void* args);
void* listen_init(void* args);

typedef struct {
    int server_socket_fd;
}listen_connect_args_t;


pthread_mutex_t passwordSet_lock = PTHREAD_MUTEX_INITIALIZER;
int numPlayers;
password_set_node_t passwords[numBucketsAndMask + 1];
int numPasswordsTot;
size_t numPasswordsUnique;
int numCracked;

start_packet_t* threadExchange[MAX_PLAYERS];
login_pair_t userList[MAX_PLAYERS];
pthread_mutex_t userList_lock = PTHREAD_MUTEX_INITIALIZER;

int usersReady;
pthread_mutex_t userReady_lock = PTHREAD_MUTEX_INITIALIZER;

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
    // Initalize password cracking data structs
    for(int i = 0; i < numBucketsAndMask + 1; i++){
        passwords[i].hashed_password[0] = 0; // We use this later when traversing the hashmap, since LinkedList doesn't work to bring to the GPU easily
    }
    for(int i = 0; i < MAX_PLAYERS; i++){
        userList[i].username[0] = 0;
        userList[i].passwordBuddy = NULL; // We'll use these as LLs so we only have to crack duplicate passwords once
    }

    // Open up a socket for everyone in the class to connect to 
    unsigned short port = 0;
    int server_socket_fd = server_socket_open(&port);
    if (server_socket_fd == -1) {
        perror("Server socket was not opened");
        exit(EXIT_FAILURE);
    }

    printf("Game started! Listening for players on port \033[0;35m%d\033[0m\n", port);
    
    // Init variables
    numPlayers = 0;
    numPasswordsTot = 0;
    numPasswordsUnique = 0;
    numCracked = 0;


    if (listen(server_socket_fd, MAX_PLAYERS)) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    listen_connect_args_t connect_args;
    connect_args.server_socket_fd = server_socket_fd;

    pthread_t listen_connect_thread;
    if (pthread_create(&listen_connect_thread, NULL, &listen_connect, &connect_args)) {
        perror("pthread_create failed");
        exit(2);
    }

    // Wait for the user to signal that the game should start
    char userInput = '\0';
    while(userInput != 's'){
        userInput = getchar();
        
        // Check that # players == num passwords - if not, it probably means someone's not done checking in 
        if(userInput == 's' && numPlayers != numPasswordsTot){
            printf("It looks like not everyone is logged in fully. Are you sure you want to start? (s to start) ");

            int c;
            while ((c = getchar()) != '\n' && c != EOF) { } // Clear stdin. From: https://stackoverflow.com/questions/7898215/how-can-i-clear-an-input-buffer-in-c

            userInput = getchar();
            if(userInput != 's'){
                printf("Waiting\n");
            }

            while ((c = getchar()) != '\n' && c != EOF) { } // Clear stdin
        }
    }

    // Start the game now that the user is ready
    GameState = 1;
    usersReady = 0;

    // Wait a sec cause they check the game state after a bit of rest
    sleep(3);

    // Then get ready for everyone to rejoin
    GameState = 0;
    for(int j = 0; j < MAX_PLAYERS; j++){
        free(threadExchange[j]);
        threadExchange[j] = NULL;
    }

    // Then just loop this so we can do as many rounds as we want
    while(true){
        while(usersReady < numPlayers){ sleep(1); }

        printf("Looks like everyone is done with their games! Press 'c' to continue the tournament ");
        fflush(stdout);

        while(userInput != 'c'){
            userInput = getchar();
        }

        // Assign everyone their opponents
        int prevWinner = -1;
        int prevLoser = -1;
        for(int j = numPlayers - 1; j >= 0; j--){ // Start at top so last person can get in
            if(userList[j].winner){ // If they won
                if(prevWinner != -1){ // If there was a prev winner, have the winners play eachother
                    userList[j].opponent = prevWinner;
                    userList[prevWinner].opponent = j;
                    userList[j].playerNum = 2; // So the ordering is more likely to swap
                    userList[prevWinner].playerNum = 1;
                    prevWinner = -1;
                }
                else{ // Otherwise record this user as a winner
                    prevWinner = j;
                }
            }
            else{ // If they lost
                if(prevLoser != -1){ // If there was a prev loser, have the winners play eachother 
                    userList[j].opponent = prevLoser;
                    userList[prevLoser].opponent = j;
                    userList[j].playerNum = 2; 
                    userList[prevLoser].playerNum = 1;
                    prevLoser = -1;
                }
                else{ // Otherwise record this user as a winner
                    prevLoser = j;
                }
            }
        }

        // After the loop, if we have an excess winner and loser, pair them, if there were an odd # total, then someone sits out
        if(prevWinner != -1){
            if(prevLoser != -1){
                userList[prevLoser].opponent = prevWinner;
                userList[prevWinner].opponent = prevLoser;
                userList[prevLoser].playerNum = 2; 
                userList[prevWinner].playerNum = 1;
            }
            else{
                userList[prevWinner].playerNum = 1;
                userList[prevWinner].opponent = -1;
            }
            
        }
        else if(prevLoser != -1){
            userList[prevLoser].playerNum = 1;
            userList[prevLoser].opponent = -1;
        }
        
        // Now that assignments are done, start the game
        GameState = 1;
        usersReady = 0;

        // Wait for the threads to check, and reset to wait for the next one
        sleep(2);
        GameState = 0;
        userInput = '\0';
    }

    
    pthread_join(listen_connect_thread, NULL);
}

// This thread will listen for new people joining, but still allow you to start the game in the other thread
void * listen_connect(void* tempArgs){
    listen_connect_args_t* args = (listen_connect_args_t*) tempArgs;
    // This is inefficient, since connecting and disconnecting causes loss of a spot
    // but we're doing it for the sake of simplicity
    pthread_t listen_threads [MAX_PLAYERS];
    args_t listen_args [MAX_PLAYERS];

    while (true){
        fflush(stdout);
        // Wait for a client to connect
        int client_socket_fd = server_socket_accept(args->server_socket_fd);

        // check if client socket is not connected
        if (client_socket_fd == -1) {
            perror("accept failed");
            exit(EXIT_FAILURE);
        }
        if(GameState == 0){ // Don't let people connect while the game is being played. Idk what that would do.
            printf("Player #%d connected!\n", numPlayers + 1);

            listen_args[numPlayers].client_socket = client_socket_fd; // store client socket for thread
            listen_args[numPlayers].index = numPlayers; // store the thread's index for thread
            
            // create a thread
            if (pthread_create(&listen_threads[numPlayers++], NULL, &listen_init, &(listen_args[numPlayers]))) {
                perror("pthread_create failed");
                exit(2);
            } 
        }
    } 


    return NULL;
}


void * listen_init(void* input_args){
    // Unpack args
    args_t* args = (args_t*) input_args;
    int client_socket_fd = args->client_socket;
    int thread_index = args->index;

    char* recived_username;
    init_packet_t* info;
    uint8_t* recived_hash;

    bool valid;
    do{ // Loop until they have a valid username
        valid = true;
        // Read a message from the client
        info = receive_init(client_socket_fd);
        if (info == NULL){
            printf("WARNING: Recieved irregular initalization message from client %d with index %d", client_socket_fd, thread_index);
        }
        recived_hash = info->passwordHash; // store password from packet 
        recived_username = info->username; // store username from packet

        // Check that the username isn't already in use
        pthread_mutex_lock(&userList_lock);
        for(int i = 0; i < MAX_PLAYERS; i++){
            if(strcmp(recived_username, userList[i].username) == 0){
                send_check(client_socket_fd, false);
                valid = false;
                break;
            }
        }
        if(valid){
            send_check(client_socket_fd, true);
            memcpy(&(userList[thread_index].hashed_password), recived_hash, MD5_DIGEST_LENGTH);
            strcpy((userList[thread_index].username), recived_username);
        }
        else{
            free(info);
        }
        pthread_mutex_unlock(&userList_lock);
    } while(!valid);

    // We will assume that the password has been verified to meet the criteria on the sending end, 
    // but we still need to check if the password is a repeat too (at least has the same hash) 
    bool isNew = true;
    pthread_mutex_lock(&passwordSet_lock);
    for(int i = 0; i < MAX_PLAYERS; i++){ // Can't do this check in the username one cause what if there's an username conflict down the line after a password conflict
        if(memcmp(recived_hash, userList[i].hashed_password, MD5_DIGEST_LENGTH) == 0 && strcmp(recived_username, userList[i].username) != 0){
            login_pair_t *pal = &userList[i];
            while(pal->passwordBuddy != NULL){ // If there was a match, go to the end of the link list
                pal = pal->passwordBuddy;
            }
            pal->passwordBuddy = &(userList[thread_index]); // And add the new buddy
            isNew = false; // Signal that we don't have to add it to the list
            break;
        }
    }
    if(isNew){ // Add new passwords to the list
        int hash_index = (recived_hash[0] & numBucketsAndMask);
        while(passwords[hash_index].hashed_password[0] != 0){
            hash_index = (hash_index + 1) % (numBucketsAndMask + 1);
        }
        memcpy(&(passwords[hash_index].hashed_password), recived_hash, MD5_DIGEST_LENGTH);
        strcpy((passwords[hash_index].username), recived_username);
        numPasswordsUnique++;
    }
    numPasswordsTot++;
    pthread_mutex_unlock(&passwordSet_lock);

    printf("\"%s\" (#%d/%d) is signed in and ready to play!\n", recived_username, thread_index + 1, numPlayers);

    threadExchange[thread_index] = NULL;

    while(GameState == 0){sleep(1);} // Wait until the main thread starts the game

    // This is all for just the first time around
    printf("STARTING!");
    fflush(stdout);
    // ISSUE: deadlocks if someone leaves 
    // We have an array that corresponds to each thread/player with their assigned opponent
    // Send start to this thread's player
    if((thread_index % 2) == 0){
        if(thread_index == numPlayers - 1){ // If they're the excess, make them wait
            userList[thread_index].playerNum = 3;
            send_start(client_socket_fd, 0, NULL, 0, thread_index, numPlayers);
        }
        else{
            userList[thread_index].playerNum = 1;
            send_start(client_socket_fd, 1, NULL, 0, thread_index, numPlayers);
            start_packet_t* info = receive_start(client_socket_fd);
            threadExchange[thread_index + 1] = info; // Communicate between clients
        }
    }
    else{
        userList[thread_index].playerNum = 2;
        while(threadExchange[thread_index] == NULL){sleep(0.5);} // Have them wait for the partner info to be added
        send_start(client_socket_fd, 2, threadExchange[thread_index]->hostname, threadExchange[thread_index]->port, thread_index, numPlayers);
    }
    send_password_list(client_socket_fd, passwords, numPasswordsUnique); // Now that they're set up, send the passwords
    int start_time = time_ms();

    // This (and all below) will only end for one thread, so we don't have to worry about repeat printing
    // Also, if theres a thread that's the odd one out, and it has the passwords, we aren't going to get 
    // them until the next round has started since it'll block at the gamestate check the whole time
    while(numCracked < numPasswordsUnique){ 
        int type;
        int result = multi_recieve_password_and_end(client_socket_fd, passwords, &type); // No need to lock since no threads overlap
                // Also the function above is set up to recieve either the password match or the end game signal
                // so the code below does different stuff depending on what it actually recieves
        if(type == 0){ // If we get a password match
            pthread_mutex_lock(&passwordSet_lock);
            numCracked += result;
            pthread_mutex_unlock(&passwordSet_lock);
        }
        else if (type == 1){ // If we get a game over - this handles all the games after the first one
                             // this just means that sometimes the passwords don't print out until you've
                             // started the next round, which can mess with the reported efficiency
            pthread_mutex_lock(&userReady_lock);
            usersReady++;
            pthread_mutex_unlock(&userReady_lock);
            userList[thread_index].winner = (userList[thread_index].playerNum == result);
            printf("\"%s\" (%d/%d) is done with their game!\n", recived_username, usersReady, numPlayers);
            while(GameState == 0){ sleep(1); } // Same stuff, wait for the main thread to singal game start

            if(userList[thread_index].playerNum == 1){
                if(userList[thread_index].opponent == -1){ 
                    send_start(client_socket_fd, 0, NULL, 0, thread_index, numPlayers); // Tell them to wait this round
                }
                else{
                    send_start(client_socket_fd, 1, NULL, 0, thread_index, numPlayers);
                    start_packet_t* info = receive_start(client_socket_fd);
                    threadExchange[userList[thread_index].opponent] = info; // Communicate between clients
                }
            }
            else{
                while(threadExchange[thread_index] == NULL){sleep(0.5);}
                send_start(client_socket_fd, 2, threadExchange[thread_index]->hostname, threadExchange[thread_index]->port, thread_index, numPlayers);
            }
        }
        else{
            perror("Unknown type");
        }
    }
    int end_time = time_ms(); // If we exited the loop, the passwords must have been cracked
    for(int i = 0; i < (numBucketsAndMask + 1); i++){ // Copy over the passwords from the password list to the user list for later use
        if(passwords[i].hashed_password[0] != 0){ 
            for(int j = 0; j < MAX_PLAYERS; j++){
                if(strcmp(passwords[i].username, userList[j].username) == 0){
                    login_pair_t* user = &(userList[j]);
                    do{
                        memcpy(user->solvedPassword, passwords[i].solved_password, PASSWORD_LENGTH);
                        user = user->passwordBuddy;
                    } while (user != NULL); // Make sure we copy over the password for all the people who had the same one (hash)
                }
            }
        }
    }

    // Print out some interesting stats
    int time_s = (end_time - start_time) / 1000;
    size_t search_space = pow(ALPHABET_SIZE, PASSWORD_LENGTH);
    printf("\n\nTested \033[0;31m%ld passwords\033[0m in %d seconds.\n\n %d passwords per second\n %5.2f times faster than our top lab implementation\n\nPasswords:\n",
                search_space, time_s, search_space/time_s, (search_space/time_s)/(308915776/10.125));
    
    // Then print out the users' passwords
    for(int i = 0; i < MAX_PLAYERS; i++){
        if(userList[i].solvedPassword[0] != 0){
            printf("%s %.*s\n", userList[i].username, PASSWORD_LENGTH, userList[i].solvedPassword);
        }
    }
    printf("\n\n");
    fflush(stdout);

    int type;
    while(true){ // Now that we've cracked the passwords, just go back to looping the tank game for as long as people want to play
        int result = multi_recieve_password_and_end(client_socket_fd, passwords, &type);
        if(type == 0){ // If we get a password match
            pthread_mutex_lock(&passwordSet_lock);
            numCracked += result;
            pthread_mutex_unlock(&passwordSet_lock);
        }
        else if (type == 1){ // If we get a game over - this handles all the games after the first one
                             // this just means that sometimes the passwords don't print out until you've
                             // started the next round, which can mess with the reported efficiency
            pthread_mutex_lock(&userReady_lock);
            usersReady++;
            pthread_mutex_unlock(&userReady_lock);
            userList[thread_index].winner = (userList[thread_index].playerNum == result);
            printf("\"%s\" (%d/%d) is done with their game!\n", recived_username, usersReady, numPlayers);
            while(GameState == 0){ sleep(1); } // Same stuff, wait for the main thread to singal game start

            if(userList[thread_index].playerNum == 1){
                if(userList[thread_index].opponent == -1){ 
                    send_start(client_socket_fd, 0, NULL, 0, thread_index, numPlayers); // Tell them to wait this round
                }
                else{
                    send_start(client_socket_fd, 1, NULL, 0, thread_index, numPlayers);
                    start_packet_t* info = receive_start(client_socket_fd);
                    threadExchange[userList[thread_index].opponent] = info; // Communicate between clients
                }
            }
            else{
                while(threadExchange[thread_index] == NULL){sleep(0.5);}
                send_start(client_socket_fd, 2, threadExchange[thread_index]->hostname, threadExchange[thread_index]->port, thread_index, numPlayers);
            }
        }
        else{
            perror("Unknown type");
        }
    }

    return NULL;
}


/**
 * Here's the flow for beginning the game:
 * host                     Server                       Client 1                        Client 2
 * thread         main     connect     listen              main                           main
 *           create connect
 *                    listen for clients 
 *                                                        connect
 *                  create listen for client
 *                                                    prompt for username
 *                                                        send init
 *                                  recieve init        wait for start
 *                                  wait for main              
 *                                                                               same init process
 * 
 *             get 's' key
 *          change game state
 *                              send start if player 1
 *                                                      receive start
 *                                                       start server
 *                                                  send start w/ server info
 *                                  receieve start
 *                                  update global
 *                            player 2 listen waits for global
 *                                    send start
 *                                 send password list                     connect to client 1 server
 *                      wait for results of game or passwords  send greeting
 *                                                                                 send greeting
 *                                                     send board and play game
 *                                                   Main thread recieves passwords     same
 *                                                  Crack passwords. Send message either when done or on hit
*/

// ISSUE: Player two not being signed in causes a seg fault