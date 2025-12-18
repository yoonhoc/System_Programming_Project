#ifndef GAME_LOGIC_H
#define GAME_LOGIC_H

#include "common.h"
#include <stdbool.h>

#define GAME_WIDTH 90
#define GAME_HEIGHT 26

// Initialization
void init_game_state(GameState* game_state, bool is_multiplayer);

// Update functions
void update_player_status(Player* player);
void update_arrows(GameState* state, int width, int height);
void update_redzones(GameState* state);
void check_collisions(GameState* state, int width, int height);

// Spawning functions
void spawn_arrow(GameState* state, int width, int height, bool is_special, int target_player_id);
void spawn_red_zone(GameState* state, int width, int height);
void create_player_attack(GameState* state, int player_id);
void trigger_special_wave(GameState* state);

void update_game_world(GameState* state, int width, int height);

#endif