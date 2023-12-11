#include "tank.h"
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
#include "message.h"

// Defines used to track the worm direction
#define DIR_NORTH 0
#define DIR_EAST 1
#define DIR_SOUTH 2
#define DIR_WEST 3

// Game parameters
#define PLAYER_1 1
#define PLAYER_2 2



// TODO: Handle loss on other side


/**
 * In-memory representation of the game board
 * Zero represents an empty cell
 * Positive numbers represent worm cells (which count up at each time step until they reach
 * worm_length) Negative numbers represent apple cells (which count up at each time step)
 */
int board[BOARD_HEIGHT][BOARD_WIDTH];
int player_num = 1;
int partner_fd = -1;

// Tank parameters for p1
int tank_dir_p1 = DIR_NORTH;
int updated_tank_dir_p1 = DIR_NORTH;
int tank_face_p1 = DIR_NORTH;
int weapon_dir_p1 = DIR_NORTH;

// Tank parameters for p2
int tank_dir_p2 = DIR_NORTH;
int updated_tank_dir_p2 = DIR_NORTH;
int tank_face_p2 = DIR_NORTH;
int weapon_dir_p2 = DIR_NORTH;


// Apple parameters
int apple_age = 120;

// Is the game running?
bool running = true;
bool fire_weapon = false;
bool p1_winner = false;
/**
 * Convert a board row number to a screen position
 * \param   row   The board row number to convert
 * \return        A corresponding row number for the ncurses screen
 */
int screen_row(int row)
{
  return 2 + row;
}

/**
 * Convert a board column number to a screen position
 * \param   col   The board column number to convert
 * \return        A corresponding column number for the ncurses screen
 */
int screen_col(int col)
{
  return 2 + col;
}

/**
 * Initialize the board display by printing the title and edges
 */
void init_display()
{
  // Print Title Line
  move(screen_row(-2), screen_col(BOARD_WIDTH / 2 - 5));
  addch(ACS_DIAMOND);
  addch(ACS_DIAMOND);
  printw(" Tanks! ");
  addch(ACS_DIAMOND);
  addch(ACS_DIAMOND);

  // Print corners
  mvaddch(screen_row(-1), screen_col(-1), ACS_ULCORNER);
  mvaddch(screen_row(-1), screen_col(BOARD_WIDTH), ACS_URCORNER);
  mvaddch(screen_row(BOARD_HEIGHT), screen_col(-1), ACS_LLCORNER);
  mvaddch(screen_row(BOARD_HEIGHT), screen_col(BOARD_WIDTH), ACS_LRCORNER);

  // Print top and bottom edges
  for (int col = 0; col < BOARD_WIDTH; col++)
  {
    mvaddch(screen_row(-1), screen_col(col), ACS_HLINE);
    mvaddch(screen_row(BOARD_HEIGHT), screen_col(col), ACS_HLINE);
  }

  // Print left and right edges
  for (int row = 0; row < BOARD_HEIGHT; row++)
  {
    mvaddch(screen_row(row), screen_col(-1), ACS_VLINE);
    mvaddch(screen_row(row), screen_col(BOARD_WIDTH), ACS_VLINE);
  }

  // Refresh the display
  refresh();
}

/**
 * Show a game over message and that Player 1 won, and wait for a key press.
 */
void end_game()
{
  mvprintw(screen_row(BOARD_HEIGHT / 2) + 1, screen_col(BOARD_WIDTH / 2) - 6, "            ");
  mvprintw(screen_row(BOARD_HEIGHT / 2), screen_col(BOARD_WIDTH / 2) - 6, "Player 1 Wins!"); 
  mvprintw(screen_row(BOARD_HEIGHT / 2) + 1, screen_col(BOARD_WIDTH / 2) - 6, "            ");
  mvprintw(screen_row(BOARD_HEIGHT / 2) + 2, screen_col(BOARD_WIDTH / 2) - 11,
           "Press any key to exit.");
  refresh();
  timeout(-1);
  task_readchar();
}

