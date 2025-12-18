#ifndef VIEW_H
#define VIEW_H

#include "common.h"
#include <curses.h>
#include <stdbool.h>

void view_init();
void view_teardown();
void view_draw_game_state(const GameState* game_state, int my_player_id, int frame);
void view_draw_game_over_screen(int winner_id, int my_player_id, int score);
void view_draw_single_player_game_over(int score, int level);
void view_draw_countdown(int seconds);
void view_draw_message_centered(const char* message, int line_offset);
void view_refresh();
void view_clear();

#endif