#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <curses.h>
#include <termios.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <time.h>
#include "common.h"
#include "game_logic.h"
#include "view.h"

// 전역 변수
int server_sock;
GameState game_state;
int my_player_id = -1;
int game_over_flag = 0;
int winner_id = -1;
pthread_mutex_t state_mutex = PTHREAD_MUTEX_INITIALIZER;
volatile int game_running = 1;

// 서버 수신 스레드
void* receive_thread(void* arg) {
    (void)arg;
    Packet packet;
    
    while (game_running) {
        int recv_size = recv(server_sock, &packet, sizeof(Packet), 0);
        
        if (recv_size <= 0) {
            game_running = 0;
            break;
        }
        
        pthread_mutex_lock(&state_mutex);
        
        switch (packet.type) {
            case PACKET_INITIAL_STATE:
                my_player_id = packet.player_id;
                memcpy(&game_state, &packet.data.initial_state.game_state, sizeof(GameState));
                break;
            case PACKET_ARROW_UPDATE:
                memcpy(game_state.arrow, packet.data.arrows.arrows, sizeof(game_state.arrow));
                break;
            case PACKET_REDZONE_UPDATE:
                memcpy(game_state.redzone, packet.data.redzones.redzones, sizeof(game_state.redzone));
                break;
            case PACKET_PLAYER_STATUS:
                if (packet.player_id >= 0 && packet.player_id < MAX_PLAYERS) {
                    memcpy(&game_state.player[packet.player_id], 
                           &packet.data.player_status.player, sizeof(Player));
                }
                break;
            case PACKET_GAME_OVER:
                game_over_flag = 1;
                winner_id = packet.player_id;
                game_running = 0;
                break;
            default:
                break;
        }
        
        pthread_mutex_unlock(&state_mutex);
    }
    return NULL;
}

// 게임 리셋 함수
void reset_game_state() {
    memset(&game_state, 0, sizeof(GameState));
    my_player_id = -1;
    game_over_flag = 0;
    winner_id = -1;
    game_running = 1;
}

