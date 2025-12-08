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
#include <netinet/tcp.h> // TCP_NODELAY를 위해 추가
#include <time.h>
#include "network_common.h"

// 전역 변수
int server_sock;
GameState game_state;
int my_player_id = -1;
int game_over_flag = 0;
int winner_id = -1;
pthread_mutex_t state_mutex = PTHREAD_MUTEX_INITIALIZER; // 이름 변경: render -> state (의미 명확화)
volatile int game_running = 1;

// 서버 수신 스레드
void* receive_thread(void* arg) {
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
                memcpy(game_state.arrows, packet.data.arrows.arrows, sizeof(game_state.arrows));
                break;
            case PACKET_REDZONE_UPDATE:
                memcpy(game_state.redzones, packet.data.redzones.redzones, sizeof(game_state.redzones));
                break;
            case PACKET_PLAYER_STATUS:
                if (packet.player_id >= 0 && packet.player_id < MAX_PLAYERS) {
                    memcpy(&game_state.players[packet.player_id], 
                           &packet.data.player_status.player, sizeof(Player));
                }
                break;
            case PACKET_GAME_OVER:
                game_over_flag = 1;
                winner_id = packet.player_id;
                game_running = 0;
                break;
        }
        
        pthread_mutex_unlock(&state_mutex);
    }
    return NULL;
}

void set_nonblocking_input() {
    struct termios ttystate;
    tcgetattr(STDIN_FILENO, &ttystate);
    ttystate.c_lflag &= ~(ICANON | ECHO);
    ttystate.c_cc[VMIN] = 0;
    ttystate.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
    
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
}

