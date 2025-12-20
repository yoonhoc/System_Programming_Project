#ifndef GAME_LOGIC_H
#define GAME_LOGIC_H

#include "common.h"
#include <stdbool.h>

#define GAME_WIDTH 90
#define GAME_HEIGHT 26


void init_game(GameState* game_state, bool is_multiplayer);

void update_game(GameState* state, int width, int height);
void update_player(Player* player);
void update_arrows(GameState* state, int width, int height);
void update_redzones(GameState* state);
void damage(Player* player);
void check_collisions(GameState* state, int width, int height);

void spawn_arrow(GameState* state, int width, int height, bool is_special, int target_player_id);
void redZone(GameState* state, int width, int height);
void create_player_attack(GameState* state, int player_id);
void trigger_special_wave(GameState* state);

void update_game_world(GameState* state, int width, int height);

#endif