#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <signal.h>
#include "network_common.h"
#include "item.h"

GameState game_state;
int client_sockets[MAX_PLAYERS] = {0};
pthread_mutex_t game_mutex = PTHREAD_MUTEX_INITIALIZER;
volatile int game_running = 1;
volatile sig_atomic_t special_wave_triggered = 0;
volatile sig_atomic_t redzone_triggered = 0;
volatile sig_atomic_t player_attack_triggered = 0;  // 플레이어 공격 트리거

// 알람 핸들러 (싱글플레이와 동일)
void alarm_handler(int sig) {
    signal(SIGALRM, alarm_handler);    
    static int alarm_count = 0;
    alarm_count++;
    
    if (alarm_count % 1 == 0) {
        special_wave_triggered = 1;
    }
    
    if (alarm_count % 2 == 0) {
        redzone_triggered = 1;
    }
}

// 5초마다 플레이어 공격 알람
void player_attack_alarm_handler(int sig) {
    signal(SIGALRM, player_attack_alarm_handler);
    player_attack_triggered = 1;
}

// 화살 생성 (싱글플레이와 동일)
void create_arrow(Arrow* arrow, int start_x, int start_y, int target_x, int target_y, int is_special) {
    arrow->x = start_x;
    arrow->y = start_y;
    arrow->is_special = is_special;
    arrow->owner = -1;  // 환경 공격
    
    int diff_x = target_x - start_x;
    int diff_y = target_y - start_y;

    if (diff_x > 0) arrow->dx = 1;
    else if (diff_x < 0) arrow->dx = -1;
    else arrow->dx = 0;

    if (diff_y > 0) arrow->dy = 1;
    else if (diff_y < 0) arrow->dy = -1;
    else arrow->dy = 0;

    if (is_special) {
        arrow->symbol = '#';
    } else {
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

// 브로드캐스트
void broadcast_packet(Packet* packet) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (client_sockets[i] > 0) {
            send(client_sockets[i], packet, sizeof(Packet), 0);
        }
    }
}

// 게임 초기화
void init_game() {
    memset(&game_state, 0, sizeof(GameState));
    
    game_state.players[0].x = 30;
    game_state.players[0].y = 12;
    game_state.players[0].player_id = 0;
    game_state.players[0].status.lives = 3;
    game_state.players[0].status.invincible_item = 1;
    game_state.players[0].status.heal_item = 1;
    game_state.players[0].status.slow_item = 1;
    
    game_state.players[1].x = 50;
    game_state.players[1].y = 12;
    game_state.players[1].player_id = 1;
    game_state.players[1].status.lives = 3;
    game_state.players[1].status.invincible_item = 1;
    game_state.players[1].status.heal_item = 1;
    game_state.players[1].status.slow_item = 1;
}

// 플레이어 360도 공격 생성
void create_player_attack(int player_id, int width, int height) {
    if (!game_state.players[player_id].connected || game_state.players[player_id].status.lives <= 0) {
        return;
    }
    
    int center_x = game_state.players[player_id].x;
    int center_y = game_state.players[player_id].y;
    
    // 8방향으로 화살 발사
    int directions[8][2] = {
        {1, 0},   // 오른쪽
        {-1, 0},  // 왼쪽
        {0, 1},   // 아래
        {0, -1},  // 위
        {1, 1},   // 오른쪽 아래
        {-1, 1},  // 왼쪽 아래
        {1, -1},  // 오른쪽 위
        {-1, -1}  // 왼쪽 위
    };
    
    for (int d = 0; d < 8; d++) {
        for (int i = 0; i < MAX_ARROWS; i++) {
            if (!game_state.arrows[i].active) {
                game_state.arrows[i].x = center_x;
                game_state.arrows[i].y = center_y;
                game_state.arrows[i].dx = directions[d][0];
                game_state.arrows[i].dy = directions[d][1];
                game_state.arrows[i].is_special = 2;  // 플레이어 공격 표시 (새로운 타입)
                game_state.arrows[i].owner = player_id;  // 공격 주인 저장
                
                // 화살 모양 설정
                if (game_state.arrows[i].dx == 1 && game_state.arrows[i].dy == 0) 
                    game_state.arrows[i].symbol = '>';
                else if (game_state.arrows[i].dx == -1 && game_state.arrows[i].dy == 0) 
                    game_state.arrows[i].symbol = '<';
                else if (game_state.arrows[i].dx == 0 && game_state.arrows[i].dy == 1) 
                    game_state.arrows[i].symbol = 'v';
                else if (game_state.arrows[i].dx == 0 && game_state.arrows[i].dy == -1) 
                    game_state.arrows[i].symbol = '^';
                else if (game_state.arrows[i].dx == 1 && game_state.arrows[i].dy == 1) 
                    game_state.arrows[i].symbol = '\\';
                else if (game_state.arrows[i].dx == -1 && game_state.arrows[i].dy == 1) 
                    game_state.arrows[i].symbol = '/';
                else if (game_state.arrows[i].dx == 1 && game_state.arrows[i].dy == -1) 
                    game_state.arrows[i].symbol = '/';
                else if (game_state.arrows[i].dx == -1 && game_state.arrows[i].dy == -1) 
                    game_state.arrows[i].symbol = '\\';
                
                game_state.arrows[i].active = 1;
                break;
            }
        }
    }
}

