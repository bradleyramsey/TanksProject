#include <curses.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "scheduler.h"
#include "util.h"

// Defines used to track the worm direction
#define DIR_NORTH 0
#define DIR_EAST 1
#define DIR_SOUTH 2
#define DIR_WEST 3

// Game parameters
#define INIT_WORM_LENGTH 3
#define WORM_HORIZONTAL_INTERVAL 200
#define WORM_VERTICAL_INTERVAL 300
#define DRAW_BOARD_INTERVAL 33
#define APPLE_UPDATE_INTERVAL 120
#define READ_INPUT_INTERVAL 150
#define GENERATE_APPLE_INTERVAL 2000
#define BOARD_WIDTH 50
#define BOARD_HEIGHT 25

/**
 * In-memory representation of the game board
 * Zero represents an empty cell
 * Positive numbers represent worm cells (which count up at each time step until they reach
 * worm_length) Negative numbers represent apple cells (which count up at each time step)
 */
int board[BOARD_HEIGHT][BOARD_WIDTH];

// Worm parameters
int worm_dir = DIR_NORTH;
int worm_length = INIT_WORM_LENGTH;
int updated_worm_dir = DIR_NORTH;

// Apple parameters
int apple_age = 120;

// Is the game running?
bool running = true;
bool keyPressed = true;
/**
 * Convert a board row number to a screen position
 * \param   row   The board row number to convert
 * \return        A corresponding row number for the ncurses screen
 */
int screen_row(int row) {
  return 2 + row;
}

/**
 * Convert a board column number to a screen position
 * \param   col   The board column number to convert
 * \return        A corresponding column number for the ncurses screen
 */
int screen_col(int col) {
  return 2 + col;
}

/**
 * Initialize the board display by printing the title and edges
 */
void init_display() {
  // Print Title Line
  move(screen_row(-2), screen_col(BOARD_WIDTH / 2 - 5));
  addch(ACS_DIAMOND);
  addch(ACS_DIAMOND);
  printw(" Worm! ");
  addch(ACS_DIAMOND);
  addch(ACS_DIAMOND);

  // Print corners
  mvaddch(screen_row(-1), screen_col(-1), ACS_ULCORNER);
  mvaddch(screen_row(-1), screen_col(BOARD_WIDTH), ACS_URCORNER);
  mvaddch(screen_row(BOARD_HEIGHT), screen_col(-1), ACS_LLCORNER);
  mvaddch(screen_row(BOARD_HEIGHT), screen_col(BOARD_WIDTH), ACS_LRCORNER);

  // Print top and bottom edges
  for (int col = 0; col < BOARD_WIDTH; col++) {
    mvaddch(screen_row(-1), screen_col(col), ACS_HLINE);
    mvaddch(screen_row(BOARD_HEIGHT), screen_col(col), ACS_HLINE);
  }

  // Print left and right edges
  for (int row = 0; row < BOARD_HEIGHT; row++) {
    mvaddch(screen_row(row), screen_col(-1), ACS_VLINE);
    mvaddch(screen_row(row), screen_col(BOARD_WIDTH), ACS_VLINE);
  }

  // Refresh the display
  refresh();
}

/**
 * Show a game over message and wait for a key press.
 */
void end_game() {
  mvprintw(screen_row(BOARD_HEIGHT / 2) - 1, screen_col(BOARD_WIDTH / 2) - 6, "            ");
  mvprintw(screen_row(BOARD_HEIGHT / 2), screen_col(BOARD_WIDTH / 2) - 6, " Game Over! ");
  mvprintw(screen_row(BOARD_HEIGHT / 2) + 1, screen_col(BOARD_WIDTH / 2) - 6, "            ");
  mvprintw(screen_row(BOARD_HEIGHT / 2) + 2, screen_col(BOARD_WIDTH / 2) - 11,
           "Press any key to exit.");
  refresh();
  timeout(-1);
  task_readchar();
}
/**
 * Move the tank right one space on the board
 */
 void tank_right(int board[][BOARD_WIDTH], int tank_center_row, int tank_center_col){
  if (tank_center_col+2 < BOARD_WIDTH){
    board[tank_center_row][tank_center_col+2] = 1;
    board[tank_center_row-1][tank_center_col+2] = 1;
    board[tank_center_row+1][tank_center_col+2] = 1;
    board[tank_center_row][tank_center_col-1] = 0;
    board[tank_center_row-1][tank_center_col-1] = 0;
    board[tank_center_row+1][tank_center_col-1] = 0;
  }
 }