// 카운트다운 표시 함수
void show_countdown() {
    for (int i = 5; i > 0; i--) {
        view_clear();
        char msg[50];
        sprintf(msg, "Game starts in %d...", i);
        view_draw_message_centered(msg, 0);
        view_refresh();
        sleep(1);
    }
    
    view_clear();
    view_draw_message_centered("START!", 0);
    view_refresh();
    sleep(1);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("사용법: %s <서버IP>\n", argv[0]);
        return 1;
    }

    // ncurses 초기화
    view_init();
    
    // 메인 게임 재시작 루프
    while (1) {
        reset_game_state();

        // 소켓 생성 및 연결
        server_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (server_sock < 0) {
            endwin();
            perror("소켓 생성 실패");
            return 1;
        }

        // TCP_NODELAY 설정
        int flag = 1;
        setsockopt(server_sock, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));

        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = inet_addr(argv[1]);
        server_addr.sin_port = htons(PORT);

        if (connect(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            endwin();
            perror("서버 연결 실패");
            return 1;
        }

        // 수신 스레드 시작
        pthread_t recv_thread_id;
        pthread_create(&recv_thread_id, NULL, receive_thread, NULL);

        int frame = 0;

        // 플레이어 ID 할당 대기
        view_clear();
        view_draw_message_centered("Connecting to server...", 0);
        view_refresh();

        while (my_player_id == -1 && game_running) {
            usleep(50000);
        }
        
        if (!game_running) {
            close(server_sock);
            pthread_join(recv_thread_id, NULL);
            break;
        }

        // 내 연결 정보가 제대로 들어올 때까지 대기
        while (game_running) {
            pthread_mutex_lock(&state_mutex);
            int my_connected = game_state.player[my_player_id].connected;
            pthread_mutex_unlock(&state_mutex);
            
            if (my_connected) break;
            usleep(50000);
        }
        
        if (!game_running) {
            close(server_sock);
            pthread_join(recv_thread_id, NULL);
            break;
        }
        
        // 상대방 연결 대기 (여기서 멈춰있다가 서버가 상대 접속 정보를 보내면 풀림)
        int opponent_id = (my_player_id == 0) ? 1 : 0;
        
        view_clear();
        char msg[50];
        sprintf(msg, "Connected as Player %d!", my_player_id + 1);
        view_draw_message_centered(msg, 0);
        view_draw_message_centered("Waiting for other player...", 2);
        view_refresh();
        
        while (game_running) {
            pthread_mutex_lock(&state_mutex);
            int connected = game_state.player[opponent_id].connected;
            pthread_mutex_unlock(&state_mutex);
            
            if (connected) break;
            
            usleep(100000);
        }
        
        if (!game_running) {
            close(server_sock);
            pthread_join(recv_thread_id, NULL);
            break;
        }

        // 게임 시작 전 카운트다운
        show_countdown();

        // --- 메인 게임 루프 ---
        while (game_running && !game_over_flag) {
            int ch;
            Packet packet;
            int send_needed = 0;
            int move_key = ERR;
            int item_key = ERR;

            // 입력 처리
            while ((ch = getch()) != ERR) {
                if (ch == 'q' || ch == 'Q') {
                    game_running = 0;
                    break;
                }
                if (ch == KEY_LEFT || ch == KEY_RIGHT || ch == KEY_UP || ch == KEY_DOWN) {
                    move_key = ch;
                } else if (ch >= '1' && ch <= '3') {
                    item_key = ch;
                }
            }

            pthread_mutex_lock(&state_mutex);

            // 이동 처리
            if (move_key != ERR) {
                int dx = 0, dy = 0;
                if (move_key == KEY_LEFT && game_state.player[my_player_id].x > 1) dx = -1;
                else if (move_key == KEY_RIGHT && game_state.player[my_player_id].x < GAME_WIDTH - 2) dx = 1;
                else if (move_key == KEY_UP && game_state.player[my_player_id].y > 1) dy = -1;
                else if (move_key == KEY_DOWN && game_state.player[my_player_id].y < GAME_HEIGHT - 2) dy = 1;

                if (dx != 0 || dy != 0) {
                    game_state.player[my_player_id].x += dx;
                    game_state.player[my_player_id].y += dy;
                    
                    packet.type = PACKET_PLAYER_MOVE;
                    packet.player_id = my_player_id;
                    packet.data.move.x = game_state.player[my_player_id].x;
                    packet.data.move.y = game_state.player[my_player_id].y;
                    send_needed = 1;
                }
            }
            
            // 아이템 사용 처리
            if (item_key != ERR) {
                if (send_needed) {
                    send(server_sock, &packet, sizeof(Packet), 0);
                }
                packet.type = PACKET_ITEM_USE;
                packet.player_id = my_player_id;
                packet.data.item_use.item_type = item_key - '0';
                send_needed = 1;
            }

            // 패킷 전송
            if (send_needed) {
                send(server_sock, &packet, sizeof(Packet), 0);
            }

            // 렌더링
            view_draw_game_state(&game_state, my_player_id, frame);

            pthread_mutex_unlock(&state_mutex);

            view_refresh();
            frame++;
            usleep(50000);
        }

        // --- 게임 종료 화면 ---
        view_draw_game_over_screen(winner_id, my_player_id, game_state.player[my_player_id].score);
        view_draw_message_centered("Restarting in 5s... (Q to quit)", 5);
        view_refresh();

        // 5초 대기 또는 Q 종료
        nodelay(stdscr, TRUE);
        time_t start_wait = time(NULL);
        int quit_app = 0;
        
        while (time(NULL) - start_wait < 5) {
            int c = getch();
            if (c == 'q' || c == 'Q') {
                quit_app = 1;
                break;
            }
            usleep(100000);
        }

        close(server_sock);
        pthread_join(recv_thread_id, NULL);

        if (quit_app) break;
    }

    view_teardown();
    return 0;
}