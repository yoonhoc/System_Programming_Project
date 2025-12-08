#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <curses.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include "network_common.h"  // game_structs.h → network_common.h
#include "item.h"
#define MAX_ARROWS 50
#define MAX_REDZONES 10

volatile sig_atomic_t special_wave_triggered = 0;
volatile sig_atomic_t redzone_triggered = 0;

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

void create_arrow(Arrow* arrow, int start_x, int start_y, int target_x, int target_y, int is_special) {
    arrow->x = start_x;
    arrow->y = start_y;
    arrow->is_special = is_special;
    
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

int main(void) {
    Arrow arrows[MAX_ARROWS] = {0};
    RedZone redzones[MAX_REDZONES] = {0};
    PlayerStatus player_status = {
        .lives = 3,
        .invincible_item = 1,
        .heal_item = 1,
        .slow_item = 1,
        .is_invincible = 0,
        .invincible_frames = 0,
        .is_slow = 0,
        .slow_frames = 0,
        .damage_cooldown = 0
    };
    
    int i, j;

    signal(SIGALRM, alarm_handler);
    alarm(10);

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
        init_pair(3, COLOR_YELLOW, COLOR_BLACK);  // 무적 상태
        init_pair(4, COLOR_GREEN, COLOR_BLACK);   // 아이템
        init_pair(5, COLOR_CYAN, COLOR_BLACK);    // 슬로우 상태
    }
    intrflush(stdscr, FALSE);
    
    srand((unsigned int)time(NULL));

    set_nonblocking_input();

    int height, width;
    getmaxyx(stdscr, height, width);

    int player_y = height / 2;
    int player_x = width / 2;
    int ch;
    int game_over = 0;
    int frame = 0;
    int score = 0;
    int special_wave_frame = 0;
    int blink_counter = 0;

    while (!game_over && player_status.lives > 0) {
        // 입력 처리
        int last_ch = ERR;
        while ((ch = getch()) != ERR) {
            last_ch = ch;
        }
        
        if (last_ch != ERR) {
            switch(last_ch) {
                case KEY_LEFT:
                    if (player_x > 1) player_x--;
                    break;
                case KEY_RIGHT:
                    if (player_x < width - 2) player_x++;
                    break;
                case KEY_UP:
                    if (player_y > 1) player_y--;
                    break;
                case KEY_DOWN:
                    if (player_y < height - 2) player_y++;
                    break;
                case '1':
                    use_invincible_item(&player_status);
                    break;
                case '2':
                    use_heal_item(&player_status);
                    break;
                case '3':
                    use_slow_item(&player_status);
                    break;
                case 'q':
                case 'Q':
                    game_over = 1;
                    break;
            }
        }
        
        flushinp();

        // 상태 업데이트
        if (player_status.invincible_frames > 0) {
            player_status.invincible_frames--;
            if (player_status.invincible_frames == 0) {
                player_status.is_invincible = 0;
            }
        }
        
        if (player_status.slow_frames > 0) {
            player_status.slow_frames--;
            if (player_status.slow_frames == 0) {
                player_status.is_slow = 0;
            }
        }
        
        if (player_status.damage_cooldown > 0) {
            player_status.damage_cooldown--;
        }

        // 특수 웨이브 트리거 처리
        if (special_wave_triggered) {
            special_wave_triggered = 0;
            special_wave_frame = 60;
            blink_counter = 0;
            alarm(10);
        }

        // 레드존 생성
        if (redzone_triggered) {
            redzone_triggered = 0;
            
            for (i = 0; i < MAX_REDZONES; i++) {
                if (!redzones[i].active) {
                    redzones[i].width = 5 + rand() % 8;
                    redzones[i].height = 3 + rand() % 5;
                    redzones[i].x = 2 + rand() % (width - redzones[i].width - 3);
                    redzones[i].y = 2 + rand() % (height - redzones[i].height - 3);
                    redzones[i].lifetime = 200;
                    redzones[i].active = 1;
                    break;
                }
            }
        }

        // 일반 화살 생성
        int level = score / 100;
        if (rand() % 100 < 10 + level * 2) {
            for (i = 0; i < MAX_ARROWS; i++) {
                if (!arrows[i].active) {
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
                    
                    create_arrow(&arrows[i], start_x, start_y, player_x, player_y, 0);
                    break;
                }
            }
        }

        // 특수 화살 생성
        if (special_wave_frame > 0) {
            blink_counter++;
            
            if (rand() % 100 < 30 + level * 4) {
                for (i = 0; i < MAX_ARROWS; i++) {
                    if (!arrows[i].active) {
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
                        
                        create_arrow(&arrows[i], start_x, start_y, player_x, player_y, 1);
                        break;
                    }
                }
            }
            
            special_wave_frame--;
        }

        // 레드존 업데이트 및 충돌 체크
        for (i = 0; i < MAX_REDZONES; i++) {
            if (!redzones[i].active) continue;
            
            redzones[i].lifetime--;
            if (redzones[i].lifetime <= 0) {
                redzones[i].active = 0;
                continue;
            }
            
            if (player_x >= redzones[i].x && 
                player_x < redzones[i].x + redzones[i].width &&
                player_y >= redzones[i].y && 
                player_y < redzones[i].y + redzones[i].height) {
                take_damage(&player_status);
                if (player_status.lives == 0) {
                    // 게임 오버 알림
                    beep();
                    flash();
                }
            }
        }

        // 화살 이동 & 충돌 체크 (슬로우 효과 적용)
        int move_frame = player_status.is_slow ? 2 : 1;  // 슬로우 시 2프레임마다 이동
        
        for (i = 0; i < MAX_ARROWS; i++) {
            if (!arrows[i].active) continue;

            // 슬로우 효과가 있으면 홀수 프레임에만 이동
            if (!player_status.is_slow || frame % 2 == 0) {
                arrows[i].x += arrows[i].dx;
                arrows[i].y += arrows[i].dy;
            }

            if (arrows[i].x <= 0 || arrows[i].x >= width - 1 ||
                arrows[i].y <= 0 || arrows[i].y >= height - 1) {
                arrows[i].active = 0;
                continue;
            }

            if (arrows[i].x == player_x && arrows[i].y == player_y) {
                take_damage(&player_status);
                if (player_status.lives == 0) {
                    // 게임 오버 알림
                    beep();
                    flash();
                }
            }
        }

        // 화면 그리기
        erase();
        
        if (special_wave_frame > 0 && (frame % 10 < 5)) {
            attron(A_REVERSE);
        }
        
        box(stdscr, 0, 0);

        // 레드존 그리기
        for (i = 0; i < MAX_REDZONES; i++) {
            if (!redzones[i].active) continue;
            
            if (has_colors()) {
                attron(COLOR_PAIR(2));
            }
            
            for (int ry = redzones[i].y; ry < redzones[i].y + redzones[i].height; ry++) {
                for (int rx = redzones[i].x; rx < redzones[i].x + redzones[i].width; rx++) {
                    if (ry > 0 && ry < height - 1 && rx > 0 && rx < width - 1) {
                        mvaddch(ry, rx, '#');
                    }
                }
            }
            
            if (has_colors()) {
                attroff(COLOR_PAIR(2));
            }
        }

        // 플레이어 (무적 상태면 깜박임)
        if (player_status.is_invincible || player_status.damage_cooldown > 0) {
            if (frame % 4 < 2) {  // 깜박임 효과
                if (has_colors()) {
                    attron(COLOR_PAIR(3) | A_BOLD);
                }
                mvaddch(player_y, player_x, '@');
                if (has_colors()) {
                    attroff(COLOR_PAIR(3) | A_BOLD);
                }
            }
        } else {
            mvaddch(player_y, player_x, '@');
        }

        // 화살
        for (i = 0; i < MAX_ARROWS; i++) {
            if (arrows[i].active) {
                if (arrows[i].is_special && has_colors()) {
                    attron(COLOR_PAIR(1) | A_BOLD);
                    mvaddch(arrows[i].y, arrows[i].x, arrows[i].symbol);
                    attroff(COLOR_PAIR(1) | A_BOLD);
                } else {
                    // 슬로우 상태일 때 화살 색상 변경
                    if (player_status.is_slow && has_colors()) {
                        attron(COLOR_PAIR(5));
                    }
                    mvaddch(arrows[i].y, arrows[i].x, arrows[i].symbol);
                    if (player_status.is_slow && has_colors()) {
                        attroff(COLOR_PAIR(5));
                    }
                }
            }
        }

        // 상태 표시
        mvprintw(0, 2, " Score: %d  Level: %d ", score, level);
        
        // 생명 표시
        mvprintw(0, 30, " Lives: ");
        for (i = 0; i < player_status.lives; i++) {
            addch('<');
            addch('3');
        }
        addstr(" ");
        
        if (special_wave_frame > 0) {
            mvprintw(0, 45, " SPECIAL WAVE! ");
        }
        
        // 아이템 표시
        if (has_colors()) {
            attron(COLOR_PAIR(4));
        }
        mvprintw(height - 1, 2, " [1]Invincible:%d [2]Heal:%d [3]Slow:%d ", 
                 player_status.invincible_item, 
                 player_status.heal_item, 
                 player_status.slow_item);
        if (has_colors()) {
            attroff(COLOR_PAIR(4));
        }
        
        // 활성 효과 표시
        if (player_status.is_invincible) {
            mvprintw(height - 1, 50, " INVINCIBLE:%.1fs ", player_status.invincible_frames / 20.0);
        }
        if (player_status.is_slow) {
            mvprintw(height - 1, 70, " SLOW:%.1fs ", player_status.slow_frames / 20.0);
        }
        
        mvprintw(0, width - 30, " (Arrow keys, Q: Quit) ");
        
        if (special_wave_frame > 0 && (frame % 10 < 5)) {
            attroff(A_REVERSE);
        }

        refresh();

        frame++;
        score++;

        napms(50);
    }

     // 게임 오버 화면
    alarm(0);
    clear();
    
    // 큰 게임 오버 메시지
    if (has_colors()) {
        attron(COLOR_PAIR(1) | A_BOLD);
    }
    
    box(stdscr, 0, 0);
    
    mvprintw(height / 2 - 4, (width - 50) / 2, "================================================");
    mvprintw(height / 2 - 3, (width - 50) / 2, "||                                            ||");
    mvprintw(height / 2 - 2, (width - 50) / 2, "||              G A M E   O V E R             ||");
    mvprintw(height / 2 - 1, (width - 50) / 2, "||                                            ||");
    mvprintw(height / 2,     (width - 50) / 2, "================================================");
    
    if (has_colors()) {
        attroff(COLOR_PAIR(1) | A_BOLD);
    }
    
    mvprintw(height / 2 + 2, (width - 30) / 2, "Final Score: %d", score);
    mvprintw(height / 2 + 3, (width - 30) / 2, "Level Reached: %d", score / 100);
    mvprintw(height / 2 + 5, (width - 30) / 2, "Press any key to exit...");
    
    refresh();
    nodelay(stdscr, FALSE);
    getch();

    endwin();
    
    // 점수를 파일로 저장 (환경 변수로 받은 경로)
    char* score_file_path = getenv("SPACEWAR_SCORE_FILE");
    if (score_file_path) {
        FILE* score_file = fopen(score_file_path, "w");
        if (score_file) {
            fprintf(score_file, "%d", score);
            fclose(score_file);
        }
    }
    
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK);

    return 0;
}