/**
 * Show a game over message and that Player 2 won and wait for a key press.
 */
void end_game_p2()
{
  mvprintw(screen_row(BOARD_HEIGHT / 2), screen_col(BOARD_WIDTH / 2) - 6, "Player 2 Wins!");
  mvprintw(screen_row(BOARD_HEIGHT / 2) + 1, screen_col(BOARD_WIDTH / 2) - 6, "            ");
  mvprintw(screen_row(BOARD_HEIGHT / 2) + 2, screen_col(BOARD_WIDTH / 2) - 11,
           "Press any key to exit.");
  refresh();
  timeout(-1);
  task_readchar();
}

void draw_tank(int board[][BOARD_WIDTH], int *player_num, int r, int c, int tank_dir)
{
  if (tank_dir == DIR_NORTH){
    if (*player_num == 0){
      mvaddch(screen_row(r), screen_col(c), '/');
      (*player_num)++;
    }
    else if (*player_num == 1){
      mvaddch(screen_row(r), screen_col(c), '|');
      (*player_num)++;
    }
    else if (*player_num == 2){
      mvaddch(screen_row(r), screen_col(c), '\\');
      (*player_num)++;
    }
    else{
      mvaddch(screen_row(r), screen_col(c), 'O');
    }
  }
  if (tank_dir == DIR_WEST){
    if (*player_num == 0){
      mvaddch(screen_row(r), screen_col(c), '/');
    }
    else if (*player_num == 3){
      mvaddch(screen_row(r), screen_col(c), '-');
    }
    else if (*player_num == 6){
      mvaddch(screen_row(r), screen_col(c), '\\');
    }
    else{
      mvaddch(screen_row(r), screen_col(c), 'O');
    }
    (*player_num)++;
  }
  if (tank_dir == DIR_EAST){
    if (*player_num == 2){
      mvaddch(screen_row(r), screen_col(c), '\\');
    }
    else if (*player_num == 5){
      mvaddch(screen_row(r), screen_col(c), '-');
    }
    else if (*player_num == 8){
      mvaddch(screen_row(r), screen_col(c), '/');
    }
    else{
      mvaddch(screen_row(r), screen_col(c), 'O');
    }
    (*player_num)++;
  }
  if (tank_dir == DIR_SOUTH){
    if (*player_num == 6){
      mvaddch(screen_row(r), screen_col(c), '\\');
    }
    else if (*player_num == 7){
      mvaddch(screen_row(r), screen_col(c), '|');
    }
    else if (*player_num == 8){
      mvaddch(screen_row(r), screen_col(c), '/');
    }
    else{
      mvaddch(screen_row(r), screen_col(c), 'O');
    }
    (*player_num)++;
  }
}

/**
 * Move the tank right one space on the board
 */
void tank_right(int board[][BOARD_WIDTH], int tank_center_row, int tank_center_col)
{
  if (tank_center_col + 2 < BOARD_WIDTH)
  {
    board[tank_center_row][tank_center_col + 2] = player_num;
    board[tank_center_row - 1][tank_center_col + 2] = player_num;
    board[tank_center_row + 1][tank_center_col + 2] = player_num;
    board[tank_center_row][tank_center_col - 1] = 0;
    board[tank_center_row - 1][tank_center_col - 1] = 0;
    board[tank_center_row + 1][tank_center_col - 1] = 0;
  }
}
/**
 * Move the tank left one space on the board
 */
void tank_left(int board[][BOARD_WIDTH], int tank_center_row, int tank_center_col)
{
  if (tank_center_col - 2 > 0)
  {
    board[tank_center_row][tank_center_col - 2] = player_num;
    board[tank_center_row - 1][tank_center_col - 2] = player_num;
    board[tank_center_row + 1][tank_center_col - 2] = player_num;
    board[tank_center_row][tank_center_col + 1] = 0;
    board[tank_center_row - 1][tank_center_col + 1] = 0;
    board[tank_center_row + 1][tank_center_col + 1] = 0;
  }
}

