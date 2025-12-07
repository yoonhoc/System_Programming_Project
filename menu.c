#include "common.h"
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>

// --- 상수 및 열거형 ---
enum {
    MENU_SCOREBOARD = 0,
    MENU_SINGLE,
    MENU_MULTI_HOST,
    MENU_MULTI_JOIN,
    MENU_EXIT,
    MENU_COUNT
};

// 서버 프로세스 PID 저장
pid_t server_pid = -1;

// --- 함수 원형 ---
void init_colors(void);
void draw_menu(WINDOW* win, int selected);
void handle_singleplay(void);
void handle_multiplay_host(void);
void handle_multiplay_join(void);
void draw_background(void);
void draw_star_field(int frame_count); 
void get_player_name(char* name, int max_len);
void get_server_ip(char* ip, int max_len);
void draw_box_with_shadow(int y, int x, int h, int w);
void show_message(const char* title, const char* message);
void cleanup_server(void);
void reinit_ncurses(void);

int main(void) {
    int selected = 0;
    int running = 1;
    int ch;
    int frame_count = 0; 

    setlocale(LC_ALL, "");
    srand((unsigned int)time(NULL)); 

    initscr();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    timeout(100); 

    if (has_colors()) {
        start_color();
        init_colors();
    }

    int max_y, max_x;
    int win_height = 22; 
    int win_width  = 54; 
    
    getmaxyx(stdscr, max_y, max_x);
    int start_y = (max_y - win_height) / 2;
    int start_x = (max_x - win_width)  / 2;

    WINDOW* menu_win = newwin(win_height, win_width, start_y, start_x);
    keypad(menu_win, TRUE);

    while (running) {
        getmaxyx(stdscr, max_y, max_x); 
        start_y = (max_y - win_height) / 2;
        start_x = (max_x - win_width)  / 2;
        mvwin(menu_win, start_y, start_x); 

        if (max_y < win_height + 4 || max_x < win_width + 4) { 
            erase();
            mvprintw(max_y/2, max_x/2 - 15, "Screen too small! Please resize.");
            refresh();
            ch = getch(); 
            if (ch == 'q' || ch == 'Q' || ch == 27) running = 0;
            continue;
        }

        draw_background();
        draw_star_field(frame_count); 
        draw_box_with_shadow(start_y, start_x, win_height, win_width);
        draw_menu(menu_win, selected);
        wrefresh(menu_win);

        ch = wgetch(menu_win); 

        switch (ch) {
            case KEY_UP:
            case 'k':
                selected = (selected - 1 + MENU_COUNT) % MENU_COUNT;
                break;
            case KEY_DOWN:
            case 'j':
                selected = (selected + 1) % MENU_COUNT;
                break;
            case '\n':
            case KEY_ENTER:
                switch (selected) {
                    case MENU_SCOREBOARD: 
                        handle_scoreboard();
                        reinit_ncurses();
                        break;
                    case MENU_SINGLE:     
                        handle_singleplay();
                        reinit_ncurses();
                        break;
                    case MENU_MULTI_HOST:      
                        handle_multiplay_host();
                        reinit_ncurses();
                        break;
                    case MENU_MULTI_JOIN:
                        handle_multiplay_join();
                        reinit_ncurses();
                        break;
                    case MENU_EXIT:       
                        running = 0;         
                        break;
                }
                break;
            case 'q':
            case 'Q':
            case 27:
                running = 0;
                break;
        }
        frame_count++;
        if (frame_count > 1000) frame_count = 0;
    }

    delwin(menu_win);
    endwin();
    
    // 서버가 실행 중이면 종료
    cleanup_server();
    
    printf("Game terminated.\n");
    return 0;
}

void reinit_ncurses(void) {
    // 1. 표준 입력(STDIN)의 Non-blocking 플래그 제거 (핵심 수정 사항)
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (flags != -1) {
        fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK);
    }

    // 2. ncurses 완전 재초기화 (기존 코드)
    clear();
    refresh();
    
    if (has_colors()) {
        start_color();
        init_colors();
    }
    
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    timeout(100); 
    
    // 화면 전체 갱신 (잔상 제거용)
    touchwin(stdscr);
    wrefresh(stdscr);
}

