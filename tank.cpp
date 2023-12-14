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

/**
 * In-memory representation of the game board:
 * Zero represents an empty cell
 * Positive numbers represent tanks (1's represent player 1, 2's represent player 2)
 * Negative numbers represent bullets (-1 and -3 represent player 1 bullets,
 * -2 and -4 represent player 2 bullets)
 */
int board[BOARD_HEIGHT][BOARD_WIDTH];
int player_num = 1;
int partner_fd = -1;
char* opponentUsername;
char* myUsername;

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

// Is the game running?
bool running = true;

// Did the player fire there weapon (Press Spacebar)?
bool fire_weapon = false;

// Did Player 1 win?
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
 * Show a game over message and that "Your" player won, and wait for a key press.
 */
void end_game()
{
  mvprintw(screen_row(BOARD_HEIGHT / 2) + 1, screen_col(BOARD_WIDTH / 2) - 6, "            ");
  mvprintw(screen_row(BOARD_HEIGHT / 2), screen_col(BOARD_WIDTH / 2) - 6, "       %s Wins!", myUsername);  //TODO: Print the winner's name here
  mvprintw(screen_row(BOARD_HEIGHT / 2) + 1, screen_col(BOARD_WIDTH / 2) - 6, "            ");
  mvprintw(screen_row(BOARD_HEIGHT / 2) + 2, screen_col(BOARD_WIDTH / 2) - 11,
           "Press any key to exit.");
  refresh();
  timeout(-1);
  task_readchar();
}

/**
 * Show a game over message and that the other player won and wait for a key press.
 */
void end_game_p2()
{
  mvprintw(screen_row(BOARD_HEIGHT / 2), screen_col(BOARD_WIDTH / 2) - 6, "     %s Wins! ", opponentUsername);
  mvprintw(screen_row(BOARD_HEIGHT / 2) + 1, screen_col(BOARD_WIDTH / 2) - 6, "            ");
  mvprintw(screen_row(BOARD_HEIGHT / 2) + 2, screen_col(BOARD_WIDTH / 2) - 11,
           "Press any key to exit.");
  refresh();
  timeout(-1);
  task_readchar();
}