/**
 * Move the tank up one space on the board
 */
void tank_up(int board[][BOARD_WIDTH], int tank_center_row, int tank_center_col)
{
  if (tank_center_row - 2 >= 0)
  {
    board[tank_center_row - 2][tank_center_col] = player_num;
    board[tank_center_row - 2][tank_center_col - 1] = player_num;
    board[tank_center_row - 2][tank_center_col + 1] = player_num;
    board[tank_center_row + 1][tank_center_col] = 0;
    board[tank_center_row + 1][tank_center_col - 1] = 0;
    board[tank_center_row + 1][tank_center_col + 1] = 0;
  }
}

/**
 * Move the tank down one space on the board
 */
void tank_down(int board[][BOARD_WIDTH], int tank_center_row, int tank_center_col)
{
  if (tank_center_row + 2 < BOARD_HEIGHT)
  {
    board[tank_center_row + 2][tank_center_col] = player_num;
    board[tank_center_row + 2][tank_center_col - 1] = player_num;
    board[tank_center_row + 2][tank_center_col + 1] = player_num;
    board[tank_center_row - 1][tank_center_col] = 0;
    board[tank_center_row - 1][tank_center_col - 1] = 0;
    board[tank_center_row - 1][tank_center_col + 1] = 0;
  }
}

/**
 * Checks if the passed coordiante corresponds to a bullet or mine on the board
 * If it does, end the game
 */
void check_kill(int board[][BOARD_WIDTH], int row, int col)
{
  if (board[row][col] == 1)
  {
    running = false;
    ungetch(0);
  }
  else if (board[row][col] == 2)
  {
    p1_winner = true;
    running = false;
    ungetch(0);
  }
}

/**
 * Move the bullets on the board one space in the direction the tank is facing
 */
void move_weapon(int board[][BOARD_WIDTH], int row, int col)
{
  int weapon_dir = DIR_NORTH;
  if (player_num == 1){
    weapon_dir = weapon_dir_p1;
  }
  else{
    weapon_dir = weapon_dir_p2;
  }
  if (weapon_dir == DIR_NORTH)
  {
    if (row - 1 >= 0)
    {
      check_kill(board, row - 1, col); // Has bullet collided with a tank?
      board[row - 1][col] = (-1 * player_num)-2;
      board[row][col] = 0;
    }
    else
    {
      board[row][col] = 0; // Bullet has reached board edge, remove from board
    }
  }
  else if (weapon_dir == DIR_EAST)
  {
    if (col + 1 < BOARD_WIDTH)
    {
      check_kill(board, row, col + 1); // Has bullet collided with a tank?
      board[row][col + 1] = (-1 * player_num)-2;
      board[row][col] = 0;
    }
    else
    {
      board[row][col] = 0; // Bullet has reached board edge, remove from board
    }
  }
  else if (weapon_dir == DIR_SOUTH)
  {
    if (row + 1 < BOARD_HEIGHT)
    {
      check_kill(board, row + 1, col); // Has bullet collided with a tank?
      board[row + 1][col] = (-1 * player_num)-2;
      board[row][col] = 0;
    }
    else
    {
      board[row][col] = 0; // Bullet has reached board edge, remove from board
    }
  }
  else if (weapon_dir == DIR_WEST)
  {
    if (col - 1 > 0)
    {
      check_kill(board, row, col - 1); // Has bullet collided with a tank?
      board[row][col - 1] = (-1 * player_num)-2;
      board[row][col] = 0;
    }
    else
    {
      board[row][col] = 0; // Bullet has reached board edge, remove from board
    }
  }
}
/**
 * Run in a task to draw the current state of the game board.
 */