void init_colors(void) {
    init_pair(COLOR_PAIR_NORMAL,    COLOR_WHITE,   COLOR_BLACK);
    init_pair(COLOR_PAIR_SELECTED,  COLOR_YELLOW,  COLOR_BLUE); 
    init_pair(COLOR_PAIR_TITLE,     COLOR_MAGENTA, COLOR_BLACK); 
    init_pair(COLOR_PAIR_BORDER,    COLOR_CYAN,    COLOR_BLACK);
    init_pair(COLOR_PAIR_ACCENT,    COLOR_GREEN,   COLOR_BLACK);
    init_pair(COLOR_PAIR_STATUS,    COLOR_BLACK,   COLOR_MAGENTA);
    init_pair(COLOR_PAIR_DECO_BLUE, COLOR_BLUE,    COLOR_BLACK);
    init_pair(COLOR_PAIR_STAR,      COLOR_WHITE,   COLOR_BLACK);
    init_pair(COLOR_PAIR_CREDITS,   COLOR_YELLOW,  COLOR_BLACK); 
}

void draw_star_field(int frame_count) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    int win_height = 22;
    int win_width  = 54;
    int start_y    = (max_y - win_height) / 2;
    int start_x    = (max_x - win_width)  / 2;

    for (int i = 1; i < max_y - 2; i++) { 
        for (int j = 1; j < max_x - 1; j++) {
            if (i >= start_y && i < start_y + win_height &&
                j >= start_x && j < start_x + win_width) {
                continue;
            }
            if (rand() % 80 == 0) { 
                chtype star_char = (frame_count % 3 == 0) ? ACS_BULLET : '*'; 
                attron(COLOR_PAIR(COLOR_PAIR_STAR)); 
                mvaddch(i, j, star_char);
                attroff(COLOR_PAIR(COLOR_PAIR_STAR));
            }
        }
    }
}

void draw_background(void) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    bkgd(COLOR_PAIR(COLOR_PAIR_NORMAL));
    erase();

    attron(COLOR_PAIR(COLOR_PAIR_BORDER) | A_BOLD);
    box(stdscr, 0, 0);
    attroff(COLOR_PAIR(COLOR_PAIR_BORDER) | A_BOLD);

    attron(COLOR_PAIR(COLOR_PAIR_STATUS) | A_BOLD);
    mvhline(1, 1, ' ', max_x - 2); 
    attroff(COLOR_PAIR(COLOR_PAIR_STATUS) | A_BOLD);
    
    const char* logo1 = "SPACE WAR"; 
    const char* logo2 = "THE GALAXY IS WAITING. JOIN THE FIGHT, PILOT!";

    attron(COLOR_PAIR(COLOR_PAIR_TITLE) | A_BOLD | A_BLINK); 
    mvprintw(1, (max_x - (int)strlen(logo1)) / 2, "%s", logo1);
    attroff(COLOR_PAIR(COLOR_PAIR_TITLE) | A_BOLD | A_BLINK);

    attron(COLOR_PAIR(COLOR_PAIR_ACCENT) | A_BOLD);
    mvprintw(2, (max_x - (int)strlen(logo2)) / 2, "%s", logo2);
    attroff(COLOR_PAIR(COLOR_PAIR_ACCENT) | A_BOLD);

    int status_y = max_y - 3; 
    int status_w = max_x - 2;

    attron(COLOR_PAIR(COLOR_PAIR_DECO_BLUE));
    mvwhline(stdscr, status_y - 1, 1, ACS_HLINE, status_w); 
    attroff(COLOR_PAIR(COLOR_PAIR_DECO_BLUE));

    mvhline(status_y, 1, ' ', status_w); 
    attron(COLOR_PAIR(COLOR_PAIR_CREDITS) | A_BOLD);
    
    long highScore = get_high_score();
    mvprintw(status_y, 3, "HIGH SCORE: %ld", highScore);
    
    const char* credits = "0204 v1.3"; 
    mvprintw(status_y, max_x - (int)strlen(credits) - 3, "%s", credits);
    attroff(COLOR_PAIR(COLOR_PAIR_CREDITS) | A_BOLD);

    attron(COLOR_PAIR(COLOR_PAIR_STATUS)); 
    mvhline(max_y - 2, 1, ' ', status_w);
    attroff(COLOR_PAIR(COLOR_PAIR_STATUS));
    refresh();
}

