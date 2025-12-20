#ifndef VIEW_H
#define VIEW_H

#include "common.h"

void view_init();
void draw_game(const GameState* game_state, int my_player_id, int frame);
void gameOverScreen(int winner_id, int my_player_id, int score);
void singleGameOverScreen(int score, int level);

#endif 