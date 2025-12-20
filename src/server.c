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

GameState state;
int client_socket[MAX_PLAYERS] = {0};
pthread_mutex_t game_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t game_cond = PTHREAD_COND_INITIALIZER;
volatile int game_running = 1;
volatile sig_atomic_t special_wave = 0;
volatile sig_atomic_t redzone = 0;

// 클라이언트 스레드 (최대 2개)
pthread_t client_threads[MAX_PLAYERS] = {0};

//화살 증가, 레드존 이벤트 처리를 위한 알람 핸들러
void event(int sig) {

    signal(SIGALRM, event);
    static int alarmCount = 0;
    alarmCount++;

    // 10초 마다 화살 증가
    if (alarmCount % 1 == 0) {
        state.special_wave = 60;
    }

    // 20초마다 레드존
    if (alarmCount % 2 == 0) {
       redZone(&state, GAME_WIDTH, GAME_HEIGHT);
    }
    alarm(10);
}

// 브로드캐스트
void send_packet(Packet* packet) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (client_socket[i] > 0) {
            write(client_socket[i], packet, sizeof(Packet));
        }
    }
}

// 연결 상태 브로드캐스트
void send_connection_status() {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (state.player[i].connected) {
            Packet packet;
            packet.type = PLAYER_STATUS;
            packet.id = i;
            memcpy(&packet.player, &state.player[i], sizeof(Player));
            send_packet(&packet);
        }
    }
}