void draw_box_with_shadow(int y, int x, int h, int w) {
    attron(COLOR_PAIR(COLOR_PAIR_NORMAL) | A_DIM);
    for (int i = 1; i <= h; i++) {
        mvhline(y + i, x + w, ' ', 2);
    }
    mvhline(y + h, x + 2, ' ', w);
    attroff(COLOR_PAIR(COLOR_PAIR_NORMAL) | A_DIM);
}

void draw_menu(WINDOW* win, int selected) {
    const char* items[MENU_COUNT] = { "SCOREBOARD", "1 PLAYER", "2P HOST", "2P JOIN", "EXIT" };
    const char* icons[MENU_COUNT] = { "[1]", "[2]", "[3]", "[4]", "[0]" };

    int height, width;
    getmaxyx(win, height, width);
    wbkgd(win, COLOR_PAIR(COLOR_PAIR_NORMAL));
    werase(win);

    wattron(win, COLOR_PAIR(COLOR_PAIR_BORDER) | A_BOLD);
    box(win, 0, 0);
    wattroff(win, COLOR_PAIR(COLOR_PAIR_BORDER) | A_BOLD);

    wattron(win, COLOR_PAIR(COLOR_PAIR_DECO_BLUE) | A_BOLD);
    wborder(win, ACS_VLINE, ACS_VLINE, ACS_HLINE, ACS_HLINE, 
                    ACS_ULCORNER, ACS_URCORNER, ACS_LLCORNER, ACS_LRCORNER);
    wattroff(win, COLOR_PAIR(COLOR_PAIR_DECO_BLUE) | A_BOLD);

    const char* title_lines[] = {
        "#  # #### #  # #  #", 
        "#### #    ## # #  #",
        "#### #### # ## #  #",
        "#  # #    #  # #  #",
        "#  # #### #  # ####"
    };
    int title_height = sizeof(title_lines) / sizeof(title_lines[0]);
    int title_start_y = 2; 

    for (int i = 0; i < title_height; i++) {
        int line_width = (int)strlen(title_lines[i]);
        wattron(win, COLOR_PAIR(COLOR_PAIR_TITLE) | A_BOLD | A_BLINK);
        mvwprintw(win, title_start_y + i, (width - line_width) / 2, "%s", title_lines[i]);
        wattroff(win, COLOR_PAIR(COLOR_PAIR_TITLE) | A_BOLD | A_BLINK);
    }

    int divider_y = title_start_y + title_height + 2; 
    wattron(win, COLOR_PAIR(COLOR_PAIR_DECO_BLUE));
    mvwhline(win, divider_y, 1, ACS_HLINE, width - 2);
    mvwaddch(win, divider_y, 0, ACS_LTEE);
    mvwaddch(win, divider_y, width - 1, ACS_RTEE);
    wattroff(win, COLOR_PAIR(COLOR_PAIR_DECO_BLUE));

    int start_y = divider_y + 2; 
    int x_menu  = 6;

    for (int i = 0; i < MENU_COUNT; i++) {
        int y = start_y + i * 2;
        if (i == selected) {
            wattron(win, COLOR_PAIR(COLOR_PAIR_SELECTED) | A_BOLD | A_BLINK);
            mvwhline(win, y, 2, ' ', width - 4);
            mvwprintw(win, y, x_menu - 2, ">> %s %s <<", icons[i], items[i]);
            wattroff(win, COLOR_PAIR(COLOR_PAIR_SELECTED) | A_BOLD | A_BLINK);
        } else {
            wattron(win, COLOR_PAIR(COLOR_PAIR_NORMAL));
            mvwprintw(win, y, x_menu, "%s %s", icons[i], items[i]);
            wattroff(win, COLOR_PAIR(COLOR_PAIR_NORMAL));
        }
    }

    wattron(win, COLOR_PAIR(COLOR_PAIR_DECO_BLUE));
    mvwhline(win, height - 3, 1, ACS_HLINE, width - 2);
    mvwaddch(win, height - 3, 0, ACS_LTEE);
    mvwaddch(win, height - 3, width - 1, ACS_RTEE);
    wattroff(win, COLOR_PAIR(COLOR_PAIR_DECO_BLUE));

    wattron(win, COLOR_PAIR(COLOR_PAIR_ACCENT) | A_DIM);
    mvwprintw(win, height - 2, 3, "      -> : MOVE   Enter: SELECT   Q: BACK");
    wattroff(win, COLOR_PAIR(COLOR_PAIR_ACCENT) | A_DIM);
}