/**
 * Move the tank left one space on the board
 */
void tank_left(int board[][BOARD_WIDTH], int tank_center_row, int tank_center_col){
  if (tank_center_col-2 > 0){
    board[tank_center_row][tank_center_col-2] = 1;
    board[tank_center_row-1][tank_center_col-2] = 1;
    board[tank_center_row+1][tank_center_col-2] = 1;
    board[tank_center_row][tank_center_col+1] = 0;
    board[tank_center_row-1][tank_center_col+1] = 0;
    board[tank_center_row+1][tank_center_col+1] = 0;
  }
}

/**
 * Move the tank up one space on the board
 */
void tank_up(int board[][BOARD_WIDTH], int tank_center_row, int tank_center_col){
  if (tank_center_row-2 >= 0){
    board[tank_center_row-2][tank_center_col] = 1;
    board[tank_center_row-2][tank_center_col-1] = 1;
    board[tank_center_row-2][tank_center_col+1] = 1;
    board[tank_center_row+1][tank_center_col] = 0;
    board[tank_center_row+1][tank_center_col-1] = 0;
    board[tank_center_row+1][tank_center_col+1] = 0;
  }
}

/**
 * Move the tank down one space on the board
 */
void tank_down(int board[][BOARD_WIDTH], int tank_center_row, int tank_center_col){
  if (tank_center_row+2 < BOARD_HEIGHT){
    board[tank_center_row+2][tank_center_col] = 1;
    board[tank_center_row+2][tank_center_col-1] = 1;
    board[tank_center_row+2][tank_center_col+1] = 1;
    board[tank_center_row-1][tank_center_col] = 0;
    board[tank_center_row-1][tank_center_col-1] = 0;
    board[tank_center_row-1][tank_center_col+1] = 0;
  }
}
/**
 * Run in a task to draw the current state of the game board.
 */
void draw_board() {
  int count = 0;
  while (running) {
    // Loop over cells of the game board
    for (int r = 0; r < BOARD_HEIGHT; r++) {
      for (int c = 0; c < BOARD_WIDTH; c++) {
        if (board[r][c] == 0) {  // Draw blank spaces
          mvaddch(screen_row(r), screen_col(c), ' ');
        } else if (board[r][c] > 0) {  // Draw tank
          if (worm_dir == DIR_NORTH){
            if (count == 0){
              mvaddch(screen_row(r), screen_col(c), '/');
              count++;
            }
            else if (count == 1){
              mvaddch(screen_row(r), screen_col(c), '|');
              count++;
            }
            else if (count == 2){
              mvaddch(screen_row(r), screen_col(c), '\\');
              count++;
            }
            else{
              mvaddch(screen_row(r), screen_col(c), 'O');
            }
          }
          if (worm_dir == DIR_WEST){
            if (count == 0){
              mvaddch(screen_row(r), screen_col(c), '/');
            }
            else if (count == 3){
              mvaddch(screen_row(r), screen_col(c), '-');
            }
            else if (count == 6){
              mvaddch(screen_row(r), screen_col(c), '\\');
            }
            else{
              mvaddch(screen_row(r), screen_col(c), 'O');
            }
            count++;
          }
          if (worm_dir == DIR_EAST){
            if (count == 2){
              mvaddch(screen_row(r), screen_col(c), '\\');
            }
            else if (count == 5){
              mvaddch(screen_row(r), screen_col(c), '-');
            }
            else if (count == 8){
              mvaddch(screen_row(r), screen_col(c), '/');
            }
            else{
              mvaddch(screen_row(r), screen_col(c), 'O');
            }
            count++;
          }
          if (worm_dir == DIR_SOUTH){
            if (count == 6){
              mvaddch(screen_row(r), screen_col(c), '\\');
            }
            else if (count == 7){
              mvaddch(screen_row(r), screen_col(c), '|');
            }
            else if (count == 8){
              mvaddch(screen_row(r), screen_col(c), '/');
            }
            else{
              mvaddch(screen_row(r), screen_col(c), 'O');
            }
            count++;
          }
        } 
        // WE SHOULDNT NEED APPLES ANYMORE,  POSSIBLE WEAPONS
        // else {  // Draw apple spinner thing
        //   char spinner_chars[] = {'|', '/', '-', '\\'};
        //   mvaddch(screen_row(r), screen_col(c), spinner_chars[abs(board[r][c] % 4)]);
        // }
      }
    }
    count = 0;

    // Draw the score
    mvprintw(screen_row(-2), screen_col(BOARD_WIDTH - 9), "Score %03d\r",
             worm_length - INIT_WORM_LENGTH);

    // Refresh the display
    refresh();

    // Sleep for a while before drawing the board again
    task_sleep(DRAW_BOARD_INTERVAL);
  }
}