void draw_board()
{
  int draw_p1 = 0;
  int draw_p2 = 0;
  while (running)
  {
    // Loop over cells of the game board
    for (int r = 0; r < BOARD_HEIGHT; r++)
    {
      for (int c = 0; c < BOARD_WIDTH; c++)
      {
        if (board[r][c] == 0)
        { // Draw blank spaces
          mvaddch(screen_row(r), screen_col(c), ' ');
        }
        else if (board[r][c] == PLAYER_1) 
        { 
          draw_tank(board, &draw_p1, r, c, tank_dir_p1); // Draw tank
        }
        else if (board[r][c] == PLAYER_2) 
        { 
          draw_tank(board, &draw_p2, r, c, tank_dir_p2); // Draw tank
        }
        else if (board[r][c] < 0)
        {
          mvaddch(screen_row(r), screen_col(c), 'x'); // Draw bullet
        }
        // WE SHOULDNT NEED APPLES ANYMORE,  POSSIBLE WEAPONS
        // else {  // Draw apple spinner thing
        //   char spinner_chars[] = {'|', '/', '-', '\\'};
        //   mvaddch(screen_row(r), screen_col(c), spinner_chars[abs(board[r][c] % 4)]);
        // }
      }
    }
    draw_p1 = 0;
    draw_p2 = 0;

    // Draw the score
    mvprintw(screen_row(-2), screen_col(BOARD_WIDTH - 9), "Score %03d\r",
             3 - INIT_WORM_LENGTH);

    // Refresh the display
    refresh();

    // Sleep for a while before drawing the board again
    task_sleep(DRAW_BOARD_INTERVAL);
  }
}

/**
 * Run in a task to process user input.
 */
void read_input()
{
  while (running)
  {
    // Read a character, potentially blocking this task until a key is pressed
    int key = task_readchar();
    // Make sure the input was read correctly
    if (key == ERR)
    {
      running = false;
      fprintf(stderr, "ERROR READING INPUT\n");
    }
    // Handle the key press
    if (key == KEY_UP)
    {
      if (player_num == 1){
        updated_tank_dir_p1 = DIR_NORTH;
        tank_face_p1 = DIR_NORTH;
      }
      else{
        updated_tank_dir_p2 = DIR_NORTH;
        tank_face_p2 = DIR_NORTH;
      }
    }
    else if (key == KEY_RIGHT)
    {
      if (player_num == 1){
        updated_tank_dir_p1 = DIR_EAST;
        tank_face_p1 = DIR_EAST;
      }
      else{
        updated_tank_dir_p2 = DIR_EAST;
        tank_face_p2 = DIR_EAST;
      }
    }
    else if (key == KEY_DOWN)
    {
      if (player_num == 1){
        updated_tank_dir_p1 = DIR_SOUTH;
        tank_face_p1 = DIR_SOUTH;
      }
      else{
        updated_tank_dir_p2 = DIR_SOUTH;
        tank_face_p2 = DIR_SOUTH;
      }
    }
    else if (key == KEY_LEFT)
    {
      if (player_num == 1){
        updated_tank_dir_p1 = DIR_WEST;
        tank_face_p1 = DIR_WEST;
      }
      else{
        updated_tank_dir_p2 = DIR_WEST;
        tank_face_p2 = DIR_WEST;
      }
    }
    else if (key == ' ')
    {
      if (player_num == 1){
        fire_weapon = true;
        weapon_dir_p1 = tank_face_p1;
      }
      else{
        fire_weapon = true;
        weapon_dir_p2 = tank_face_p2;
      }
    }
    else if (key == 'q')
    {
      running = false;
    }
  }
}

/**
 * Run in a task to move the worm around on the board
 */