void show_message(const char* title, const char* message) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    int win_height = 22;
    int win_width  = 54;
    int start_y    = (max_y - win_height) / 2;
    int start_x    = (max_x - win_width)  / 2;

    draw_box_with_shadow(start_y, start_x, win_height, win_width);
    WINDOW* msg_win = newwin(win_height, win_width, start_y, start_x);
    wbkgd(msg_win, COLOR_PAIR(COLOR_PAIR_NORMAL));

    wattron(msg_win, COLOR_PAIR(COLOR_PAIR_BORDER) | A_BOLD);
    box(msg_win, 0, 0);
    wattroff(msg_win, COLOR_PAIR(COLOR_PAIR_BORDER) | A_BOLD);

    wattron(msg_win, COLOR_PAIR(COLOR_PAIR_TITLE) | A_BOLD);
    mvwprintw(msg_win, 1, (win_width - (int)strlen(title)) / 2, "%s", title);
    wattroff(msg_win, COLOR_PAIR(COLOR_PAIR_TITLE) | A_BOLD);
    
    wattron(msg_win, COLOR_PAIR(COLOR_PAIR_DECO_BLUE));
    mvwhline(msg_win, 2, 1, ACS_HLINE, win_width - 2);
    wattroff(msg_win, COLOR_PAIR(COLOR_PAIR_DECO_BLUE));

    wattron(msg_win, COLOR_PAIR(COLOR_PAIR_ACCENT) | A_BOLD);
    mvwprintw(msg_win, 4, (win_width - (int)strlen(message)) / 2, "%s", message);
    wattroff(msg_win, COLOR_PAIR(COLOR_PAIR_ACCENT) | A_BOLD);

    const char* hint = "PRESS ANY KEY TO CONTINUE";
    wattron(msg_win, COLOR_PAIR(COLOR_PAIR_STATUS) | A_BOLD | A_BLINK); 
    mvwprintw(msg_win, win_height - 2, (win_width - (int)strlen(hint)) / 2, " %s ", hint);
    wattroff(msg_win, COLOR_PAIR(COLOR_PAIR_STATUS) | A_BOLD | A_BLINK);

    wrefresh(msg_win);
    nodelay(msg_win, FALSE);
    wgetch(msg_win);
    delwin(msg_win);
}

