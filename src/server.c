#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <signal.h>
#include "game_logic.h"
#include "item.h"
#include "common.h"

GameState game_state;
int client_sockets[MAX_PLAYERS] = {0};
pthread_mutex_t game_mutex = PTHREAD_MUTEX_INITIALIZER;
volatile int game_running = 1;
volatile sig_atomic_t special_wave_triggered = 0;
volatile sig_atomic_t redzone_triggered = 0;

// 알람 핸들러
void alarm_handler(int sig) {
    (void)sig;
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

// 브로드캐스트
void broadcast_packet(Packet* packet) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (client_sockets[i] > 0) {
            send(client_sockets[i], packet, sizeof(Packet), 0);
        }
    }
}

// 연결 상태 브로드캐스트
void broadcast_connection_status() {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (game_state.player[i].connected) {
            Packet packet;
            packet.type = PACKET_PLAYER_STATUS;
            packet.player_id = i;
            memcpy(&packet.data.player_status.player, &game_state.player[i], sizeof(Player));
            broadcast_packet(&packet);
        }
    }
}

// 게임 루프 (핵심 수정 부분)
void* game_loop(void* arg) {
    (void)arg;
    int alarm_started = 0;
    time_t connect_wait_time = 0;
    
    while (game_running) {
        pthread_mutex_lock(&game_mutex);
        
        // 연결된 플레이어 수 확인
        int connected_count = 0;
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (game_state.player[i].connected) {
                connected_count++;
            }
        }
        
        // --- 게임 종료 조건 확인 ---
        int game_over_reason = -2; // -2: 진행중, -1: 무승부, >=0: 승자 ID
        int last_alive_player = -1;

        if (alarm_started && connected_count < 2) {
            // 1. 게임 중에 한 명이 나간 경우
            printf("플레이어 연결 끊김으로 게임 종료.\n");
            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (game_state.player[i].connected) {
                    last_alive_player = i;
                    break;
                }
            }
            game_over_reason = last_alive_player; // 남은 사람이 승자
        } else if (connected_count == 2) {
            int alive_count = 0;
            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (game_state.player[i].connected && game_state.player[i].lives > 0) {
                    alive_count++;
                    last_alive_player = i;
                }
            }
            if (alive_count == 1 && alarm_started) {
                // 2. 한 명만 살아남은 경우
                game_over_reason = last_alive_player;
            } else if (alive_count == 0 && alarm_started) {
                // 3. 둘 다 죽은 경우
                game_over_reason = -1; // 무승부
            }
        }

        // --- 게임 종료 처리 ---
        if (game_over_reason != -2) {
            if (game_over_reason >= 0) printf("게임 종료! 플레이어 %d 승리!\n", game_over_reason);
            else printf("게임 종료! 무승부!\n");

            Packet packet;
            packet.type = PACKET_GAME_OVER;
            packet.player_id = game_over_reason;
            broadcast_packet(&packet);
            
            alarm(0);
            pthread_mutex_unlock(&game_mutex);
            
            sleep(5); // 클라이언트가 결과 확인하고 재시작할 시간
            
            pthread_mutex_lock(&game_mutex);
            init_game_state(&game_state, true);
            alarm_started = 0;
            connect_wait_time = 0;
            pthread_mutex_unlock(&game_mutex);
            continue;
        }

        // --- 대기 및 카운트다운 로직 ---
        if (connected_count < 2) {
            connect_wait_time = 0;
            broadcast_connection_status();
            pthread_mutex_unlock(&game_mutex);
            usleep(100000);
            continue;
        }

        if (connect_wait_time == 0) {
            connect_wait_time = time(NULL);
            printf("2명 접속 완료! 5초 후 게임 시작...\n");
        }

        // 5초 대기 (클라이언트의 카운트다운과 싱크를 맞춤)
        if (time(NULL) - connect_wait_time < 5) {
            broadcast_connection_status();
            pthread_mutex_unlock(&game_mutex);
            usleep(100000);
            continue;
        }

        // --- 게임 시작 ---
        if (!alarm_started) {
            signal(SIGALRM, alarm_handler);
            alarm(10);
            alarm_started = 1;
            game_state.frame = 0; // 게임 시작 시 프레임 초기화
            printf("게임 로직 및 알람 시작!\n");
        }
        
        // --- 게임 진행 로직 ---
        update_game_world(&game_state, GAME_WIDTH, GAME_HEIGHT);
        
        // 5초마다 플레이어 공격
        if (alarm_started && game_state.frame > 0 && game_state.frame % 100 == 0) {
            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (game_state.player[i].connected && game_state.player[i].lives > 0) {
                    create_player_attack(&game_state, i);
                }
            }
        }
        
        // 특수 웨이브 트리거 처리
        if (special_wave_triggered) {
            trigger_special_wave(&game_state);
            special_wave_triggered = 0;
            alarm(10);
        }
        
        // 레드존 생성
        if (redzone_triggered) {
            spawn_red_zone(&game_state, GAME_WIDTH, GAME_HEIGHT);
            redzone_triggered = 0;
        }
        
        // 게임 상태 브로드캐스트
        Packet packet;
        packet.type = PACKET_ARROW_UPDATE;
        memcpy(packet.data.arrows.arrows, game_state.arrow, sizeof(game_state.arrow));
        broadcast_packet(&packet);
        
        packet.type = PACKET_REDZONE_UPDATE;
        memcpy(packet.data.redzones.redzones, game_state.redzone, sizeof(game_state.redzone));
        broadcast_packet(&packet);
        
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (game_state.player[i].connected) {
                packet.type = PACKET_PLAYER_STATUS;
                packet.player_id = i;
                memcpy(&packet.data.player_status.player, &game_state.player[i], sizeof(Player));
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
        if (!game_state.player[i].connected) {
            player_id = i;
            game_state.player[i].connected = 1;
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
    
    // 초기 상태 전송
    pthread_mutex_lock(&game_mutex);
    Packet packet;
    packet.type = PACKET_INITIAL_STATE;
    packet.player_id = player_id;
    memcpy(&packet.data.initial_state.game_state, &game_state, sizeof(GameState));
    pthread_mutex_unlock(&game_mutex);
    
    send(client_sock, &packet, sizeof(Packet), 0);
    
    // 연결 상태 즉시 브로드캐스트
    pthread_mutex_lock(&game_mutex);
    broadcast_connection_status();
    pthread_mutex_unlock(&game_mutex);
    
    while (game_running) {
        Packet recv_packet;
        int recv_size = recv(client_sock, &recv_packet, sizeof(Packet), 0);
        
        if (recv_size <= 0) {
            break;
        }
        
        pthread_mutex_lock(&game_mutex);
        
        switch (recv_packet.type) {
            case PACKET_PLAYER_MOVE:
                game_state.player[player_id].x = recv_packet.data.move.x;
                game_state.player[player_id].y = recv_packet.data.move.y;
                break;
                
            case PACKET_ITEM_USE:
                switch (recv_packet.data.item_use.item_type) {
                    case 1: use_invincible_item(&game_state.player[player_id]); break;
                    case 2: use_heal_item(&game_state.player[player_id]); break;
                    case 3: use_slow_item(&game_state.player[player_id]); break;
                }
                break;
            default: break;
        }
        pthread_mutex_unlock(&game_mutex);
    }
    
    pthread_mutex_lock(&game_mutex);
    game_state.player[player_id].connected = 0;
    game_state.player[player_id].lives = 3;
    game_state.player[player_id].score = 0;
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
    init_game_state(&game_state, true);
    
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