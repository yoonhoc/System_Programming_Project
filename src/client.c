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
int id = -1;
int game_over = 0;
int winner = -1;
pthread_mutex_t state_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t state_cond = PTHREAD_COND_INITIALIZER;
volatile int game_running = 1;

// 서버 수신 스레드
void* receive_thread(void* arg) {
    (void)arg;
    Packet packet;
    
    while (game_running) {
        int recv_size = recv(server_sock, &packet, sizeof(Packet), 0);
        
        if (recv_size <= 0) {
            pthread_mutex_lock(&state_mutex);
            game_running = 0;
            pthread_cond_broadcast(&state_cond);  // 모든 대기 스레드 깨우기
            pthread_mutex_unlock(&state_mutex);
            break;
        }
        
        pthread_mutex_lock(&state_mutex);
        
        switch (packet.type) {
            case INITIAL_STATE:
                id = packet.id;
                memcpy(&game_state, &packet.game_state, sizeof(GameState));
                pthread_cond_signal(&state_cond);  // ID 할당 알림
                break;
            case ARROW_UPDATE:
                memcpy(game_state.arrow, packet.arrows, sizeof(game_state.arrow));
                break;
            case REDZONE_UPDATE:
                memcpy(game_state.redzone, packet.redzones, sizeof(game_state.redzone));
                break;
            case PLAYER_STATUS:
                if (packet.id >= 0 && packet.id < MAX_PLAYERS) {
                    memcpy(&game_state.player[packet.id], 
                           &packet.player, sizeof(Player));
                    pthread_cond_signal(&state_cond);  // 플레이어 상태 변경 알림
                }
                break;
            case GAME_OVER:
                game_over = 1;
                winner = packet.id;
                game_running = 0;
                pthread_cond_signal(&state_cond);
                break;
            default:
                break;
        }
        
        pthread_mutex_unlock(&state_mutex);
    }
    return NULL;
}


int main(int argc, char* argv[]) {

    if (argc != 2) {
        printf("사용법: %s <서버IP>\n", argv[0]);
        return 1;
    }
    view_init();
    
    // 메인 게임 재시작 루프
    while (1) {
        
        //초기화
        memset(&game_state, 0, sizeof(GameState));
        id = -1;
        game_over = 0;
        winner = -1;
        game_running = 1;

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
        pthread_t recv_thread;
        pthread_create(&recv_thread, NULL, receive_thread, NULL);

        int frame = 0;

        // 플레이어 ID 할당 대기
        erase();
        mvprintw(GAME_HEIGHT / 2 , (GAME_WIDTH - strlen("Connecting to server...")) / 2, "%s", "Connecting to server...");
        
        refresh();

        pthread_mutex_lock(&state_mutex);
        while (id == -1 && game_running) {//플레이어가 할당 안되면
            pthread_cond_wait(&state_cond, &state_mutex);
        }
        pthread_mutex_unlock(&state_mutex);
        
        if (!game_running) {
            close(server_sock);
            pthread_join(recv_thread, NULL);
            break;
        }

        // 내 연결 정보가 제대로 들어올 때까지 대기
        pthread_mutex_lock(&state_mutex);
        while (!game_state.player[id].connected && game_running) {
            pthread_cond_wait(&state_cond, &state_mutex);
        }
        pthread_mutex_unlock(&state_mutex);
        
        if (!game_running) {
            close(server_sock);
            pthread_join(recv_thread, NULL);
            break;
        }
        
        // 상대방 연결 대기 
        int opponent_id = (id == 0) ? 1 : 0;
        
        erase();
        char msg[50];
        sprintf(msg, "Connected as Player %d!", id + 1);
        mvprintw(GAME_HEIGHT / 2, (GAME_WIDTH - strlen(msg)) / 2, "%s",msg);
        mvprintw(GAME_HEIGHT / 2 + 2, (GAME_WIDTH - strlen("Waiting for other player...")) / 2, "%s", "Waiting for other player...");
        refresh();
        
        pthread_mutex_lock(&state_mutex);
        //상대방 연결 될때까지 대기
        while (!game_state.player[opponent_id].connected && game_running) {
            pthread_cond_wait(&state_cond, &state_mutex);
        }
        pthread_mutex_unlock(&state_mutex);
        
        if (!game_running) {
            close(server_sock);
            pthread_join(recv_thread, NULL);
            break;
        }

        // 게임 시작 전 카운트다운
        for (int i = 5; i > 0; i--) {
            erase();
            char msg[50];
            sprintf(msg, "Game starts in %d...", i);
            mvprintw(GAME_HEIGHT / 2 , (GAME_WIDTH - strlen(msg)) / 2, "%s", msg);
            refresh();
            sleep(1);
        }
        
        erase();
        mvprintw(GAME_HEIGHT / 2, (GAME_WIDTH - strlen("START!")) / 2, "%s", "START!");
        refresh();
        sleep(1);

        // --- 메인 게임 루프 ---
        while (game_running && !game_over) {
            int ch;
            int move_key = ERR;
            int item_key = ERR;

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

            if (move_key != ERR) {
                int dx = 0, dy = 0;
                if (move_key == KEY_LEFT && game_state.player[id].x > 1) dx = -1;
                else if (move_key == KEY_RIGHT && game_state.player[id].x < GAME_WIDTH - 2) dx = 1;
                else if (move_key == KEY_UP && game_state.player[id].y > 1) dy = -1;
                else if (move_key == KEY_DOWN && game_state.player[id].y < GAME_HEIGHT - 2) dy = 1;
                
                if (dx != 0 || dy != 0) {
                    game_state.player[id].x += dx;
                    game_state.player[id].y += dy;
                    
                    Packet packet;
                    packet.type = PLAYER_MOVE;
                    packet.id = id;
                    packet.x = game_state.player[id].x;
                    packet.y = game_state.player[id].y;
                    send(server_sock, &packet, sizeof(Packet), 0);
                }
            }
            
            if (item_key != ERR) {
                Packet packet;
                packet.type = ITEM_USE;
                packet.id = id;
                packet.item_type = item_key - '0';
                send(server_sock, &packet, sizeof(Packet), 0);
            }

            draw_game(&game_state, id, frame);

            pthread_mutex_unlock(&state_mutex);

            refresh();
            frame++;
            usleep(50000);
        }

        // --- 게임 종료 화면 ---
        gameOverScreen(winner, id, game_state.player[id].score);
        mvprintw(GAME_HEIGHT / 2 + 5, (GAME_WIDTH - strlen("WRestarting in 5s... (Q to quit)")) / 2, "%s", "Restarting in 5s... (Q to quit)");
        refresh();

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
        pthread_join(recv_thread, NULL);

        if (quit_app) break;
    }

    endwin();
    return 0;
}