void get_player_name(char* name, int max_len) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    int win_height = 12;
    int win_width  = 50;
    int start_y    = (max_y - win_height) / 2;
    int start_x    = (max_x - win_width)  / 2;

    draw_box_with_shadow(start_y, start_x, win_height, win_width);
    WINDOW* input_win = newwin(win_height, win_width, start_y, start_x);
    wbkgd(input_win, COLOR_PAIR(COLOR_PAIR_NORMAL));

    wattron(input_win, COLOR_PAIR(COLOR_PAIR_BORDER) | A_BOLD);
    box(input_win, 0, 0);
    wattroff(input_win, COLOR_PAIR(COLOR_PAIR_BORDER) | A_BOLD);

    wattron(input_win, COLOR_PAIR(COLOR_PAIR_TITLE) | A_BOLD);
    mvwprintw(input_win, 2, (win_width - 17) / 2, "ENTER YOUR NAME");
    wattroff(input_win, COLOR_PAIR(COLOR_PAIR_TITLE) | A_BOLD);
    
    wattron(input_win, COLOR_PAIR(COLOR_PAIR_DECO_BLUE));
    mvwhline(input_win, 3, 1, ACS_HLINE, win_width - 2);
    wattroff(input_win, COLOR_PAIR(COLOR_PAIR_DECO_BLUE));

    wattron(input_win, COLOR_PAIR(COLOR_PAIR_ACCENT));
    mvwprintw(input_win, 5, 5, "Name: ");
    wattroff(input_win, COLOR_PAIR(COLOR_PAIR_ACCENT));

    wattron(input_win, COLOR_PAIR(COLOR_PAIR_STATUS) | A_DIM);
    mvwprintw(input_win, 8, (win_width - 30) / 2, "(Max %d chars, Enter to confirm)", max_len - 1);
    wattroff(input_win, COLOR_PAIR(COLOR_PAIR_STATUS) | A_DIM);

    wrefresh(input_win);
    echo();
    curs_set(1);
    nodelay(input_win, FALSE);
    
    mvwgetnstr(input_win, 5, 11, name, max_len - 1);
    
    noecho();
    curs_set(0);
    
    if (strlen(name) == 0) {
        strcpy(name, "PLAYER");
    }
    delwin(input_win);
}

void get_server_ip(char* ip, int max_len) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    int win_height = 14;
    int win_width  = 50;
    int start_y    = (max_y - win_height) / 2;
    int start_x    = (max_x - win_width)  / 2;

    draw_box_with_shadow(start_y, start_x, win_height, win_width);
    WINDOW* input_win = newwin(win_height, win_width, start_y, start_x);
    wbkgd(input_win, COLOR_PAIR(COLOR_PAIR_NORMAL));

    wattron(input_win, COLOR_PAIR(COLOR_PAIR_BORDER) | A_BOLD);
    box(input_win, 0, 0);
    wattroff(input_win, COLOR_PAIR(COLOR_PAIR_BORDER) | A_BOLD);

    wattron(input_win, COLOR_PAIR(COLOR_PAIR_TITLE) | A_BOLD);
    mvwprintw(input_win, 2, (win_width - 19) / 2, "MULTIPLAYER SETUP");
    wattroff(input_win, COLOR_PAIR(COLOR_PAIR_TITLE) | A_BOLD);
    
    wattron(input_win, COLOR_PAIR(COLOR_PAIR_DECO_BLUE));
    mvwhline(input_win, 3, 1, ACS_HLINE, win_width - 2);
    wattroff(input_win, COLOR_PAIR(COLOR_PAIR_DECO_BLUE));

    wattron(input_win, COLOR_PAIR(COLOR_PAIR_ACCENT));
    mvwprintw(input_win, 5, 5, "Server IP: ");
    wattroff(input_win, COLOR_PAIR(COLOR_PAIR_ACCENT));

    wattron(input_win, COLOR_PAIR(COLOR_PAIR_STATUS) | A_DIM);
    mvwprintw(input_win, 8, 5, "Default: 127.0.0.1 (localhost)");
    mvwprintw(input_win, 9, 5, "Press Enter for default");
    wattroff(input_win, COLOR_PAIR(COLOR_PAIR_STATUS) | A_DIM);

    wrefresh(input_win);
    echo();
    curs_set(1);
    nodelay(input_win, FALSE);
    
    mvwgetnstr(input_win, 5, 16, ip, max_len - 1);
    
    noecho();
    curs_set(0);
    
    if (strlen(ip) == 0) {
        strcpy(ip, "127.0.0.1");
    }
    delwin(input_win);
}

