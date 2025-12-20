#include "menu_ui.h"
#include "launcher.h"
#include "common.h"
#include <time.h>
#include <stdlib.h>
#include <stdio.h> 

int main(void) {
    int selected = 0;
    int running = 1;
    int ch; 

    srand((unsigned int)time(NULL)); 
    initNcurses(); // Ncurses 초기화

    int max_y, max_x;
    int win_height = 22; 
    int win_width  = 54; 
    
    getmaxyx(stdscr, max_y, max_x);
    int start_y = (max_y - win_height) / 2;
    int start_x = (max_x - win_width)  / 2;

    // 메뉴 윈도우 생성
    WINDOW* menu_win = newwin(win_height, win_width, start_y, start_x);
    keypad(menu_win, TRUE);

    while (running) {
        // 화면 리사이징 대응
        getmaxyx(stdscr, max_y, max_x); 
        start_y = (max_y - win_height) / 2;
        start_x = (max_x - win_width)  / 2;
        mvwin(menu_win, start_y, start_x); 

        // 화면 작으면 경고 출력
        if (max_y < win_height + 4 || max_x < win_width + 4) { 
            erase();
            mvprintw(max_y/2, max_x/2 - 15, "Screen too small! Please resize.");
            refresh();
            ch = getch(); 
            if (ch == 'q' || ch == 'Q' || ch == 27) running = 0;
            continue;
        }

        // UI 렌더링 
        drawBackground();
        draw_box_with_shadow(start_y, start_x, win_height, win_width);
        drawMenu(menu_win, selected);
        wrefresh(menu_win);

        ch = wgetch(menu_win); 

        // 입력 처리
        switch (ch) {
            case KEY_UP: case 'k':
                selected = (selected - 1 + MENU_COUNT) % MENU_COUNT;
                break;
            case KEY_DOWN: case 'j':
                selected = (selected + 1) % MENU_COUNT;
                break;
            case '\n': case KEY_ENTER:
                switch (selected) {
                    case MENU_SCOREBOARD: 
                        handleScoreboard(); 
                        reinitNcurses();
                        break;
                    case MENU_SINGLE:     
                        handleSingleplay();
                        reinitNcurses();
                        break;
                    case MENU_MULTI_HOST:      
                        handleMultihost();
                        reinitNcurses();
                        break;
                    case MENU_MULTI_JOIN:
                        handleMultijoin();
                        reinitNcurses();
                        break;
                    case MENU_EXIT:       
                        running = 0;         
                        break;
                }
                break;
            case 'q': case 'Q': case 27:
                running = 0;
                break;
        }
    }

    // 종료 처리
    delwin(menu_win);
    endwin();
    
    cleanServer(); // 좀비 서버 정리
    
    printf("Game terminated.\n");
    return 0;
}