void update_tank()
{
  int tank_dir = DIR_NORTH;
  int opponent_dir;
  int weapon_dir = DIR_NORTH;
  while (running)
  {
    receive_and_update_screen(partner_fd, board, &opponent_dir);
    if (player_num == 1){
      tank_dir = updated_tank_dir_p1;
      tank_dir_p1 = updated_tank_dir_p1;
      tank_dir_p2 = opponent_dir;
      weapon_dir = weapon_dir_p1;
    }
    else{
      tank_dir = updated_tank_dir_p2;
      tank_dir_p2 = updated_tank_dir_p2;
      tank_dir_p1 = opponent_dir;
      weapon_dir = weapon_dir_p2;
    }
    int count = 0;
    int tank_center_row = 0;
    int tank_center_col = 0;
    // Loop over cells of the game board
    for (int r = 0; r < BOARD_HEIGHT; r++)
    {
      for (int c = 0; c < BOARD_WIDTH; c++)
      {
        if (board[r][c] == (-1 * player_num) ){  // is board coordinate a bullet?                          
          move_weapon(board, r, c); // move bullet
        }
        else if (board[r][c] == -3){
          board[r][c] = -1;
        }
        else if (board[r][c] == -4){
          board[r][c] = -2;
        }
        if (board[r][c] == player_num)
        {
          count++;
        }
        if (count == 5){ // Is this the tank center coordinate?
          tank_center_row = r; // Save center row
          tank_center_col = c; // Save center column
        }
      }
    }
    if (fire_weapon)
    {
      if (weapon_dir == DIR_NORTH)
      {
        board[tank_center_row - 3][tank_center_col] = (-1 * player_num);
        fire_weapon = false;
      }
      else if (weapon_dir == DIR_EAST)
      {
        board[tank_center_row][tank_center_col + 3] = (-1 * player_num);
        fire_weapon = false;
      }
      else if (weapon_dir == DIR_SOUTH)
      {
        board[tank_center_row + 3][tank_center_col] = (-1 * player_num);
        fire_weapon = false;
      }
      else if (weapon_dir == DIR_WEST)
      {
        board[tank_center_row][tank_center_col - 3] = (-1 * player_num);
        fire_weapon = false;
      }
    }
    if (tank_dir == DIR_NORTH)
    {
      tank_up(board, tank_center_row, tank_center_col);
    }
    else if (tank_dir == DIR_EAST)
    {
      tank_right(board, tank_center_row, tank_center_col);
    }
    else if (tank_dir == DIR_SOUTH)
    {
      tank_down(board, tank_center_row, tank_center_col);
    }
    else if (tank_dir == DIR_WEST)
    {
      tank_left(board, tank_center_row, tank_center_col);
    }
    if (player_num == 1){
      updated_tank_dir_p1 = -1;
    }
    else{
      updated_tank_dir_p2 = -1;
    }

    for (int r = 0; r < BOARD_HEIGHT; r++){
      for (int c = 0; c < BOARD_WIDTH; c++){
        if (board[r][c] == -3){
          board[r][c] = -1;
        }
        else if (board[r][c] == -4){
          board[r][c] = -2;
        }
      }
    }
    send_screen(partner_fd, 1, board, tank_dir);
    // Update the worm movement speed to deal with rectangular cursors
    if (tank_dir == DIR_NORTH || tank_dir == DIR_SOUTH)
    {
      task_sleep(WORM_VERTICAL_INTERVAL);
    }
    else
    {
      task_sleep(WORM_HORIZONTAL_INTERVAL);
    }
  }
}

/**
 * Run in a task to update all the apples on the board.
 */
// void update_apples() {
//   while (running) {
//     // "Age" each apple
//     for (int r = 0; r < BOARD_HEIGHT; r++) {
//       for (int c = 0; c < BOARD_WIDTH; c++) {
//         if (board[r][c] < 0) {  // Add one to each apple cell
//           board[r][c]++;
//         }
//       }
//     }

//     task_sleep(APPLE_UPDATE_INTERVAL);
//   }
// }

/**
 * Run in a task to generate apples on the board.
 */
// void generate_apple() {
//   while (running) {
//     bool inserted = false;
//     // Repeatedly try to insert an apple at a random empty cell
//     while (!inserted) {
//       int r = rand() % BOARD_HEIGHT;
//       int c = rand() % BOARD_WIDTH;