void handle_singleplay(void) {
    char player_name[32];
    get_player_name(player_name, sizeof(player_name));
    
    endwin();
    
    // 점수를 파일로 전달하기 위한 임시 파일
    char temp_score_file[] = "/tmp/spacewar_score_XXXXXX";
    int score_fd = mkstemp(temp_score_file);
    if (score_fd == -1) {
        perror("Failed to create temp file");
        return;
    }
    close(score_fd);
    
    pid_t pid = fork();
    
    if (pid == 0) {
        // 환경 변수로 점수 파일 경로 전달
        setenv("SPACEWAR_SCORE_FILE", temp_score_file, 1);
        execl("./single_play", "single_play", NULL);
        perror("Failed to execute single_play");
        exit(1);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        
        // 점수 파일에서 점수 읽기
        int final_score = 0;
        FILE* score_file = fopen(temp_score_file, "r");
        if (score_file) {
            fscanf(score_file, "%d", &final_score);
            fclose(score_file);
        }
        unlink(temp_score_file);
        
        // ncurses 재초기화
        initscr();
        cbreak();
        noecho();
        curs_set(0);
        keypad(stdscr, TRUE);
        timeout(100);
        
        if (has_colors()) {
            start_color();
            init_colors();
        }
        
        clear();
        refresh();
        
        // 점수가 0보다 크면 저장
        if (final_score > 0) {
            save_score(player_name, final_score, "SINGLE");
            
            int max_y, max_x;
            getmaxyx(stdscr, max_y, max_x);
            int win_height = 14;
            int win_width  = 50;
            int start_y    = (max_y - win_height) / 2;
            int start_x    = (max_x - win_width)  / 2;

            draw_box_with_shadow(start_y, start_x, win_height, win_width);
            WINDOW* score_win = newwin(win_height, win_width, start_y, start_x);
            wbkgd(score_win, COLOR_PAIR(COLOR_PAIR_NORMAL));

            wattron(score_win, COLOR_PAIR(COLOR_PAIR_BORDER) | A_BOLD);
            box(score_win, 0, 0);
            wattroff(score_win, COLOR_PAIR(COLOR_PAIR_BORDER) | A_BOLD);

            wattron(score_win, COLOR_PAIR(COLOR_PAIR_TITLE) | A_BOLD);
            mvwprintw(score_win, 2, (win_width - 13) / 2, "GAME RESULT");
            wattroff(score_win, COLOR_PAIR(COLOR_PAIR_TITLE) | A_BOLD);
            
            wattron(score_win, COLOR_PAIR(COLOR_PAIR_DECO_BLUE));
            mvwhline(score_win, 3, 1, ACS_HLINE, win_width - 2);
            wattroff(score_win, COLOR_PAIR(COLOR_PAIR_DECO_BLUE));

            wattron(score_win, COLOR_PAIR(COLOR_PAIR_ACCENT) | A_BOLD);
            mvwprintw(score_win, 5, (win_width - 30) / 2, "Final Score: %d", final_score);
            mvwprintw(score_win, 6, (win_width - 30) / 2, "Score saved!");
            wattroff(score_win, COLOR_PAIR(COLOR_PAIR_ACCENT) | A_BOLD);
            
            wattron(score_win, COLOR_PAIR(COLOR_PAIR_STATUS) | A_BLINK);
            mvwprintw(score_win, 10, (win_width - 25) / 2, "Press any key to continue");
            wattroff(score_win, COLOR_PAIR(COLOR_PAIR_STATUS) | A_BLINK);

            wrefresh(score_win);
            nodelay(score_win, FALSE);
            wgetch(score_win);
            delwin(score_win);
        }
    }
}

void cleanup_server(void) {
    if (server_pid > 0) {
        kill(server_pid, SIGTERM);
        waitpid(server_pid, NULL, 0);
        server_pid = -1;
    }
}

