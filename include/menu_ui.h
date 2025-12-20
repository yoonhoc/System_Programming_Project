#ifndef MENU_UI_H
#define MENU_UI_H

#include <ncurses.h>

// 메뉴 상수
enum {
    MENU_SCOREBOARD = 0,
    MENU_SINGLE,
    MENU_MULTI_HOST,
    MENU_MULTI_JOIN,
    MENU_EXIT,
    MENU_COUNT
};

void initNcurses(void);
void initColors(void);
void reinitNcurses(void);
void drawBackground(void);
void drawMenu(WINDOW* win, int selected);
void getPlayername(char* name, int max_len);
void getServerip(char* ip, int max_len);
WINDOW* createInputwindow(int height, int width, const char* title);

#endif