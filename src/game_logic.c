#include "game_logic.h"
#include "item.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

// This is a helper function, not exposed in the header
static void create_arrow_internal(Arrow* arrow, int start_x, int start_y, int target_x, int target_y, int is_special, int owner) {
    arrow->x = start_x;
    arrow->y = start_y;
    arrow->is_special = is_special;
    arrow->owner = owner;

    int diff_x = target_x - start_x;
    int diff_y = target_y - start_y;

    if (diff_x > 0) arrow->dx = 1;
    else if (diff_x < 0) arrow->dx = -1;
    else arrow->dx = 0;

    if (diff_y > 0) arrow->dy = 1;
    else if (diff_y < 0) arrow->dy = -1;
    else arrow->dy = 0;

    if (is_special == 2) { // Player attack
        if (arrow->dx == 1 && arrow->dy == 0) arrow->symbol = '>';
        else if (arrow->dx == -1 && arrow->dy == 0) arrow->symbol = '<';
        else if (arrow->dx == 0 && arrow->dy == 1) arrow->symbol = 'v';
        else if (arrow->dx == 0 && arrow->dy == -1) arrow->symbol = '^';
        else if (arrow->dx == 1 && arrow->dy == 1) arrow->symbol = '\\';
        else if (arrow->dx == -1 && arrow->dy == 1) arrow->symbol = '/';
        else if (arrow->dx == 1 && arrow->dy == -1) arrow->symbol = '/';
        else if (arrow->dx == -1 && arrow->dy == -1) arrow->symbol = '\\';
        else arrow->symbol = '*';
    } else if (is_special == 1) { // Special wave
        arrow->symbol = '#';
    } else { // Normal
        if (arrow->dx == 1 && arrow->dy == 0) arrow->symbol = '>';
        else if (arrow->dx == -1 && arrow->dy == 0) arrow->symbol = '<';
        else if (arrow->dx == 0 && arrow->dy == 1) arrow->symbol = 'v';
        else if (arrow->dx == 0 && arrow->dy == -1) arrow->symbol = '^';
        else if (arrow->dx == 1 && arrow->dy == 1) arrow->symbol = '\\';
        else if (arrow->dx == -1 && arrow->dy == 1) arrow->symbol = '/';
        else if (arrow->dx == 1 && arrow->dy == -1) arrow->symbol = '/';
        else if (arrow->dx == -1 && arrow->dy == -1) arrow->symbol = '\\';
        else arrow->symbol = '*';
    }
    arrow->active = 1;
}

void init_game_state(GameState* game_state, bool is_multiplayer) {
    memset(game_state, 0, sizeof(GameState));
    game_state->is_multiplayer = is_multiplayer;
    srand(time(NULL));

    // Player 1
    game_state->players[0].x = is_multiplayer ? 30 : GAME_WIDTH / 2;
    game_state->players[0].y = is_multiplayer ? 12 : GAME_HEIGHT / 2;
    game_state->players[0].player_id = 0;
    game_state->players[0].status.lives = 3;
    game_state->players[0].status.invincible_item = 1;
    game_state->players[0].status.heal_item = 1;
    game_state->players[0].status.slow_item = 1;
    game_state->players[0].connected = !is_multiplayer; // 멀티플레이에서는 0, 싱글플레이에서는 1로 초기화

    if (is_multiplayer) {
        // Player 2
        game_state->players[1].x = 50;
        game_state->players[1].y = 12;
        game_state->players[1].player_id = 1;
        game_state->players[1].status.lives = 3;
        game_state->players[1].status.invincible_item = 1;
        game_state->players[1].status.heal_item = 1;
        game_state->players[1].status.slow_item = 1;
    }
}

void update_player_status(Player* player) {
    if (player->status.invincible_frames > 0) {
        player->status.invincible_frames--;
        if (player->status.invincible_frames == 0) {
            player->status.is_invincible = 0;
        }
    }
    if (player->status.slow_frames > 0) {
        player->status.slow_frames--;
        if (player->status.slow_frames == 0) {
            player->status.is_slow = 0;
        }
    }
    if (player->status.damage_cooldown > 0) {
        player->status.damage_cooldown--;
    }
    player->score++;
}

void update_arrows(GameState* state, int width, int height) {
    bool is_any_slow = false;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (state->players[i].connected && state->players[i].status.is_slow) {
            is_any_slow = true;
            break;
        }
    }

    for (int i = 0; i < MAX_ARROWS; i++) {
        if (!state->arrows[i].active) continue;

        if (!is_any_slow || state->frame % 2 == 0) {
            state->arrows[i].x += state->arrows[i].dx;
            state->arrows[i].y += state->arrows[i].dy;
        }

        if (state->arrows[i].x <= 0 || state->arrows[i].x >= width - 1 ||
            state->arrows[i].y <= 0 || state->arrows[i].y >= height - 1) {
            state->arrows[i].active = 0;
        }
    }
}