void handle_multiplay_host(void) {
    char player_name[32];
    get_player_name(player_name, sizeof(player_name));
    
    // 서버를 백그라운드로 시작
    server_pid = fork();
    
    if (server_pid == 0) {
        // 자식 프로세스: 서버 실행
        // 표준 출력/에러를 /dev/null로 리다이렉트 (출력 숨김)
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        
        execl("./server", "server", NULL);
        exit(1);
    } else if (server_pid > 0) {
        // 부모 프로세스: 서버 시작 대기 후 클라이언트 실행
        sleep(1);  // 서버 초기화 대기 (2초 → 1초로 단축)
        
        // ncurses 그대로 유지하고 "Connecting..." 메시지 표시
        int max_y, max_x;
        getmaxyx(stdscr, max_y, max_x);
        int win_height = 10;
        int win_width  = 50;
        int start_y    = (max_y - win_height) / 2;
        int start_x    = (max_x - win_width)  / 2;

        WINDOW* wait_win = newwin(win_height, win_width, start_y, start_x);
        wbkgd(wait_win, COLOR_PAIR(COLOR_PAIR_NORMAL));
        box(wait_win, 0, 0);
        
        wattron(wait_win, COLOR_PAIR(COLOR_PAIR_TITLE) | A_BOLD);
        mvwprintw(wait_win, 3, (win_width - 20) / 2, "Starting Server...");
        wattroff(wait_win, COLOR_PAIR(COLOR_PAIR_TITLE) | A_BOLD);
        
        wattron(wait_win, COLOR_PAIR(COLOR_PAIR_ACCENT) | A_BLINK);
        mvwprintw(wait_win, 5, (win_width - 15) / 2, "Please wait...");
        wattroff(wait_win, COLOR_PAIR(COLOR_PAIR_ACCENT) | A_BLINK);
        
        wrefresh(wait_win);
        delwin(wait_win);
        
        // ncurses 종료 후 클라이언트 실행
        endwin();
        
        // 점수 파일 생성
        char temp_score_file[] = "/tmp/spacewar_score_XXXXXX";
        int score_fd = mkstemp(temp_score_file);
        if (score_fd == -1) {
            perror("Failed to create temp file");
            cleanup_server();
            return;
        }
        close(score_fd);
        
        pid_t client_pid = fork();
        
        if (client_pid == 0) {
            setenv("SPACEWAR_SCORE_FILE", temp_score_file, 1);
            execl("./client", "client", "127.0.0.1", NULL);
            perror("Failed to execute client");
            exit(1);
        } else if (client_pid > 0) {
            int status;
            waitpid(client_pid, &status, 0);
            
            // 점수 읽기
            int final_score = 0;
            FILE* score_file = fopen(temp_score_file, "r");
            if (score_file) {
                fscanf(score_file, "%d", &final_score);
                fclose(score_file);
            }
            unlink(temp_score_file);
            
            // ncurses 재초기화
            initscr();
            cbreak();
            noecho();
            curs_set(0);
            keypad(stdscr, TRUE);
            timeout(100);
            
            if (has_colors()) {
                start_color();
                init_colors();
            }
            
            clear();
            refresh();
            
            if (final_score > 0) {
                save_score(player_name, final_score, "MULTI");
                
                int max_y, max_x;
                getmaxyx(stdscr, max_y, max_x);
                int win_height = 14;
                int win_width  = 50;
                int start_y    = (max_y - win_height) / 2;
                int start_x    = (max_x - win_width)  / 2;

                draw_box_with_shadow(start_y, start_x, win_height, win_width);
                WINDOW* score_win = newwin(win_height, win_width, start_y, start_x);
                wbkgd(score_win, COLOR_PAIR(COLOR_PAIR_NORMAL));

                wattron(score_win, COLOR_PAIR(COLOR_PAIR_BORDER) | A_BOLD);
                box(score_win, 0, 0);
                wattroff(score_win, COLOR_PAIR(COLOR_PAIR_BORDER) | A_BOLD);

                wattron(score_win, COLOR_PAIR(COLOR_PAIR_TITLE) | A_BOLD);
                mvwprintw(score_win, 2, (win_width - 13) / 2, "GAME RESULT");
                wattroff(score_win, COLOR_PAIR(COLOR_PAIR_TITLE) | A_BOLD);
                
                wattron(score_win, COLOR_PAIR(COLOR_PAIR_DECO_BLUE));
                mvwhline(score_win, 3, 1, ACS_HLINE, win_width - 2);
                wattroff(score_win, COLOR_PAIR(COLOR_PAIR_DECO_BLUE));

                wattron(score_win, COLOR_PAIR(COLOR_PAIR_ACCENT) | A_BOLD);
                mvwprintw(score_win, 5, (win_width - 30) / 2, "Final Score: %d", final_score);
                mvwprintw(score_win, 6, (win_width - 30) / 2, "Score saved!");
                wattroff(score_win, COLOR_PAIR(COLOR_PAIR_ACCENT) | A_BOLD);
                
                wattron(score_win, COLOR_PAIR(COLOR_PAIR_STATUS) | A_BLINK);
                mvwprintw(score_win, 10, (win_width - 25) / 2, "Press any key to continue");
                wattroff(score_win, COLOR_PAIR(COLOR_PAIR_STATUS) | A_BLINK);

                wrefresh(score_win);
                nodelay(score_win, FALSE);
                wgetch(score_win);
                delwin(score_win);
            }
            
            cleanup_server();
        }
    }
}