// 게임 리셋 함수 (프로세스 재생성 대신 사용)
void reset_game_state() {
    memset(&game_state, 0, sizeof(GameState));
    my_player_id = -1;
    game_over_flag = 0;
    winner_id = -1;
    game_running = 1;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("사용법: %s <서버IP>\n", argv[0]);
        return 1;
    }

    // ncurses 초기화
    initscr();
    raw();
    noecho();
    curs_set(0);
    start_color();
    use_default_colors();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);

    // 색상 설정
    if (has_colors()) {
        init_pair(1, COLOR_RED, -1);           
        init_pair(2, COLOR_RED, COLOR_RED);    
        init_pair(3, COLOR_YELLOW, -1);        
        init_pair(4, COLOR_GREEN, -1);         
        init_pair(5, COLOR_CYAN, -1);          
        init_pair(6, COLOR_BLUE, -1);          
        init_pair(7, COLOR_MAGENTA, -1);       
    }

    set_nonblocking_input();
    
    // 메인 게임 재시작 루프
    while (1) {
        reset_game_state();

        // 소켓 생성 및 연결
        server_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (server_sock == -1) {
            endwin();
            perror("소켓 생성 실패");
            return 1;
        }

        // TCP_NODELAY 설정 (반응속도 향상)
        int flag = 1;
        setsockopt(server_sock, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));

        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = inet_addr(argv[1]);
        server_addr.sin_port = htons(PORT);

        if (connect(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
            endwin();
            perror("서버 연결 실패");
            return 1;
        }

        // 수신 스레드 시작
        pthread_t recv_thread_id;
        pthread_create(&recv_thread_id, NULL, receive_thread, NULL);

        int height = 26;
        int width = 90;
        int frame = 0;

        // 플레이어 ID 할당 대기
        clear();
        mvprintw(height / 2, (width - 30) / 2, "Connecting to server...");
        refresh();

        while (my_player_id == -1 && game_running) usleep(50000);

        // 상대방 연결 대기
        int opponent_id = (my_player_id == 0) ? 1 : 0;
        while (game_running) {
            pthread_mutex_lock(&state_mutex);
            int connected = game_state.players[opponent_id].connected;
            pthread_mutex_unlock(&state_mutex);
            
            if (connected) break;

            erase();
            box(stdscr, 0, 0);
            mvprintw(height / 2, (width - 30) / 2, "Connected as Player %d!", my_player_id + 1);
            mvprintw(height / 2 + 2, (width - 30) / 2, "Waiting for other player...");
            refresh();
            usleep(100000);
        }

        // 게임 시작 카운트다운
        clear();
        mvprintw(height / 2, (width - 20) / 2, "Game Starting...");
        refresh();
        //7초 대기 표시
        for (int i = 7; i > 0; i--) {
            mvprintw(height / 2 + 2, (width - 10) / 2, " %d ", i);
            refresh();
            sleep(1);
        }

        // --- 메인 게임 루프 ---
        while (game_running && !game_over_flag) {
            int ch;
            Packet packet;
            int send_needed = 0;
            int move_key = ERR; // 이동 키는 마지막 입력만 처리
            int item_key = ERR; // 아이템 키

            // 입력 버퍼 모두 비우기 (반응성 향상)
            while ((ch = getch()) != ERR) {
                if (ch == 'q' || ch == 'Q') {
                    game_running = 0;
                    break;
                }
                if (ch == KEY_LEFT || ch == KEY_RIGHT || ch == KEY_UP || ch == KEY_DOWN) {
                    move_key = ch;
                } else if (ch >= '1' && ch <= '3') {
                    item_key = ch; // 아이템 키 저장
                }
            }

            pthread_mutex_lock(&state_mutex);

            // 이동 처리
            if (move_key != ERR) {
                int dx = 0, dy = 0;
                if (move_key == KEY_LEFT && game_state.players[my_player_id].x > 1) dx = -1;
                else if (move_key == KEY_RIGHT && game_state.players[my_player_id].x < width - 2) dx = 1;
                else if (move_key == KEY_UP && game_state.players[my_player_id].y > 1) dy = -1;
                else if (move_key == KEY_DOWN && game_state.players[my_player_id].y < height - 2) dy = 1;

                if (dx != 0 || dy != 0) {
                    game_state.players[my_player_id].x += dx;
                    game_state.players[my_player_id].y += dy;
                    
                    packet.type = PACKET_PLAYER_MOVE;
                    packet.player_id = my_player_id;
                    packet.data.move.x = game_state.players[my_player_id].x;
                    packet.data.move.y = game_state.players[my_player_id].y;
                    send_needed = 1;
                }
            }
            
            // 아이템 사용 처리 (이동과 별도로 전송 가능하도록)
            if (item_key != ERR) {
                // 이미 이동 패킷을 보냈다면 먼저 전송
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

            // --- 렌더링 로직 (Double buffering 효과) ---
            erase(); // 버퍼 지우기
            box(stdscr, 0, 0);

            // 레드존
            if (has_colors()) attron(COLOR_PAIR(2));
            for (int i = 0; i < MAX_REDZONES; i++) {
                if (game_state.redzones[i].active) {
                    for (int ry = 0; ry < game_state.redzones[i].height; ry++) {
                        for (int rx = 0; rx < game_state.redzones[i].width; rx++) {
                            int py = game_state.redzones[i].y + ry;
                            int px = game_state.redzones[i].x + rx;
                            if (py > 0 && py < height - 1 && px > 0 && px < width - 1)
                                mvaddch(py, px, '#');
                        }
                    }
                }
            }
            if (has_colors()) attroff(COLOR_PAIR(2));

            // 플레이어
            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (!game_state.players[i].connected) continue;
                char p_char = (i == 0) ? '@' : '$';
                int color = (i == my_player_id) ? 3 : 6;
                int attrs = 0;

                if (i == my_player_id && (game_state.players[i].status.is_invincible || 
                    game_state.players[i].status.damage_cooldown > 0)) {
                    if (frame % 4 < 2) attrs = A_BOLD; // 깜박임
                }

                if (has_colors()) attron(COLOR_PAIR(color) | attrs);
                mvaddch(game_state.players[i].y, game_state.players[i].x, p_char);
                if (has_colors()) attroff(COLOR_PAIR(color) | attrs);
            }

            // 화살
            int is_any_slow = game_state.players[0].status.is_slow || game_state.players[1].status.is_slow;
            for (int i = 0; i < MAX_ARROWS; i++) {
                if (!game_state.arrows[i].active) continue;
                
                int color = 0;
                int attr = 0;

                if (game_state.arrows[i].is_special == 2) { color = 7; attr = A_BOLD; } // 유저 공격
                else if (game_state.arrows[i].is_special == 1) { color = 1; attr = A_BOLD; } // 특수
                else if (is_any_slow) { color = 5; } // 슬로우

                if (has_colors() && color) attron(COLOR_PAIR(color) | attr);
                mvaddch(game_state.arrows[i].y, game_state.arrows[i].x, game_state.arrows[i].symbol);
                if (has_colors() && color) attroff(COLOR_PAIR(color) | attr);
            }

            // UI
            mvprintw(0, 2, " P%d Score:%d ", my_player_id + 1, game_state.players[my_player_id].score);
            mvprintw(0, 45, " Lives:");
            for(int k=0; k<game_state.players[my_player_id].status.lives; k++) addstr("<3");
            
            if (game_state.players[opponent_id].connected) {
                mvprintw(0, 65, " Enemy:");
                for(int k=0; k<game_state.players[opponent_id].status.lives; k++) addstr("<3");
            }

            if (game_state.special_wave_frame > 0) mvprintw(1, 2, " SPECIAL WAVE! ");

            if (has_colors()) attron(COLOR_PAIR(4));
            mvprintw(height-1, 2, "[1]Inv:%d [2]Heal:%d [3]Slow:%d", 
                game_state.players[my_player_id].status.invincible_item,
                game_state.players[my_player_id].status.heal_item,
                game_state.players[my_player_id].status.slow_item);
            if (has_colors()) attroff(COLOR_PAIR(4));

            pthread_mutex_unlock(&state_mutex); // 렌더 데이터 접근 끝

            refresh();
            frame++;
            usleep(50000); // 20 FPS 유지
        }

        // --- 게임 종료 화면 ---
        clear();
        box(stdscr, 0, 0);
        attron(A_BOLD);
        if (winner_id == my_player_id) mvprintw(height/2, (width-10)/2, "YOU WIN!");
        else if (winner_id == -1) mvprintw(height/2, (width-10)/2, "DRAW!");
        else mvprintw(height/2, (width-10)/2, "YOU LOSE!");
        attroff(A_BOLD);
        
        mvprintw(height/2 + 2, (width-30)/2, "Final Score: %d", game_state.players[my_player_id].score);
        mvprintw(height/2 + 5, (width-30)/2, "Restarting in 5s... (Q to quit)");
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
        pthread_join(recv_thread_id, NULL); // 스레드 정리

        if (quit_app) break;
    }

    endwin();
    return 0;
}