void* game_loop(void* arg) {
    (void)arg;

    int start = 0;//게임 시작햇는지
    
    time_t connect_wait_time = 0;
    
    while (game_running) {
        pthread_mutex_lock(&game_mutex);
        
        // 연결된 플레이어 수 확인
        int connected = 0;
        if(state.player[0].connected)connected++;
        if(state.player[1].connected)connected++;
        
        // --- 게임 종료 조건 확인 ---
        int winner = -1;
        int game_over = 0;
        
        if (start && connected < 2) {
            // 1. 게임 중에 한 명이 나간 경우
            printf("플레이어 연결 끊김으로 게임 종료.\n");
            
            if(state.player[0].connected)winner=0;
            else if (state.player[1].connected)winner=1;

            game_over=1;
        } else if (connected == 2) {

            int alive_count = 0;
            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (state.player[i].connected && state.player[i].lives > 0) {
                    alive_count++;
                    winner = i;
                }
            }
            if (alive_count == 1 && start) {
                game_over=1;
            }
        }
        // --- 게임 종료 처리 ---
        if (game_over) {
            if (winner >= 0) printf("게임 종료! 플레이어 %d 승리!\n", winner);

            Packet packet;
            packet.type = GAME_OVER;
            packet.id = winner;
            send_packet(&packet);
            
            alarm(0);
            pthread_mutex_unlock(&game_mutex);
            
            sleep(5); // 클라이언트가 결과 확인하고 재시작할 시간
            
            pthread_mutex_lock(&game_mutex);
            init_game(&state, true);
            start = 0;
            connect_wait_time = 0;
            pthread_mutex_unlock(&game_mutex);
            continue;
        }

        // --- 대기 및 카운트다운 로직 (조건 변수 사용) ---
        while (connected < 2 && game_running) {
            connect_wait_time = 0;
            send_connection_status();
            pthread_cond_wait(&game_cond, &game_mutex);
            
            connected = 0;
            if(state.player[0].connected)connected++;
            if(state.player[1].connected)connected++;
        }
        
        if (!game_running) {
            pthread_mutex_unlock(&game_mutex);
            break;
        }

        if (connect_wait_time == 0) {
            connect_wait_time = time(NULL);
            printf("2명 접속 완료! 5초 후 게임 시작...\n");
        }

        // 5초 대기 (조건 변수 타임아웃 사용)
        struct timespec ts;
        ts.tv_sec = connect_wait_time + 5;
        ts.tv_nsec = 0;
        
        while (time(NULL) - connect_wait_time < 5 && game_running) {
            send_connection_status();
            pthread_cond_timedwait(&game_cond, &game_mutex, &ts);
        }
        
        if (!game_running) {
            pthread_mutex_unlock(&game_mutex);
            break;
        }

        // --- 게임 시작 ---
        if (!start) {
            signal(SIGALRM, event);
            alarm(10);
            start = 1;
            state.frame = 0; // 게임 시작 시 프레임 초기화
            printf("게임 로직 및 알람 시작!\n");
        }
        
        // --- 게임 진행 로직 ---
        update_game(&state, GAME_WIDTH, GAME_HEIGHT);
        
        // 5초마다 플레이어 공격
        if (start && state.frame > 0 && state.frame % 100 == 0) {
            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (state.player[i].connected && state.player[i].lives > 0) {
                    create_player_attack(&state, i);
                }
            }
        }
        
        // 특수 웨이브 트리거 처리
        if (special_wave) {
            state.special_wave = 60;
            special_wave = 0;
            alarm(10);
        }
        
        // 레드존 생성
        if (redzone) {
            redZone(&state, GAME_WIDTH, GAME_HEIGHT);
            redzone = 0;
        }
        
        // 게임 상태 전송 (화살,레드존 플레이어)
        Packet packet;
        packet.type = ARROW_UPDATE;
        memcpy(packet.arrows, state.arrow, sizeof(state.arrow));
        send_packet(&packet);
        
        packet.type = REDZONE_UPDATE;
        memcpy(packet.redzones, state.redzone, sizeof(state.redzone));
        send_packet(&packet);
        
        if (state.player[0].connected) {
            packet.type = PLAYER_STATUS;
            packet.id = 0;
            memcpy(&packet.player, &state.player[0], sizeof(Player));
            send_packet(&packet);
        }
        if (state.player[1].connected) {
            packet.type = PLAYER_STATUS;
            packet.id = 1;
            memcpy(&packet.player, &state.player[1], sizeof(Player));
            send_packet(&packet);
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
    int id = -1;
    
    pthread_mutex_lock(&game_mutex);
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!state.player[i].connected) {
            id = i;
            state.player[i].connected = 1;
            client_socket[i] = client_sock;
            break;
        }
    }
    pthread_cond_signal(&game_cond);  // 플레이어 접속 알림
    pthread_mutex_unlock(&game_mutex);
    
    if (id == -1) {
        printf("서버 가득참\n");
        close(client_sock);
        return NULL;
    }
    
    printf("플레이어 %d 연결됨\n", id);
    
    // 초기 상태 전송
    pthread_mutex_lock(&game_mutex);
    Packet packet;
    packet.type = INITIAL_STATE;
    packet.id = id;
    memcpy(&packet.game_state, &state, sizeof(GameState));
    pthread_mutex_unlock(&game_mutex);
    
    write(client_sock, &packet, sizeof(Packet));
    
    // 연결 상태 즉시 전송
    pthread_mutex_lock(&game_mutex);
    send_connection_status();
    pthread_mutex_unlock(&game_mutex);
    
    while (game_running) {
        Packet recv_packet;
        int recv_size = read(client_sock, &recv_packet, sizeof(Packet));
        
        if (recv_size <= 0) {
            break;
        }
        
        pthread_mutex_lock(&game_mutex);
        
        switch (recv_packet.type) {
            case PLAYER_MOVE:
                state.player[id].x = recv_packet.x;
                state.player[id].y = recv_packet.y;
                break;
                
            case ITEM_USE:
                switch (recv_packet.item_type) {
                    case 1: invincible_item(&state.player[id]); break;
                    case 2: heal_item(&state.player[id]); break;
                    case 3: slow_item(&state.player[id]); break;
                }
                break;
            default: break;
        }
        pthread_mutex_unlock(&game_mutex);
    }
    
    pthread_mutex_lock(&game_mutex);
    state.player[id].connected = 0;
    client_socket[id] = 0;
    pthread_cond_signal(&game_cond);  // 플레이어 연결 해제 알림
    pthread_mutex_unlock(&game_mutex);
    
    printf("플레이어 %d 연결 해제\n", id);
    close(client_sock);
    return NULL;
}

int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_size;
    pthread_t game_thread;
    
    srand(time(NULL));
    init_game(&state, true);
    
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
    
    printf("서버 시작 포트 %d\n", PORT);
    
    pthread_create(&game_thread, NULL, game_loop, NULL);
    
    while (game_running) {
        client_addr_size = sizeof(client_addr);
        client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_addr_size);
        
        if (client_sock == -1) {
            perror("연결 수락 실패");
            continue;
        }
        
        int* client_sock_ptr = malloc(sizeof(int));
        *client_sock_ptr = client_sock;
        
        // 플레이어 0 또는 1에 할당
        pthread_mutex_lock(&game_mutex);
        int slot = -1;

        if(!state.player[0].connected)slot=0;
        else if (!state.player[1].connected)slot=1;

        pthread_mutex_unlock(&game_mutex);
        
        if (slot != -1) {
            pthread_create(&client_threads[slot], NULL, client_handler, client_sock_ptr);
        } else {
            printf("서버 가득참\n");
            free(client_sock_ptr);
            close(client_sock);
        }
    }
    
    // 서버 종료 시 모든 스레드 join
    if (client_threads[0] != 0) pthread_join(client_threads[0], NULL);
    if (client_threads[1] != 0) pthread_join(client_threads[1], NULL);

    
    pthread_join(game_thread, NULL);
    close(server_sock);
    return 0;
}