void handle_multiplay_join(void) {
    char player_name[32];
    char server_ip[32];
    
    get_player_name(player_name, sizeof(player_name));
    get_server_ip(server_ip, sizeof(server_ip));
    
    endwin();
    
    // 점수 파일 생성
    char temp_score_file[] = "/tmp/spacewar_score_XXXXXX";
    int score_fd = mkstemp(temp_score_file);
    if (score_fd == -1) {
        perror("Failed to create temp file");
        return;
    }
    close(score_fd);
    
    pid_t pid = fork();
    
    if (pid == 0) {
        setenv("SPACEWAR_SCORE_FILE", temp_score_file, 1);
        execl("./client", "client", server_ip, NULL);
        perror("Failed to execute client");
        exit(1);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        
        // 점수 읽기
        int final_score = 0;
        FILE* score_file = fopen(temp_score_file, "r");
        if (score_file) {
            fscanf(score_file, "%d", &final_score);
            fclose(score_file);
        }
        unlink(temp_score_file);
        
        // ncurses 재초기화
        initscr();
        cbreak();
        noecho();
        curs_set(0);
        keypad(stdscr, TRUE);
        timeout(100);
        
        if (has_colors()) {
            start_color();
            init_colors();
        }
        
        clear();
        refresh();
        
        if (final_score > 0) {
            save_score(player_name, final_score, "MULTI");
            
            int max_y, max_x;
            getmaxyx(stdscr, max_y, max_x);
            int win_height = 14;
            int win_width  = 50;
            int start_y    = (max_y - win_height) / 2;
            int start_x    = (max_x - win_width)  / 2;

            draw_box_with_shadow(start_y, start_x, win_height, win_width);
            WINDOW* score_win = newwin(win_height, win_width, start_y, start_x);
            wbkgd(score_win, COLOR_PAIR(COLOR_PAIR_NORMAL));

            wattron(score_win, COLOR_PAIR(COLOR_PAIR_BORDER) | A_BOLD);
            box(score_win, 0, 0);
            wattroff(score_win, COLOR_PAIR(COLOR_PAIR_BORDER) | A_BOLD);

            wattron(score_win, COLOR_PAIR(COLOR_PAIR_TITLE) | A_BOLD);
            mvwprintw(score_win, 2, (win_width - 13) / 2, "GAME RESULT");
            wattroff(score_win, COLOR_PAIR(COLOR_PAIR_TITLE) | A_BOLD);
            
            wattron(score_win, COLOR_PAIR(COLOR_PAIR_DECO_BLUE));
            mvwhline(score_win, 3, 1, ACS_HLINE, win_width - 2);
            wattroff(score_win, COLOR_PAIR(COLOR_PAIR_DECO_BLUE));

            wattron(score_win, COLOR_PAIR(COLOR_PAIR_ACCENT) | A_BOLD);
            mvwprintw(score_win, 5, (win_width - 30) / 2, "Final Score: %d", final_score);
            mvwprintw(score_win, 6, (win_width - 30) / 2, "Score saved!");
            wattroff(score_win, COLOR_PAIR(COLOR_PAIR_ACCENT) | A_BOLD);
            
            wattron(score_win, COLOR_PAIR(COLOR_PAIR_STATUS) | A_BLINK);
            mvwprintw(score_win, 10, (win_width - 25) / 2, "Press any key to continue");
            wattroff(score_win, COLOR_PAIR(COLOR_PAIR_STATUS) | A_BLINK);

            wrefresh(score_win);
            nodelay(score_win, FALSE);
            wgetch(score_win);
            delwin(score_win);
        }
    }
}