#include "menu_ui.h"
#include "common.h"
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <locale.h>
 
// 색상 쌍 정의
void initColors(void) {
    init_pair(COLOR_PAIR_NORMAL, COLOR_WHITE, COLOR_BLACK);
    init_pair(COLOR_PAIR_SELECTED,  COLOR_YELLOW,  COLOR_BLUE); 
    init_pair(COLOR_PAIR_TITLE,     COLOR_MAGENTA, COLOR_BLACK); 
    init_pair(COLOR_PAIR_BORDER,    COLOR_CYAN,    COLOR_BLACK);
    init_pair(COLOR_PAIR_ACCENT,    COLOR_GREEN,   COLOR_BLACK);
    init_pair(COLOR_PAIR_STATUS,    COLOR_BLACK,   COLOR_MAGENTA);
    init_pair(COLOR_PAIR_DECO_BLUE, COLOR_BLUE,    COLOR_BLACK);
    init_pair(COLOR_PAIR_STAR,      COLOR_WHITE,   COLOR_BLACK);
    init_pair(COLOR_PAIR_CREDITS,   COLOR_YELLOW,  COLOR_BLACK); 
}

// ncurses 초기화
void initNcurses(void) {
    setlocale(LC_ALL, "");
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    timeout(100); 

    if (has_colors()) {
        start_color();
        initColors();
    }
}

// Ncurses 재시동
void reinitNcurses(void) {
    // stdin 블로킹 모드 복구
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (flags != -1) {
        fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK);
    }

    clear();
    refresh();
    
    // 색상, 모드 재설정
    if (has_colors()) {
        start_color();
        initColors();
    }
    
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    timeout(100); 
    
    touchwin(stdscr);
    wrefresh(stdscr);
}

// 메인 배경, 타이틀 
void drawBackground(void) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    bkgd(COLOR_PAIR(COLOR_PAIR_NORMAL));
    erase();

    // 외곽 테두리
    attron(COLOR_PAIR(COLOR_PAIR_BORDER) | A_BOLD);
    box(stdscr, 0, 0);
    attroff(COLOR_PAIR(COLOR_PAIR_BORDER) | A_BOLD);

    // 로고 출력
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

    // 하단 상태바
    int status_y = max_y - 3; 
    int status_w = max_x - 2;

    attron(COLOR_PAIR(COLOR_PAIR_DECO_BLUE));
    mvwhline(stdscr, status_y - 1, 1, ACS_HLINE, status_w); 
    attroff(COLOR_PAIR(COLOR_PAIR_DECO_BLUE));

    mvhline(status_y, 1, ' ', status_w); 
    attron(COLOR_PAIR(COLOR_PAIR_CREDITS) | A_BOLD);
    
    // 최고 점수 로드
    long maxScore = highScore(); 
    mvprintw(status_y, 3, "HIGH SCORE: %ld", maxScore);
    
    const char* credits = "0204 v1.3"; 
    mvprintw(status_y, max_x - (int)strlen(credits) - 3, "%s", credits);
    attroff(COLOR_PAIR(COLOR_PAIR_CREDITS) | A_BOLD);

    attron(COLOR_PAIR(COLOR_PAIR_STATUS)); 
    mvhline(max_y - 2, 1, ' ', status_w);
    attroff(COLOR_PAIR(COLOR_PAIR_STATUS));
    refresh();
}

// 메뉴 리스트 출력
void drawMenu(WINDOW* win, int selected) {
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

    const char* titleLines[] = { // MENU
        "#  # #### #  # #  #", 
        "#### #    ## # #  #",
        "#### #### # ## #  #",
        "#  # #    #  # #  #",
        "#  # #### #  # ####"
    };
    int title_height = sizeof(titleLines) / sizeof(titleLines[0]);
    int title_start_y = 2; 

    for (int i = 0; i < title_height; i++) {
        int line_width = (int)strlen(titleLines[i]);
        wattron(win, COLOR_PAIR(COLOR_PAIR_TITLE) | A_BOLD | A_BLINK);
        mvwprintw(win, title_start_y + i, (width - line_width) / 2, "%s", titleLines[i]);
        wattroff(win, COLOR_PAIR(COLOR_PAIR_TITLE) | A_BOLD | A_BLINK);
    }

    int divider_y = title_start_y + title_height + 2; 
    wattron(win, COLOR_PAIR(COLOR_PAIR_DECO_BLUE));
    mvwhline(win, divider_y, 1, ACS_HLINE, width - 2);
    mvwaddch(win, divider_y, 0, ACS_LTEE);
    mvwaddch(win, divider_y, width - 1, ACS_RTEE);
    wattroff(win, COLOR_PAIR(COLOR_PAIR_DECO_BLUE));

    // 메뉴 항목 루프
    int start_y = divider_y + 2; 
    int x_menu  = 6;

    for (int i = 0; i < MENU_COUNT; i++) {
        int y = start_y + i * 2;
        if (i == selected) {
            // 선택된 항목 강조
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

// 입력창 윈도우 생성
WINDOW* createInputwindow(int height, int width, const char* title) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    int start_y = (max_y - height) / 2;
    int start_x = (max_x - width) / 2;

    draw_box_with_shadow(start_y, start_x, height, width);
    WINDOW* win = newwin(height, width, start_y, start_x);
    wbkgd(win, COLOR_PAIR(COLOR_PAIR_NORMAL));

    wattron(win, COLOR_PAIR(COLOR_PAIR_BORDER) | A_BOLD);
    box(win, 0, 0);
    wattroff(win, COLOR_PAIR(COLOR_PAIR_BORDER) | A_BOLD);

    wattron(win, COLOR_PAIR(COLOR_PAIR_TITLE) | A_BOLD);
    mvwprintw(win, 2, (width - strlen(title)) / 2, "%s", title);
    wattroff(win, COLOR_PAIR(COLOR_PAIR_TITLE) | A_BOLD);
    
    wattron(win, COLOR_PAIR(COLOR_PAIR_DECO_BLUE));
    mvwhline(win, 3, 1, ACS_HLINE, width - 2);
    wattroff(win, COLOR_PAIR(COLOR_PAIR_DECO_BLUE));

    return win;
}

// 플레이어 이름 입력 받기
void getPlayername(char* name, int max_len) {
    WINDOW* inputwin = createInputwindow(12, 50, "ENTER YOUR NAME");

    wattron(inputwin, COLOR_PAIR(COLOR_PAIR_ACCENT));
    mvwprintw(inputwin, 5, 5, "Name: ");
    wattroff(inputwin, COLOR_PAIR(COLOR_PAIR_ACCENT));

    wattron(inputwin, COLOR_PAIR(COLOR_PAIR_STATUS) | A_DIM);
    mvwprintw(inputwin, 8, (50 - 30) / 2, "(Max %d chars, Enter to confirm)", max_len - 1);
    wattroff(inputwin, COLOR_PAIR(COLOR_PAIR_STATUS) | A_DIM);

    wrefresh(inputwin);
    echo();
    curs_set(1);
    nodelay(inputwin, FALSE);
    
    mvwgetnstr(inputwin, 5, 11, name, max_len - 1);
    
    noecho();
    curs_set(0);
    
    if (strlen(name) == 0) {
        strcpy(name, "PLAYER");
    }
    delwin(inputwin);
}

// 서버 IP 입력 받기
void getServerip(char* ip, int max_len) {
    WINDOW* input_win = createInputwindow(14, 50, "MULTIPLAYER SETUP");

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