// 충돌 체크 (플레이어 공격 고려)
void check_collision(Player* player, int width, int height) {
    // 화살 충돌
    for (int i = 0; i < MAX_ARROWS; i++) {
        if (game_state.arrows[i].active) {
            if (game_state.arrows[i].x == player->x && 
                game_state.arrows[i].y == player->y) {
                // 플레이어 공격은 자기 자신에게는 데미지 안줌
                if (game_state.arrows[i].is_special == 2) {
                    if (game_state.arrows[i].owner != player->player_id) {
                        take_damage(&player->status);
                    }
                } else {
                    take_damage(&player->status);
                }
            }
        }
    }
    
    // 레드존 충돌
    for (int i = 0; i < MAX_REDZONES; i++) {
        if (game_state.redzones[i].active) {
            if (player->x >= game_state.redzones[i].x &&
                player->x < game_state.redzones[i].x + game_state.redzones[i].width &&
                player->y >= game_state.redzones[i].y &&
                player->y < game_state.redzones[i].y + game_state.redzones[i].height) {
                take_damage(&player->status);
            }
        }
    }
}

// 게임 루프 (싱글플레이 로직 기반)
void* game_loop(void* arg) {
    int width = 80;
    int height = 24;
    int alarm_started = 0;  // 알람 시작 플래그
    int player_attack_alarm_started = 0;  // 플레이어 공격 알람 플래그
    
    while (game_running) {
        // 두 플레이어가 모두 접속하면 알람 시작
        if (!alarm_started && game_state.players[0].connected && game_state.players[1].connected) {
            signal(SIGALRM, alarm_handler);
            alarm(10);
            alarm_started = 1;
            printf("게임 시작! 알람 타이머 시작\n");
        }
        
        // 플레이어 공격 타이머 시작 (5초마다)
        if (!player_attack_alarm_started && game_state.players[0].connected && game_state.players[1].connected) {
            // SIGALRM은 이미 사용 중이므로 프레임 카운트로 처리
            player_attack_alarm_started = 1;
        }
        pthread_mutex_lock(&game_mutex);
        
        // 생존자 확인
        int alive_count = 0;
        int connected_count = 0;
        int winner_id = -1;
        
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (game_state.players[i].connected) {
                connected_count++;
                if (game_state.players[i].status.lives > 0) {
                    alive_count++;
                    winner_id = i;
                }
            }
        }
        
        // 연결된 플레이어가 2명이고, 한 명만 살아있으면 승리
        if (connected_count == 2 && alive_count == 1) {
            Packet packet;
            packet.type = PACKET_GAME_OVER;
            packet.player_id = winner_id;
            broadcast_packet(&packet);
            
            printf("게임 종료! 플레이어 %d 승리!\n", winner_id);
            pthread_mutex_unlock(&game_mutex);
            
            alarm(0);  // 알람 정지
            sleep(5);
            
            pthread_mutex_lock(&game_mutex);
            memset(game_state.arrows, 0, sizeof(game_state.arrows));
            memset(game_state.redzones, 0, sizeof(game_state.redzones));
            game_state.frame = 0;
            game_state.special_wave_frame = 0;
            special_wave_triggered = 0;
            redzone_triggered = 0;
            alarm_started = 0;  // 알람 리셋
            player_attack_alarm_started = 0;  // 플레이어 공격 알람 리셋
            player_attack_alarm_started = 0;  // 플레이어 공격 알람 리셋
            
            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (game_state.players[i].connected) {
                    game_state.players[i].status.lives = 3;
                    game_state.players[i].status.invincible_item = 1;
                    game_state.players[i].status.heal_item = 1;
                    game_state.players[i].status.slow_item = 1;
                    game_state.players[i].status.is_invincible = 0;
                    game_state.players[i].status.invincible_frames = 0;
                    game_state.players[i].status.is_slow = 0;
                    game_state.players[i].status.slow_frames = 0;
                    game_state.players[i].status.damage_cooldown = 0;
                    game_state.players[i].score = 0;
                    game_state.players[i].x = (i == 0) ? 30 : 50;
                    game_state.players[i].y = 12;
                }
            }
            pthread_mutex_unlock(&game_mutex);
            continue;
        }
        
        // 둘 다 죽으면 무승부
        if (connected_count == 2 && alive_count == 0) {
            Packet packet;
            packet.type = PACKET_GAME_OVER;
            packet.player_id = -1;
            broadcast_packet(&packet);
            
            printf("게임 종료! 무승부!\n");
            pthread_mutex_unlock(&game_mutex);
            
            alarm(0);  // 알람 정지
            sleep(5);
            
            pthread_mutex_lock(&game_mutex);
            memset(game_state.arrows, 0, sizeof(game_state.arrows));
            memset(game_state.redzones, 0, sizeof(game_state.redzones));
            game_state.frame = 0;
            game_state.special_wave_frame = 0;
            special_wave_triggered = 0;
            redzone_triggered = 0;
            alarm_started = 0;  // 알람 리셋
            
            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (game_state.players[i].connected) {
                    game_state.players[i].status.lives = 3;
                    game_state.players[i].status.invincible_item = 1;
                    game_state.players[i].status.heal_item = 1;
                    game_state.players[i].status.slow_item = 1;
                    game_state.players[i].status.is_invincible = 0;
                    game_state.players[i].status.invincible_frames = 0;
                    game_state.players[i].status.is_slow = 0;
                    game_state.players[i].status.slow_frames = 0;
                    game_state.players[i].status.damage_cooldown = 0;
                    game_state.players[i].score = 0;
                    game_state.players[i].x = (i == 0) ? 30 : 50;
                    game_state.players[i].y = 12;
                }
            }
            pthread_mutex_unlock(&game_mutex);
            continue;
        }
        
        // 플레이어 상태 업데이트
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (game_state.players[i].connected && game_state.players[i].status.lives > 0) {
                if (game_state.players[i].status.invincible_frames > 0) {
                    game_state.players[i].status.invincible_frames--;
                    if (game_state.players[i].status.invincible_frames == 0) {
                        game_state.players[i].status.is_invincible = 0;
                    }
                }
                
                if (game_state.players[i].status.slow_frames > 0) {
                    game_state.players[i].status.slow_frames--;
                    if (game_state.players[i].status.slow_frames == 0) {
                        game_state.players[i].status.is_slow = 0;
                    }
                }
                
                if (game_state.players[i].status.damage_cooldown > 0) {
                    game_state.players[i].status.damage_cooldown--;
                }
                
                game_state.players[i].score++;
            }
        }
        
        // 5초마다 플레이어 공격 (100 프레임 = 5초)
        if (player_attack_alarm_started && game_state.frame % 100 == 0 && game_state.frame > 0) {
            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (game_state.players[i].connected && game_state.players[i].status.lives > 0) {
                    create_player_attack(i, width, height);
                }
            }
        }
        
        // 특수 웨이브 트리거 처리
        if (special_wave_triggered) {
            special_wave_triggered = 0;
            game_state.special_wave_frame = 60;
            alarm(10);
        }
        
        // 레드존 생성
        if (redzone_triggered) {
            redzone_triggered = 0;
            
            for (int i = 0; i < MAX_REDZONES; i++) {
                if (!game_state.redzones[i].active) {
                    game_state.redzones[i].width = 5 + rand() % 8;
                    game_state.redzones[i].height = 3 + rand() % 5;
                    game_state.redzones[i].x = 2 + rand() % (width - game_state.redzones[i].width - 3);
                    game_state.redzones[i].y = 2 + rand() % (height - game_state.redzones[i].height - 3);
                    game_state.redzones[i].lifetime = 200;
                    game_state.redzones[i].active = 1;
                    break;
                }
            }
        }
        
        int level = game_state.frame / 100;
        
        // 특수 화살 생성 (특수 웨이브 중)
        if (game_state.special_wave_frame > 0) {
            game_state.special_wave_frame--;
            
            if (rand() % 100 < 30 + level * 4) {
                for (int i = 0; i < MAX_ARROWS; i++) {
                    if (!game_state.arrows[i].active) {
                        int edge = rand() % 4;
                        int start_x, start_y;
                        
                        if (edge == 0) {
                            start_x = 1;
                            start_y = rand() % (height - 2) + 1;
                        } else if (edge == 1) {
                            start_x = width - 2;
                            start_y = rand() % (height - 2) + 1;
                        } else if (edge == 2) {
                            start_x = rand() % (width - 2) + 1;
                            start_y = 1;
                        } else {
                            start_x = rand() % (width - 2) + 1;
                            start_y = height - 2;
                        }
                        
                        int target_player = rand() % MAX_PLAYERS;
                        if (game_state.players[target_player].connected && 
                            game_state.players[target_player].status.lives > 0) {
                            create_arrow(&game_state.arrows[i], start_x, start_y, 
                                       game_state.players[target_player].x, 
                                       game_state.players[target_player].y, 1);
                        }
                        break;
                    }
                }
            }
        }
        
        // 일반 화살 생성
        if (rand() % 100 < 10 + level * 2) {
            for (int i = 0; i < MAX_ARROWS; i++) {
                if (!game_state.arrows[i].active) {
                    int edge = rand() % 4;
                    int start_x, start_y;
                    
                    if (edge == 0) {
                        start_x = 1;
                        start_y = rand() % (height - 2) + 1;
                    } else if (edge == 1) {
                        start_x = width - 2;
                        start_y = rand() % (height - 2) + 1;
                    } else if (edge == 2) {
                        start_x = rand() % (width - 2) + 1;
                        start_y = 1;
                    } else {
                        start_x = rand() % (width - 2) + 1;
                        start_y = height - 2;
                    }
                    
                    int target_player = rand() % MAX_PLAYERS;
                    if (game_state.players[target_player].connected && 
                        game_state.players[target_player].status.lives > 0) {
                        create_arrow(&game_state.arrows[i], start_x, start_y, 
                                   game_state.players[target_player].x, 
                                   game_state.players[target_player].y, 0);
                    }
                    break;
                }
            }
        }
        
        // 화살 이동 (슬로우 효과 적용)
        int is_slow = game_state.players[0].status.is_slow || 
                      game_state.players[1].status.is_slow;
        
        for (int i = 0; i < MAX_ARROWS; i++) {
            if (!game_state.arrows[i].active) continue;
            
            if (!is_slow || game_state.frame % 2 == 0) {
                game_state.arrows[i].x += game_state.arrows[i].dx;
                game_state.arrows[i].y += game_state.arrows[i].dy;
            }
            
            if (game_state.arrows[i].x <= 0 || game_state.arrows[i].x >= width - 1 ||
                game_state.arrows[i].y <= 0 || game_state.arrows[i].y >= height - 1) {
                game_state.arrows[i].active = 0;
            }
        }
        
        // 레드존 업데이트
        for (int i = 0; i < MAX_REDZONES; i++) {
            if (game_state.redzones[i].active) {
                game_state.redzones[i].lifetime--;
                if (game_state.redzones[i].lifetime <= 0) {
                    game_state.redzones[i].active = 0;
                }
            }
        }
        
        // 충돌 체크
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (game_state.players[i].connected && game_state.players[i].status.lives > 0) {
                check_collision(&game_state.players[i], width, height);
            }
        }
        
        game_state.frame++;
        
        // 게임 상태 브로드캐스트
        Packet packet;
        packet.type = PACKET_ARROW_UPDATE;
        memcpy(packet.data.arrows.arrows, game_state.arrows, sizeof(game_state.arrows));
        broadcast_packet(&packet);
        
        packet.type = PACKET_REDZONE_UPDATE;
        memcpy(packet.data.redzones.redzones, game_state.redzones, sizeof(game_state.redzones));
        broadcast_packet(&packet);
        
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (game_state.players[i].connected) {
                packet.type = PACKET_PLAYER_STATUS;
                packet.player_id = i;
                memcpy(&packet.data.player_status.player, &game_state.players[i], sizeof(Player));
                broadcast_packet(&packet);
            }
        }
        
        pthread_mutex_unlock(&game_mutex);
        
        usleep(50000);
    }
    
    return NULL;
}

