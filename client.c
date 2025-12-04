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
#include "common.h"
#include "game_structs.h"

int server_sock;
GameState game_state;
int my_player_id = -1;
int game_over_flag = 0;
int winner_id = -1;
pthread_mutex_t render_mutex = PTHREAD_MUTEX_INITIALIZER;
volatile int game_running = 1;

// 서버로부터 데이터 수신 스레드
void* receive_thread(void* arg) {
    Packet packet;
    
    while (game_running) {
        int recv_size = recv(server_sock, &packet, sizeof(Packet), 0);
        
        if (recv_size <= 0) {
            game_running = 0;
            break;
        }
        
        pthread_mutex_lock(&render_mutex);
        
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
        
        pthread_mutex_unlock(&render_mutex);
    }
    
    return NULL;
}

// 논블로킹 입력 설정
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

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("사용법: %s <서버IP>\n", argv[0]);
        return 1;
    }
    
    struct sockaddr_in server_addr;
    
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        perror("소켓 생성 실패");
        return 1;
    }
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);
    server_addr.sin_port = htons(PORT);
    
    if (connect(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("서버 연결 실패");
        return 1;
    }
    
    printf("서버에 연결됨!\n");
    sleep(1);
    
    // ncurses 초기화
    initscr();
    raw();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    timeout(0);
    
    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_RED, COLOR_BLACK);
        init_pair(2, COLOR_RED, COLOR_RED);
        init_pair(3, COLOR_YELLOW, COLOR_BLACK);
        init_pair(4, COLOR_GREEN, COLOR_BLACK);
        init_pair(5, COLOR_CYAN, COLOR_BLACK);
        init_pair(6, COLOR_BLUE, COLOR_BLACK);
    }
    
    intrflush(stdscr, FALSE);
    set_nonblocking_input();
    
    // 수신 스레드 시작
    pthread_t recv_thread;
    pthread_create(&recv_thread, NULL, receive_thread, NULL);
    
    int height, width;
    getmaxyx(stdscr, height, width);
    
    int ch;
    int frame = 0;
    
    // 플레이어 ID 할당 대기
    while (my_player_id == -1 && game_running) {
        usleep(100000);
    }
    
    if (!game_running) {
        endwin();
        printf("서버 연결 실패\n");
        return 1;
    }
    
    // 대기 화면
    clear();
    box(stdscr, 0, 0);
    mvprintw(height / 2, (width - 30) / 2, "Connected as Player %d!", my_player_id + 1);
    mvprintw(height / 2 + 2, (width - 30) / 2, "Waiting for other player...");
    refresh();
    
    // 다른 플레이어 대기
    int opponent_id = (my_player_id == 0) ? 1 : 0;
    while (!game_state.players[opponent_id].connected && game_running) {
        usleep(100000);
    }
    
    if (!game_running) {
        endwin();
        printf("다른 플레이어가 연결되지 않았습니다\n");
        return 1;
    }
    
    clear();
    mvprintw(height / 2, (width - 20) / 2, "Game Starting...");
    refresh();
    sleep(1);
    
    // 게임 루프
    while (game_running && !game_over_flag) {
        // 입력 처리
        int last_ch = ERR;
        while ((ch = getch()) != ERR) {
            last_ch = ch;
        }
        
        Packet packet;
        int send_packet = 0;
        
        pthread_mutex_lock(&render_mutex);
        
        if (last_ch != ERR) {
            switch(last_ch) {
                case KEY_LEFT:
                    if (game_state.players[my_player_id].x > 1) {
                        game_state.players[my_player_id].x--;
                        packet.type = PACKET_PLAYER_MOVE;
                        packet.player_id = my_player_id;
                        packet.data.move.x = game_state.players[my_player_id].x;
                        packet.data.move.y = game_state.players[my_player_id].y;
                        send_packet = 1;
                    }
                    break;
                case KEY_RIGHT:
                    if (game_state.players[my_player_id].x < width - 2) {
                        game_state.players[my_player_id].x++;
                        packet.type = PACKET_PLAYER_MOVE;
                        packet.player_id = my_player_id;
                        packet.data.move.x = game_state.players[my_player_id].x;
                        packet.data.move.y = game_state.players[my_player_id].y;
                        send_packet = 1;
                    }
                    break;
                case KEY_UP:
                    if (game_state.players[my_player_id].y > 1) {
                        game_state.players[my_player_id].y--;
                        packet.type = PACKET_PLAYER_MOVE;
                        packet.player_id = my_player_id;
                        packet.data.move.x = game_state.players[my_player_id].x;
                        packet.data.move.y = game_state.players[my_player_id].y;
                        send_packet = 1;
                    }
                    break;
                case KEY_DOWN:
                    if (game_state.players[my_player_id].y < height - 2) {
                        game_state.players[my_player_id].y++;
                        packet.type = PACKET_PLAYER_MOVE;
                        packet.player_id = my_player_id;
                        packet.data.move.x = game_state.players[my_player_id].x;
                        packet.data.move.y = game_state.players[my_player_id].y;
                        send_packet = 1;
                    }
                    break;
                case '1':
                    packet.type = PACKET_ITEM_USE;
                    packet.player_id = my_player_id;
                    packet.data.item_use.item_type = 1;
                    send_packet = 1;
                    break;
                case '2':
                    packet.type = PACKET_ITEM_USE;
                    packet.player_id = my_player_id;
                    packet.data.item_use.item_type = 2;
                    send_packet = 1;
                    break;
                case '3':
                    packet.type = PACKET_ITEM_USE;
                    packet.player_id = my_player_id;
                    packet.data.item_use.item_type = 3;
                    send_packet = 1;
                    break;
                case 'q':
                case 'Q':
                    game_running = 0;
                    break;
            }
        }
        
        pthread_mutex_unlock(&render_mutex);
        
        if (send_packet) {
            send(server_sock, &packet, sizeof(Packet), 0);
        }
        
        flushinp();
        
        // 화면 그리기 (싱글플레이와 거의 동일)
        pthread_mutex_lock(&render_mutex);
        
        erase();
        
        // 특수 웨이브 중 깜박임
        if (game_state.special_wave_frame > 0 && (frame % 10 < 5)) {
            attron(A_REVERSE);
        }
        
        box(stdscr, 0, 0);
        
        // 레드존 그리기
        for (int i = 0; i < MAX_REDZONES; i++) {
            if (!game_state.redzones[i].active) continue;
            
            if (has_colors()) {
                attron(COLOR_PAIR(2));
            }
            
            for (int ry = game_state.redzones[i].y; 
                 ry < game_state.redzones[i].y + game_state.redzones[i].height; ry++) {
                for (int rx = game_state.redzones[i].x; 
                     rx < game_state.redzones[i].x + game_state.redzones[i].width; rx++) {
                    if (ry > 0 && ry < height - 1 && rx > 0 && rx < width - 1) {
                        mvaddch(ry, rx, '#');
                    }
                }
            }
            
            if (has_colors()) {
                attroff(COLOR_PAIR(2));
            }
        }
        
        // 플레이어들 그리기
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (!game_state.players[i].connected) continue;
            
            char player_char = (i == 0) ? '@' : '$';
            
            if (i == my_player_id) {
                // 내 플레이어
                if (game_state.players[i].status.is_invincible || 
                    game_state.players[i].status.damage_cooldown > 0) {
                    if (frame % 4 < 2) {
                        if (has_colors()) {
                            attron(COLOR_PAIR(3) | A_BOLD);
                        }
                        mvaddch(game_state.players[i].y, game_state.players[i].x, player_char);
                        if (has_colors()) {
                            attroff(COLOR_PAIR(3) | A_BOLD);
                        }
                    }
                } else {
                    mvaddch(game_state.players[i].y, game_state.players[i].x, player_char);
                }
            } else {
                // 다른 플레이어
                if (has_colors()) {
                    attron(COLOR_PAIR(6));
                }
                mvaddch(game_state.players[i].y, game_state.players[i].x, player_char);
                if (has_colors()) {
                    attroff(COLOR_PAIR(6));
                }
            }
        }
        
        // 화살 그리기
        int is_slow = game_state.players[0].status.is_slow || 
                      game_state.players[1].status.is_slow;
        
        for (int i = 0; i < MAX_ARROWS; i++) {
            if (game_state.arrows[i].active) {
                if (game_state.arrows[i].is_special && has_colors()) {
                    attron(COLOR_PAIR(1) | A_BOLD);
                    mvaddch(game_state.arrows[i].y, game_state.arrows[i].x, 
                           game_state.arrows[i].symbol);
                    attroff(COLOR_PAIR(1) | A_BOLD);
                } else {
                    if (is_slow && has_colors()) {
                        attron(COLOR_PAIR(5));
                    }
                    mvaddch(game_state.arrows[i].y, game_state.arrows[i].x, 
                           game_state.arrows[i].symbol);
                    if (is_slow && has_colors()) {
                        attroff(COLOR_PAIR(5));
                    }
                }
            }
        }
        
        // UI 표시
        int level = game_state.players[my_player_id].score / 100;
        mvprintw(0, 2, " You: P%d ", my_player_id + 1);
        mvprintw(0, 15, " Score: %d  Level: %d ", game_state.players[my_player_id].score, level);
        
        // 내 생명
        mvprintw(0, 45, " Lives: ");
        for (int i = 0; i < game_state.players[my_player_id].status.lives; i++) {
            addch('<');
            addch('3');
        }
        addstr(" ");
        
        // 다른 플레이어 상태
        int other_player_id = (my_player_id == 0) ? 1 : 0;
        if (game_state.players[other_player_id].connected) {
            mvprintw(0, 60, " P%d Lives: ", other_player_id + 1);
            for (int i = 0; i < game_state.players[other_player_id].status.lives; i++) {
                addch('<');
                addch('3');
            }
        }
        
        // 특수 웨이브 표시
        if (game_state.special_wave_frame > 0) {
            mvprintw(1, 2, " SPECIAL WAVE! ");
        }
        
        // 아이템 표시
        if (has_colors()) {
            attron(COLOR_PAIR(4));
        }
        mvprintw(height - 1, 2, " [1]Invincible:%d [2]Heal:%d [3]Slow:%d ", 
                 game_state.players[my_player_id].status.invincible_item,
                 game_state.players[my_player_id].status.heal_item,
                 game_state.players[my_player_id].status.slow_item);
        if (has_colors()) {
            attroff(COLOR_PAIR(4));
        }
        
        // 활성 효과
        if (game_state.players[my_player_id].status.is_invincible) {
            mvprintw(height - 1, 50, " INVINCIBLE:%.1fs ", 
                    game_state.players[my_player_id].status.invincible_frames / 20.0);
        }
        if (is_slow) {
            int max_slow = game_state.players[0].status.slow_frames > 
                          game_state.players[1].status.slow_frames ?
                          game_state.players[0].status.slow_frames :
                          game_state.players[1].status.slow_frames;
            mvprintw(height - 1, 70, " SLOW:%.1fs ", max_slow / 20.0);
        }
        
        if (game_state.special_wave_frame > 0 && (frame % 10 < 5)) {
            attroff(A_REVERSE);
        }
        
        pthread_mutex_unlock(&render_mutex);
        
        refresh();
        frame++;
        
        usleep(50000);
    }
    
    // 게임 오버 화면
    clear();
    
    if (has_colors()) {
        attron(COLOR_PAIR(1) | A_BOLD);
    }
    
    box(stdscr, 0, 0);
    
    mvprintw(height / 2 - 4, (width - 50) / 2, "================================================");
    mvprintw(height / 2 - 3, (width - 50) / 2, "||                                            ||");
    
    if (winner_id == -1) {
        mvprintw(height / 2 - 2, (width - 50) / 2, "||                  DRAW!                     ||");
    } else if (winner_id == my_player_id) {
        mvprintw(height / 2 - 2, (width - 50) / 2, "||                YOU WIN!                    ||");
    } else {
        mvprintw(height / 2 - 2, (width - 50) / 2, "||                YOU LOSE!                   ||");
    }
    
    mvprintw(height / 2 - 1, (width - 50) / 2, "||                                            ||");
    mvprintw(height / 2,     (width - 50) / 2, "================================================");
    
    if (has_colors()) {
        attroff(COLOR_PAIR(1) | A_BOLD);
    }
    
    mvprintw(height / 2 + 2, (width - 30) / 2, "Your Final Score: %d", 
            game_state.players[my_player_id].score);
    
    int other_id = (my_player_id == 0) ? 1 : 0;
    if (game_state.players[other_id].connected) {
        mvprintw(height / 2 + 3, (width - 30) / 2, "Player %d Score: %d", 
                other_id + 1, game_state.players[other_id].score);
    }
    
    mvprintw(height / 2 + 5, (width - 30) / 2, "Game will restart in 5s...");
    mvprintw(height / 2 + 6, (width - 30) / 2, "(Press Q to quit)");
    
    refresh();
    
    // 5초 대기 또는 Q 입력
    nodelay(stdscr, TRUE);
    time_t start_time = time(NULL);
    while (time(NULL) - start_time < 5) {
        int ch = getch();
        if (ch == 'q' || ch == 'Q') {
            // 점수 저장
            char* score_file_path = getenv("SPACEWAR_SCORE_FILE");
            if (score_file_path) {
                FILE* score_file = fopen(score_file_path, "w");
                if (score_file) {
                    fprintf(score_file, "%d", game_state.players[my_player_id].score);
                    fclose(score_file);
                }
            }
            
            endwin();
            close(server_sock);
            return 0;
        }
        usleep(100000);
    }
    
    // 점수를 파일로 저장
    char* score_file_path = getenv("SPACEWAR_SCORE_FILE");
    if (score_file_path) {
        FILE* score_file = fopen(score_file_path, "w");
        if (score_file) {
            fprintf(score_file, "%d", game_state.players[my_player_id].score);
            fclose(score_file);
        }
    }
    
    // 게임 재시작 (기존 코드는 제거)
    endwin();
    close(server_sock);
    
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK);

    return 0;
}