/**
 * Run in a task to process user input.
 */
void read_input() {
  while (running) {
    // Read a character, potentially blocking this task until a key is pressed
    int key = task_readchar();

    // Make sure the input was read correctly
    if (key == ERR) {
      running = false;
      fprintf(stderr, "ERROR READING INPUT\n");
    }
    int count = 0;
    int tank_center_row = 0;
    int tank_center_col = 0;
    // Loop over cells of the game board
    for (int r = 0; r < BOARD_HEIGHT; r++) {
      for (int c = 0; c < BOARD_WIDTH; c++) {
        if (board[r][c] > 0){
          count++;
        }
        if (count == 5){
          tank_center_row = r;
          tank_center_col = c;
        }
      }
    }
    // Handle the key press
    if (key == KEY_UP) {
      updated_worm_dir = DIR_NORTH;
      // worm_dir = updated_worm_dir;
      // tank_up(board,tank_center_row, tank_center_col);
    } else if (key == KEY_RIGHT) {
      updated_worm_dir = DIR_EAST;
      // worm_dir = updated_worm_dir;
      // tank_right(board, tank_center_row, tank_center_col);
    } else if (key == KEY_DOWN) {
      updated_worm_dir = DIR_SOUTH;
      // worm_dir = updated_worm_dir;
      // tank_down(board, tank_center_row, tank_center_col);
    } else if (key == KEY_LEFT) {
      updated_worm_dir = DIR_WEST;
      // worm_dir = updated_worm_dir;
      // tank_left(board, tank_center_row, tank_center_col);
    } else if (key == 'q') {
      running = false;
    }
  }
}

/**
 * Run in a task to move the worm around on the board
 */
void update_worm() {
    while(running){
      worm_dir = updated_worm_dir;
      int count = 0;
      int tank_center_row = 0;
      int tank_center_col = 0;
      // Loop over cells of the game board
      for (int r = 0; r < BOARD_HEIGHT; r++) {
        for (int c = 0; c < BOARD_WIDTH; c++) {
          if (board[r][c] > 0){
            count++;
          }
          if (count == 5){
            tank_center_row = r;
            tank_center_col = c;
          }
        }
      }
      if (worm_dir == DIR_NORTH){
        tank_up(board, tank_center_row, tank_center_col);
      }
      else if (worm_dir == DIR_EAST){
        tank_right(board, tank_center_row, tank_center_col);
      }
      else if (worm_dir == DIR_SOUTH){
        tank_down(board, tank_center_row, tank_center_col);
      }
      else if (worm_dir == DIR_WEST){
        tank_left(board, tank_center_row, tank_center_col);
      }
      updated_worm_dir= -1; 
      // Update the worm movement speed to deal with rectangular cursors
      if (worm_dir == DIR_NORTH || worm_dir == DIR_SOUTH) {
        task_sleep(WORM_VERTICAL_INTERVAL);
      } else {
        task_sleep(WORM_HORIZONTAL_INTERVAL);
      }  
    }
  }