/*
* Draw the tank on the board
* Paramters: int board[][]: 2d-array storing board values
             int *player_num: stores the how many tank spaces have been updated
             int r: The row where the rank is centered
             int c: The column where the tank is centered
             int tank_dir: The direction the tank faces
  Output: None, the board is updated to have the new tank position
*/
void draw_tank(int board[][BOARD_WIDTH], int *player_num, int r, int c, int tank_dir)
{
  if (tank_dir == DIR_NORTH){ // Is the tank facing north?
    if (*player_num == 0){ // Is this the first tank piece to draw
      mvaddch(screen_row(r), screen_col(c), '/');
      (*player_num)++;
    }
    else if (*player_num == 1){ // Is this the second tank piece to draw
      mvaddch(screen_row(r), screen_col(c), '|');
      (*player_num)++;
    }
    else if (*player_num == 2){ // Is this the third tank piece to draw
      mvaddch(screen_row(r), screen_col(c), '\\');
      (*player_num)++;
    }
    else{
      mvaddch(screen_row(r), screen_col(c), 'O');
    }
  }
  if (tank_dir == DIR_WEST){ // Is the tank facing west?
    if (*player_num == 0){ // Is this the first tank piece to draw
      mvaddch(screen_row(r), screen_col(c), '/');
    }
    else if (*player_num == 3){ // Is this the fourth tank piece to draw
      mvaddch(screen_row(r), screen_col(c), '-');
    }
    else if (*player_num == 6){ // Is this the seventh tank piece to draw
      mvaddch(screen_row(r), screen_col(c), '\\');
    }
    else{
      mvaddch(screen_row(r), screen_col(c), 'O');
    }
    (*player_num)++;
  }
  if (tank_dir == DIR_EAST){ // Is the tank facing east?
    if (*player_num == 2){ // Is this the third tank piece to draw
      mvaddch(screen_row(r), screen_col(c), '\\');
    }
    else if (*player_num == 5){ // Is this the sixth tank piece to draw
      mvaddch(screen_row(r), screen_col(c), '-');
    }
    else if (*player_num == 8){ // Is this the ninth tank piece to draw
      mvaddch(screen_row(r), screen_col(c), '/');
    }
    else{
      mvaddch(screen_row(r), screen_col(c), 'O');
    }
    (*player_num)++;
  }
  if (tank_dir == DIR_SOUTH){ // Is the tank facing south?
    if (*player_num == 6){// Is this the seventh tank piece to draw
      mvaddch(screen_row(r), screen_col(c), '\\');
    }
    else if (*player_num == 7){ // Is this the eigth tank piece to draw
      mvaddch(screen_row(r), screen_col(c), '|');
    }
    else if (*player_num == 8){ // Is this the ninth tank piece to draw
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
  if (tank_center_col + 2 < BOARD_WIDTH) // Check boundaries
  {
    // Check to ensure you don't overwrite other tank
    if (board[tank_center_row][tank_center_col +2] < 1 &&
        board[tank_center_row + 1][tank_center_col + 2] < 1 && 
        board[tank_center_row - 1][tank_center_col + 2] < 1)
    {
      board[tank_center_row][tank_center_col + 2] = player_num;
      board[tank_center_row - 1][tank_center_col + 2] = player_num;
      board[tank_center_row + 1][tank_center_col + 2] = player_num;
      board[tank_center_row][tank_center_col - 1] = 0;
      board[tank_center_row - 1][tank_center_col - 1] = 0;
      board[tank_center_row + 1][tank_center_col - 1] = 0;
    }
  }
}
/**
 * Move the tank left one space on the board
 */
void tank_left(int board[][BOARD_WIDTH], int tank_center_row, int tank_center_col)
{
  // Check boundaries
  if (tank_center_col - 2 > 0)
  {
    // Check to ensure you don't overwrite other tank
    if (board[tank_center_row][tank_center_col -2] < 1 &&
        board[tank_center_row + 1][tank_center_col - 2] < 1 &&
        board[tank_center_row - 1][tank_center_col - 2] < 1)
    {
      board[tank_center_row][tank_center_col - 2] = player_num;
      board[tank_center_row - 1][tank_center_col - 2] = player_num;
      board[tank_center_row + 1][tank_center_col - 2] = player_num;
      board[tank_center_row][tank_center_col + 1] = 0;
      board[tank_center_row - 1][tank_center_col + 1] = 0;
      board[tank_center_row + 1][tank_center_col + 1] = 0;
    }
  }
}

/**
 * Move the tank up one space on the board
 */
void tank_up(int board[][BOARD_WIDTH], int tank_center_row, int tank_center_col)
{
  // Check boundaries
  if (tank_center_row - 2 >= 0)
  {
    // Check to ensure you don't overwrite other tank
    if (board[tank_center_row - 2][tank_center_col] < 1 &&
        board[tank_center_row - 2][tank_center_col + 1] < 1 && 
        board[tank_center_row - 2][tank_center_col-1] < 1)
    {
      board[tank_center_row - 2][tank_center_col] = player_num;
      board[tank_center_row - 2][tank_center_col - 1] = player_num;
      board[tank_center_row - 2][tank_center_col + 1] = player_num;
      board[tank_center_row + 1][tank_center_col] = 0;
      board[tank_center_row + 1][tank_center_col - 1] = 0;
      board[tank_center_row + 1][tank_center_col + 1] = 0;
    }
  }
}

/**
 * Move the tank down one space on the board
 */
void tank_down(int board[][BOARD_WIDTH], int tank_center_row, int tank_center_col)
{
  // Check boundaries
  if (tank_center_row + 2 < BOARD_HEIGHT)
  {
    // Check to ensure you don't overwrite other tank
    if (board[tank_center_row + 2][tank_center_col] < 1 &&
        board[tank_center_row + 2][tank_center_col + 1] < 1 &&
        board[tank_center_row + 2][tank_center_col-1] < 1)
    {
      board[tank_center_row + 2][tank_center_col] = player_num;
      board[tank_center_row + 2][tank_center_col - 1] = player_num;
      board[tank_center_row + 2][tank_center_col + 1] = player_num;
      board[tank_center_row - 1][tank_center_col] = 0;
      board[tank_center_row - 1][tank_center_col - 1] = 0;
      board[tank_center_row - 1][tank_center_col + 1] = 0;
    }
  }
}

/**
 * Checks if the passed coordiante corresponds to a bullet or mine on the board
 * If it does, end the game
 */
void check_kill(int board[][BOARD_WIDTH], int row, int col)
{
  if (board[row][col] == 1) // Did player 1 collide with a bullet?
  {
    running = false; // end game
    ungetch(0);
  }
  else if (board[row][col] == 2) // Did player 2 collide with a bullet?
  {
    p1_winner = true;
    running = false; // end game
    ungetch(0);
  }
}

/**
 * Move the bullets on the board one space in the direction the tank is facing
 */
void move_weapon(int board[][BOARD_WIDTH], int row, int col)
{
  int weapon_dir = DIR_NORTH;
  if (player_num == 1){ // Is this player 1?
    weapon_dir = weapon_dir_p1;
  }
  else{
    weapon_dir = weapon_dir_p2;
  }
  if (weapon_dir == DIR_NORTH)
  {
    if (row - 1 >= 0) // Check board boundaries
    {
      check_kill(board, row - 1, col); // Has bullet collided with a tank?
      board[row - 1][col] = (-1 * player_num)-2; // update bullet on board
      board[row][col] = 0;
    }
    else
    {
      board[row][col] = 0; // Bullet has reached board edge, remove from board
    }
  }
  else if (weapon_dir == DIR_EAST)
  {
    if (col + 1 < BOARD_WIDTH) // Check board boundaries
    {
      check_kill(board, row, col + 1); // Has bullet collided with a tank?
      board[row][col + 1] = (-1 * player_num)-2; // update bullet on board
      board[row][col] = 0;
    }
    else
    {
      board[row][col] = 0; // Bullet has reached board edge, remove from board
    }
  }
  else if (weapon_dir == DIR_SOUTH)
  {
    if (row + 1 < BOARD_HEIGHT) // Check board boundaries
    {
      check_kill(board, row + 1, col); // Has bullet collided with a tank?
      board[row + 1][col] = (-1 * player_num)-2; // update bullet on board
      board[row][col] = 0;
    }
    else
    {
      board[row][col] = 0; // Bullet has reached board edge, remove from board
    }
  }
  else if (weapon_dir == DIR_WEST)
  {
    if (col - 1 > 0)// Check board boundaries
    {
      check_kill(board, row, col - 1); // Has bullet collided with a tank?
      board[row][col - 1] = (-1 * player_num)-2; // update bullet on board
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
        else if (board[r][c] == 1) 
        { 
          draw_tank(board, &draw_p1, r, c, tank_dir_p1); // Draw tank
        }
        else if (board[r][c] == 2) 
        { 
          draw_tank(board, &draw_p2, r, c, tank_dir_p2); // Draw tank
        }
        else if (board[r][c] < 0)
        {
          mvaddch(screen_row(r), screen_col(c), 'x'); // Draw bullet
        }
      }
    }

    // Reset values to draw tanks
    draw_p1 = 0;
    draw_p2 = 0;

    // Draw the score //@BRADLEY, REMOVE THIS?
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
    // Handle the key press and update player direction
    if (key == KEY_UP)
    {
      if (player_num == 1){ // Is this Player 1?
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
    int status = receive_and_update_screen(partner_fd, board, &opponent_dir);
    if(status != 1){
      running = false;
      ungetch(0);
      p1_winner = (status == 2); // We're ignoring that -1 is also an option here.
    }
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

    // Place a bullet on the board
    if (fire_weapon)
    {
      if (weapon_dir == DIR_NORTH)
      {
        board[tank_center_row - 3][tank_center_col] = (-1 * player_num);
        fire_weapon = false; 
      }
      else if (weapon_dir == DIR_EAST)
      {
        if (tank_center_col + 3 < BOARD_WIDTH){
          board[tank_center_row][tank_center_col + 3] = (-1 * player_num);
          fire_weapon = false;
        }
      }
      else if (weapon_dir == DIR_SOUTH)
      {
        board[tank_center_row + 3][tank_center_col] = (-1 * player_num);
        fire_weapon = false;
      }
      else if (weapon_dir == DIR_WEST)
      {
        if (tank_center_col - 3 > 0){
          board[tank_center_row][tank_center_col - 3] = (-1 * player_num);
          fire_weapon = false;
        }
      }
    }
    // Move the tank based on the direction the tank is facing
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

    // Get player num, and stop the tank after it has moved
    if (player_num == 1){
      updated_tank_dir_p1 = -1;
    }
    else{
      updated_tank_dir_p2 = -1;
    }

    // Loop back through and reset any bullets that were subtracted twice
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
    send_screen(partner_fd, 1, board, tank_dir); // send screen to other player
    // Update the worm movement speed to deal with rectangular cursors
    if (tank_dir == DIR_NORTH || tank_dir == DIR_SOUTH)
    {
      task_sleep(VERTICAL_INTERVAL);
    }
    else
    {
      task_sleep(HORIZONTAL_INTERVAL);
    }
  }
}

// Entry point: Set up the game, create jobs, then run the scheduler
void * tankMain(void * temp_args)
{
  WINDOW *mainwin; // window for ncurses
  tank_main_args_t* args;
  int games;
  if(temp_args != NULL){ // initialize player arguments
    args = (tank_main_args_t*) temp_args;
    player_num = args->player_num;
    partner_fd = args->partner_fd;
    opponentUsername = args->opponentUsername;
    myUsername = args->myUsername;
    mainwin = args->gameState;
    games = args->numGames;
  }
  else{
    perror("Args not recongized");
    return NULL;
  }

  // reset these values in case user plays multiple games
  running = true;
  p1_winner = false;
  
  // check if this is the first game
  if (games == 0){
    mainwin = initscr();
  }
  else{
    wrefresh(mainwin);
  }

  // Did ncurses window initialize correctly?
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

    // Put tank for player 1 in the top left of the board
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

  // Create tasks for each task in the game
  task_create(&draw_board_task, draw_board);
  task_create(&update_tank_task, update_tank);
  task_create(&read_input_task, read_input);

  // Wait for these tasks to exit
  task_wait(update_tank_task);
  task_wait(draw_board_task);
  task_wait(read_input_task);

  // Display the end of game message and wait for user input
  if (p1_winner){
    send_screen(partner_fd, 2, board, tank_dir_p2);
    args->winnerResult = 1;
    if (player_num == 1){
      end_game();
    }
    else{
      end_game_p2();
    }
    
  }
  else{
    send_screen(partner_fd, 3, board, tank_dir_p1);
    args->winnerResult = 2;
    if (player_num == 2){
      end_game();
    }
    else{
      end_game_p2();
    }
    
  }
  args->gameState = mainwin;
  // Clean up window
  endwin();

  return 0;
}
