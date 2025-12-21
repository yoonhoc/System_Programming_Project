#include "game_logic.h"
#include "item.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void create_arrow(Arrow* arrow, int start_x, int start_y, int target_x, int target_y, int special, int owner) {
    arrow->x = start_x;
    arrow->y = start_y;
    arrow->special = special;
    arrow->owner = owner;

    int diff_x = target_x - start_x;
    int diff_y = target_y - start_y;

    if (diff_x > 0) arrow->dx = 1;
    else if (diff_x < 0) arrow->dx = -1;
    else arrow->dx = 0;

    if (diff_y > 0) arrow->dy = 1;
    else if (diff_y < 0) arrow->dy = -1;
    else arrow->dy = 0;

    if (special == 2) { // Player attack
        if (arrow->dx == 1 && arrow->dy == 0) arrow->symbol = '>';
        else if (arrow->dx == -1 && arrow->dy == 0) arrow->symbol = '<';
        else if (arrow->dx == 0 && arrow->dy == 1) arrow->symbol = 'v';
        else if (arrow->dx == 0 && arrow->dy == -1) arrow->symbol = '^';
        else if (arrow->dx == 1 && arrow->dy == 1) arrow->symbol = '\\';
        else if (arrow->dx == -1 && arrow->dy == 1) arrow->symbol = '/';
        else if (arrow->dx == 1 && arrow->dy == -1) arrow->symbol = '/';
        else if (arrow->dx == -1 && arrow->dy == -1) arrow->symbol = '\\';
        else arrow->symbol = '*';
    } else if (special == 1) { // Special wave
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

void init_game(GameState* game_state, bool multiplay) {
    memset(game_state, 0, sizeof(GameState));
    game_state->multiplay = multiplay;
    srand(time(NULL));

    // Player 1
    game_state->player[0].x = multiplay ? 30 : GAME_WIDTH / 2;
    game_state->player[0].y = multiplay ? 12 : GAME_HEIGHT / 2;
    game_state->player[0].id = 0;
    game_state->player[0].lives = 3;
    game_state->player[0].invincible_item = 1;
    game_state->player[0].heal_item = 1;
    game_state->player[0].slow_item = 1;
   game_state->player[0].connected = !multiplay; // 멀티플레이에서는 0, 싱글플레이에서는 1로 초기화

    if (multiplay) {
        // Player 2
        game_state->player[1].x = 50;
        game_state->player[1].y = 12;
        game_state->player[1].id = 1;
        game_state->player[1].lives = 3;
        game_state->player[1].invincible_item = 1;
        game_state->player[1].heal_item = 1;
        game_state->player[1].slow_item = 1;
    }
}


void update_player(Player* player) {
    //무적상태이면
    if (player->invincible_frames > 0) {
        player->invincible_frames--; //무적 시간감소
        if (player->invincible_frames == 0) {//무적끝나면
            player->invincible = 0;
        }
    }
    if (player-> slow_frames > 0) {//슬로우 상태면
        player-> slow_frames--;
        if (player-> slow_frames == 0) {
            player-> slow = 0;
        }
    }
    if (player-> damage_cooldown > 0) {//화살 충돌 후 무적 상태면
        player->damage_cooldown--;
    }
    player->score++;
}

void update_arrows(GameState* state, int width, int height) {
    bool is_any_slow = false;
    
    for (int i = 0; i < 2; i++) {
        if (state->player[i].connected && state->player[i].slow) {
            is_any_slow = true;
            break;
        }
    }

    for (int i = 0; i < MAX_ARROWS; i++) {
        if (!state->arrow[i].active) continue;
        
        //슬로우 상태면 짝수프레임 일때만 (화살 1/2로 생성)
        //슬로우 상태아니면 정상 적으로 증가
        if (!is_any_slow || state->frame % 2 == 0) {
            state->arrow[i].x += state->arrow[i].dx;
            state->arrow[i].y += state->arrow[i].dy;
        }

        //화살이 벽만나면
        if (state->arrow[i].x <= 0 || state->arrow[i].x >= width - 1 ||
            state->arrow[i].y <= 0 || state->arrow[i].y >= height - 1) {
            state->arrow[i].active = 0;
        }
    }
}

void update_redzones(GameState* state) {
    for (int i = 0; i < MAX_REDZONES; i++) {
        if (state->redzone[i].active) {
            state->redzone[i].lifetime--;
            if (state->redzone[i].lifetime <= 0) {
                state->redzone[i].active = 0;
            }
        }
    }
}

void damage(Player* player) {
    if (player->invincible || player->damage_cooldown > 0) {
        return;
    }
    player->lives--;
    player->damage_cooldown = 40;
}

void check_collisions(GameState* state, int width, int height) {

    (void)width; 
    (void)height; 

    for (int idx = 0; idx < 2; idx++) {
        Player* player = &state->player[idx];
        if (!player->connected || player->lives <= 0) continue;

        for (int i = 0; i < MAX_ARROWS; i++) {
            if (state->arrow[i].active && state->arrow[i].x == player->x && state->arrow[i].y == player->y) {
                if (state->arrow[i].owner != player->id) {
                    damage(player);
                    state->arrow[i].active = 0;
                }
            }
        }

        for (int i = 0; i < MAX_REDZONES; i++) {
            if (state->redzone[i].active) {
                if (player->x >= state->redzone[i].x &&
                    player->x < state->redzone[i].x + state->redzone[i].width &&
                    player->y >= state->redzone[i].y &&
                    player->y < state->redzone[i].y + state->redzone[i].height) {
                    damage(player);
                }
            }
        }
    }
}

//화살 발사 함수
void spawn_arrow(GameState* state, int width, int height, bool is_special, int id) {

    for (int i = 0; i < MAX_ARROWS; i++) {

        if (!state->arrow[i].active) {
            //발사할 가장자리 랜덤 
            int edge = rand() % 4;

            int start_x, start_y;
            
            switch (edge) {
                case 0: // 왼쪽 가장자리
                    start_x = 1;
                    start_y = rand() % (height - 2) + 1;
                    break;
                case 1: // 오른쪽 가장자리
                    start_x = width - 2;
                    start_y = rand() % (height - 2) + 1;
                    break;
                case 2: // 위쪽 가장자리
                    start_x = rand() % (width - 2) + 1;
                    start_y = 1;
                    break;
                case 3: // 아래쪽 가장자리
                    start_x = rand() % (width - 2) + 1;
                    start_y = height - 2;
                    break;
            }

            if (state->player[id].connected && state->player[id].lives > 0) {
                create_arrow(&state->arrow[i], start_x, start_y,
                               state->player[id].x, state->player[id].y, is_special, -1);
            }
    
            //화살 생성했으면 종료
            break;
        }
    }
}


void redZone(GameState* state, int width, int height) {
    for (int i = 0; i < MAX_REDZONES; i++) {
        if (!state->redzone[i].active) {
            state->redzone[i].width = 5 + rand() % 8;
            state->redzone[i].height = 3 + rand() % 5;
            state->redzone[i].x = 2 + rand() % (width - state->redzone[i].width - 3);
            state->redzone[i].y = 2 + rand() % (height - state->redzone[i].height - 3);
            state->redzone[i].lifetime = 200;
            state->redzone[i].active = 1;
            break;
        }
    }
}

void create_player_attack(GameState* state, int id) {
    if (!state->player[id].connected || state->player[id].lives <= 0) {
        return;
    }

    int x = state->player[id].x;
    int y = state->player[id].y;

    
    if (state->multiplay) {
        
        //360 공격
        int directions[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {-1, 1}, {1, -1}, {-1, -1}};
        for (int d = 0; d < 8; d++) {

            for (int i = 0; i < MAX_ARROWS; i++) {
                if (!state->arrow[i].active) {
                    
                    create_arrow(&state->arrow[i], x, y, x + directions[d][0], y + directions[d][1], 2, id);
                    break;
                }
            }

        }

    } else {

        for (int i = 0; i < MAX_ARROWS; i++) {
            if (!state->arrow[i].active) {

                create_arrow(&state->arrow[i], x, y, x, y - 1, 2, id);
                break;
            }
        }
    }
}



void update_game(GameState* state, int width, int height) {
    for (int i = 0; i < 2; i++) {
        if (state->player[i].connected && state->player[i].lives > 0) {
            update_player(&state->player[i]);
        }
    }

    update_arrows(state, width, height);
    update_redzones(state);
    check_collisions(state, width, height);

    int level = state->frame / 100;
    int connected_players = 0;

    if(state->multiplay)connected_players=2;

    //화살 증가 중이면 증가 생성
    if (state->special_wave > 0) {
        state->special_wave--;
        if (rand() % 100 < 30 + level * 4) {
            
            //싱글이면 표적은 player 0아니면 랜덤
            int target_id = (connected_players > 1) ? (rand() % 2) : 0;
            spawn_arrow(state, width, height, true, target_id);
        }
    }

    //레벨에 맞는 화살생성
    if (rand() % 100 < 10 + level * 2) {
    
        int target_id = (connected_players > 1) ? (rand() % 2) : 0;
        spawn_arrow(state, width, height, false, target_id);
    }

    state->frame++;
}