// 클라이언트 핸들러
void* client_handler(void* arg) {
    int client_sock = *(int*)arg;
    free(arg);
    int player_id = -1;
    
    pthread_mutex_lock(&game_mutex);
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!game_state.players[i].connected) {
            player_id = i;
            game_state.players[i].connected = 1;
            client_sockets[i] = client_sock;
            break;
        }
    }
    pthread_mutex_unlock(&game_mutex);
    
    if (player_id == -1) {
        printf("서버 가득참\n");
        close(client_sock);
        return NULL;
    }
    
    printf("플레이어 %d 연결됨\n", player_id);
    
    Packet packet;
    packet.type = PACKET_INITIAL_STATE;
    packet.player_id = player_id;
    memcpy(&packet.data.initial_state.game_state, &game_state, sizeof(GameState));
    send(client_sock, &packet, sizeof(Packet), 0);
    
    while (game_running) {
        Packet recv_packet;
        int recv_size = recv(client_sock, &recv_packet, sizeof(Packet), 0);
        
        if (recv_size <= 0) {
            break;
        }
        
        pthread_mutex_lock(&game_mutex);
        
        switch (recv_packet.type) {
            case PACKET_PLAYER_MOVE:
                game_state.players[player_id].x = recv_packet.data.move.x;
                game_state.players[player_id].y = recv_packet.data.move.y;
                break;
                
            case PACKET_ITEM_USE:
                switch (recv_packet.data.item_use.item_type) {
                    case 1:
                        use_invincible_item(&game_state.players[player_id].status);
                        break;
                    case 2:
                        use_heal_item(&game_state.players[player_id].status);
                        break;
                    case 3:
                        use_slow_item(&game_state.players[player_id].status);
                        break;
                }
                break;
        }
        
        pthread_mutex_unlock(&game_mutex);
    }
    
    pthread_mutex_lock(&game_mutex);
    game_state.players[player_id].connected = 0;
    game_state.players[player_id].status.lives = 3;
    game_state.players[player_id].score = 0;
    client_sockets[player_id] = 0;
    pthread_mutex_unlock(&game_mutex);
    
    printf("플레이어 %d 연결 해제\n", player_id);
    close(client_sock);
    
    return NULL;
}

int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_size;
    pthread_t game_thread;
    
    srand(time(NULL));
    init_game();
    
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        perror("소켓 생성 실패");
        exit(1);
    }
    
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);
    
    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("바인드 실패");
        exit(1);
    }
    
    if (listen(server_sock, 5) == -1) {
        perror("리슨 실패");
        exit(1);
    }
    
    printf("서버 시작... 포트 %d\n", PORT);
    
    pthread_create(&game_thread, NULL, game_loop, NULL);
    
    while (game_running) {
        client_addr_size = sizeof(client_addr);
        client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_addr_size);
        
        if (client_sock == -1) {
            perror("연결 수락 실패");
            continue;
        }
        
        pthread_t client_thread;
        int* client_sock_ptr = malloc(sizeof(int));
        *client_sock_ptr = client_sock;
        pthread_create(&client_thread, NULL, client_handler, client_sock_ptr);
        pthread_detach(client_thread);
    }
    

    close(server_sock);
    return 0;
}