#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include "cracker-gpu.h"
#define MD5_DIGEST_LENGTH 16


#define MAX_USERNAME_LENGTH 64
#define PASSWORD_LENGTH 7




int main(int argc, char** argv) {
    if(argc != 3) {
        print_usage(argv[0]);
        exit(1);
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


    return 0;
}









// int main(void) {
//   // Initialize the ncurses window
//   WINDOW* mainwin = initscr();
//   if (mainwin == NULL) {
//     fprintf(stderr, "Error initializing ncurses.\n");
//     exit(2);
//   }

//   // Seed random number generator with the time in milliseconds
//   srand(time_ms());

//   noecho();                // Don't print keys when pressed
//   keypad(mainwin, true);   // Support arrow keys
//   nodelay(mainwin, true);  // Non-blocking keyboard access

//   // Initialize the game display
//   init_display();

//   // Zero out the board contents
//   memset(board, 0, BOARD_WIDTH * BOARD_HEIGHT * sizeof(int));

//   // Put the worm at the middle of the board
//   board[BOARD_HEIGHT / 2][BOARD_WIDTH / 2] = 1;

//   // MY CHANGES ----------------------
//   // board[BOARD_HEIGHT / 2][(BOARD_WIDTH / 2)+1] = 1;
//   // board[BOARD_HEIGHT / 2][(BOARD_WIDTH / 2)-1] = 1;
//   // board[(BOARD_HEIGHT / 2)+1][(BOARD_WIDTH / 2)] = 1;
//   // board[(BOARD_HEIGHT / 2)-1][(BOARD_WIDTH / 2)] = 1;
//   // MY CHANGES ----------------------


//   // Task handles for each of the game tasks
//   task_t update_worm_task;
//   task_t draw_board_task;
//   task_t read_input_task;
//   task_t update_apples_task;
//   task_t generate_apple_task;

//   // Initialize the scheduler library
//   scheduler_init();

//   // Create tasks for each task in the game
//   task_create(&update_worm_task, update_worm);
//   task_create(&draw_board_task, draw_board);
//   task_create(&read_input_task, read_input);
//   task_create(&update_apples_task, update_apples);
//   task_create(&generate_apple_task, generate_apple);

//   // Wait for these tasks to exit
//   task_wait(update_worm_task);
//   task_wait(draw_board_task);
//   task_wait(read_input_task);
//   task_wait(update_apples_task);

//   // Don't wait for the generate_apple task because it sleeps for 2 seconds,
//   // which creates a noticeable delay when exiting.
//   // task_wait(generate_apple_task);

//   // Display the end of game message and wait for user input
//   end_game();

//   // Clean up window
//   delwin(mainwin);
//   endwin();

//   return 0;
// }
