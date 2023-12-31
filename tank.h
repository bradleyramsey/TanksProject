#ifndef TANK_H
#define TANK_H

#include <stdint.h>
#include <stdlib.h>
#include <curses.h>

// Game parameters
#define INIT_WORM_LENGTH 3
#define HORIZONTAL_INTERVAL 200
#define VERTICAL_INTERVAL 300
#define DRAW_BOARD_INTERVAL 33
#define APPLE_UPDATE_INTERVAL 120
#define READ_INPUT_INTERVAL 150
#define GENERATE_APPLE_INTERVAL 2000
#define BOARD_WIDTH 50
#define BOARD_HEIGHT 25

typedef struct{
    int player_num;
    int partner_fd;
    char* opponentUsername;
    char* myUsername;
    int winnerResult;
    WINDOW* gameState;
    int numGames;
} tank_main_args_t;

void * tankMain(void* none);


#endif