//       // If the cell is empty, add an apple
//       if (board[r][c] == 0) {
//         // Pick a random age between apple_age/2 and apple_age*1.5
//         // Negative numbers represent apples, so negate the whole value
//         board[r][c] = -((rand() % apple_age) + apple_age / 2);
//         inserted = true;
//       }
//     }
//     task_sleep(GENERATE_APPLE_INTERVAL);
//   }
// }

// Entry point: Set up the game, create jobs, then run the scheduler
void * tankMain(void * temp_args)
{
  if(temp_args != NULL){
    tank_main_args_t* args = (tank_main_args_t*) temp_args;
    player_num = args->player_num;
    partner_fd = args->partner_fd;
  }
  // Initialize the ncurses window
  WINDOW *mainwin = initscr();
  if (mainwin == NULL)
  {
    fprintf(stderr, "Error initializing ncurses.\n");
    exit(2);
  }

  // Seed random number generator with the time in milliseconds
  srand(time_ms());

  noecho();               // Don't print keys when pressed
  keypad(mainwin, true);  // Support arrow keys
  nodelay(mainwin, true); // Non-blocking keyboard access

  // Initialize the game display
  init_display();

  if(player_num == 1 || temp_args == NULL){
    // Zero out the board contents
    memset(board, 0, BOARD_WIDTH * BOARD_HEIGHT * sizeof(int));

    // Put tank for player 1 in the bottom right of the board
    board[0][1] = 1;
    board[0][2] = 1;
    board[0][3] = 1;
    board[1][1] = 1;
    board[1][2] = 1;
    board[1][3] = 1;
    board[2][1] = 1;
    board[2][2] = 1;
    board[2][3] = 1;

    // Put tank for player 2 in the bottom right of the board
    board[BOARD_HEIGHT - 1][BOARD_WIDTH - 3] = 2;
    board[BOARD_HEIGHT - 1][BOARD_WIDTH - 2] = 2;
    board[BOARD_HEIGHT - 1][BOARD_WIDTH - 1] = 2;
    board[BOARD_HEIGHT - 2][BOARD_WIDTH - 3] = 2;
    board[BOARD_HEIGHT - 2][BOARD_WIDTH - 2] = 2;
    board[BOARD_HEIGHT - 2][BOARD_WIDTH - 1] = 2;
    board[BOARD_HEIGHT - 3][BOARD_WIDTH - 3] = 2;
    board[BOARD_HEIGHT - 3][BOARD_WIDTH - 2] = 2;
    board[BOARD_HEIGHT - 3][BOARD_WIDTH - 1] = 2;

    send_screen(partner_fd, 1, board, 0);
  }
  else{
    int dummy;
    receive_and_update_screen(partner_fd, board, &dummy);
    send_screen(partner_fd, 1, board, 0);
  }

  // Task handles for each of the game tasks
  task_t update_tank_task;
  task_t draw_board_task;
  task_t read_input_task;
  // task_t update_apples_task;
  // task_t generate_apple_task;
  // task_t update_weapon_task;

  // Create tasks for each task in the game
  task_create(&draw_board_task, draw_board);
  task_create(&update_tank_task, update_tank);
  task_create(&read_input_task, read_input);
  // task_create(&update_apples_task, update_apples);
  // task_create(&generate_apple_task, generate_apple);
  // task_create(&update_weapon_task, update_weapon);

  // Wait for these tasks to exit
  task_wait(update_tank_task);
  task_wait(draw_board_task);
  task_wait(read_input_task);
  // task_wait(update_weapon_task);
  // task_wait(update_apples_task);
  // task_wait(update_weapon_task);

  // Don't wait for the generate_apple task because it sleeps for 2 seconds,
  // which creates a noticeable delay when exiting.
  // task_wait(generate_apple_task);

  // Display the end of game message and wait for user input
  if (p1_winner){
    end_game();
  }
  else{
    end_game_p2();
  }

  // Clean up window
  delwin(mainwin);
  endwin();

  return 0;
}