void update_redzones(GameState* state) {
    for (int i = 0; i < MAX_REDZONES; i++) {
        if (state->redzones[i].active) {
            state->redzones[i].lifetime--;
            if (state->redzones[i].lifetime <= 0) {
                state->redzones[i].active = 0;
            }
        }
    }
}

void check_collisions(GameState* state, int width, int height) {
    (void)width;  // Unused parameter
    (void)height; // Unused parameter
    for (int p_idx = 0; p_idx < MAX_PLAYERS; p_idx++) {
        Player* player = &state->players[p_idx];
        if (!player->connected || player->status.lives <= 0) continue;

        for (int i = 0; i < MAX_ARROWS; i++) {
            if (state->arrows[i].active && state->arrows[i].x == player->x && state->arrows[i].y == player->y) {
                if (state->arrows[i].owner != player->player_id) {
                    take_damage(&player->status);
                    state->arrows[i].active = 0;
                }
            }
        }

        for (int i = 0; i < MAX_REDZONES; i++) {
            if (state->redzones[i].active) {
                if (player->x >= state->redzones[i].x &&
                    player->x < state->redzones[i].x + state->redzones[i].width &&
                    player->y >= state->redzones[i].y &&
                    player->y < state->redzones[i].y + state->redzones[i].height) {
                    take_damage(&player->status);
                }
            }
        }
    }
}

void spawn_arrow(GameState* state, int width, int height, bool is_special, int target_player_id) {
    for (int i = 0; i < MAX_ARROWS; i++) {
        if (!state->arrow[i].active) {
            int edge = rand() % 4;
            int start_x, start_y;

            if (edge == 0) { start_x = 1; start_y = rand() % (height - 2) + 1; }
            else if (edge == 1) { start_x = width - 2; start_y = rand() % (height - 2) + 1; }
            else if (edge == 2) { start_x = rand() % (width - 2) + 1; start_y = 1; }
            else { start_x = rand() % (width - 2) + 1; start_y = height - 2; }

            if (state->player[target_player_id].connected && state->player[target_player_id].status.lives > 0) {
                create_arrow_internal(&state->arrow[i], start_x, start_y,
                               state->players[target_player_id].x, state->player[target_player_id].y, is_special, -1);
            }
            break;
        }
    }
}


void create_player_attack(GameState* state, int player_id) {
    if (!state->players[player_id].connected || state->players[player_id].status.lives <= 0) {
        return;
    }

    int center_x = state->players[player_id].x;
    int center_y = state->players[player_id].y;

    if (state->is_multiplayer) {
        // 8-way attack for multiplayer
        int directions[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {-1, 1}, {1, -1}, {-1, -1}};
        for (int d = 0; d < 8; d++) {
            for (int i = 0; i < MAX_ARROWS; i++) {
                if (!state->arrows[i].active) {
                    create_arrow_internal(&state->arrows[i], center_x, center_y, center_x + directions[d][0], center_y + directions[d][1], 2, player_id);
                    break;
                }
            }
        }
    } else {
        // Single upward arrow for single player
        for (int i = 0; i < MAX_ARROWS; i++) {
            if (!state->arrows[i].active) {
                create_arrow_internal(&state->arrows[i], center_x, center_y, center_x, center_y - 1, 2, player_id);
                break;
            }
        }
    }
}



void update_game_world(GameState* state, int width, int height) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (state->players[i].connected && state->players[i].status.lives > 0) {
            update_player_status(&state->players[i]);
        }
    }

    update_arrows(state, width, height);
    update_redzones(state);
    check_collisions(state, width, height);

    int level = state->frame / 100;
    int connected_players = 0;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (state->players[i].connected) {
            connected_players++;
        }
    }

    if (state->special_wave_frame > 0) {
        state->special_wave_frame--;
        if (rand() % 100 < 30 + level * 4) {
            // In single player, always target player 0. Otherwise, random.
            int target_id = (connected_players > 1) ? (rand() % MAX_PLAYERS) : 0;
            spawn_arrow(state, width, height, true, target_id);
        }
    }

    if (rand() % 100 < 10 + level * 2) {
        // In single player, always target player 0. Otherwise, random.
        int target_id = (connected_players > 1) ? (rand() % MAX_PLAYERS) : 0;
        spawn_arrow(state, width, height, false, target_id);
    }

    state->frame++;
}