/**
 * Run in a task to update all the apples on the board.
 */
void update_apples() {
  while (running) {
    // "Age" each apple
    for (int r = 0; r < BOARD_HEIGHT; r++) {
      for (int c = 0; c < BOARD_WIDTH; c++) {
        if (board[r][c] < 0) {  // Add one to each apple cell
          board[r][c]++;
        }
      }
    }

    task_sleep(APPLE_UPDATE_INTERVAL);
  }
}

/**
 * Run in a task to generate apples on the board.
 */
void generate_apple() {
  while (running) {
    bool inserted = false;
    // Repeatedly try to insert an apple at a random empty cell
    while (!inserted) {
      int r = rand() % BOARD_HEIGHT;
      int c = rand() % BOARD_WIDTH;

      // If the cell is empty, add an apple
      if (board[r][c] == 0) {
        // Pick a random age between apple_age/2 and apple_age*1.5
        // Negative numbers represent apples, so negate the whole value
        board[r][c] = -((rand() % apple_age) + apple_age / 2);
        inserted = true;
      }
    }
    task_sleep(GENERATE_APPLE_INTERVAL);
  }
}

// Entry point: Set up the game, create jobs, then run the scheduler
int tankMain(void* none) {
  // Initialize the ncurses window
  WINDOW* mainwin = initscr();
  if (mainwin == NULL) {
    fprintf(stderr, "Error initializing ncurses.\n");
    exit(2);
  }

  // Seed random number generator with the time in milliseconds
  srand(time_ms());

  noecho();                // Don't print keys when pressed
  keypad(mainwin, true);   // Support arrow keys
  nodelay(mainwin, true);  // Non-blocking keyboard access

  // Initialize the game display
  init_display();

  // Zero out the board contents
  memset(board, 0, BOARD_WIDTH * BOARD_HEIGHT * sizeof(int));

  // Put the tank at the middle of the board
  board[BOARD_HEIGHT / 2][BOARD_WIDTH / 2] = 1;
  board[BOARD_HEIGHT / 2][(BOARD_WIDTH / 2)+1] = 1;
  board[BOARD_HEIGHT / 2][(BOARD_WIDTH / 2)-1] = 1;
  board[(BOARD_HEIGHT / 2) + 1][BOARD_WIDTH / 2] = 1;
  board[(BOARD_HEIGHT / 2) + 1][(BOARD_WIDTH / 2)+1] = 1;
  board[(BOARD_HEIGHT / 2) + 1][(BOARD_WIDTH / 2)-1] = 1;
  board[(BOARD_HEIGHT / 2) - 1][BOARD_WIDTH / 2] = 1;
  board[(BOARD_HEIGHT / 2) - 1][(BOARD_WIDTH / 2)+1] = 1;
  board[(BOARD_HEIGHT / 2) - 1][(BOARD_WIDTH / 2)-1] = 1;


  // Task handles for each of the game tasks
  task_t update_worm_task;
  task_t draw_board_task;
  task_t read_input_task;
  task_t update_apples_task;
  task_t generate_apple_task;

  // Initialize the scheduler library
  scheduler_init();

  // Create tasks for each task in the game
  task_create(&draw_board_task, draw_board);
    task_create(&update_worm_task, update_worm);
  task_create(&read_input_task, read_input);
  task_create(&update_apples_task, update_apples);
  task_create(&generate_apple_task, generate_apple);

  // Wait for these tasks to exit
  task_wait(update_worm_task);
  task_wait(draw_board_task);
  task_wait(read_input_task);
  task_wait(update_apples_task);

  // Don't wait for the generate_apple task because it sleeps for 2 seconds,
  // which creates a noticeable delay when exiting.
  // task_wait(generate_apple_task);

  // Display the end of game message and wait for user input
  end_game();

  // Clean up window
  delwin(mainwin);
  endwin();

  return 0;
}
