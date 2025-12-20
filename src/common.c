#include "common.h"


void draw_box_with_shadow(int y, int x, int h, int w) {
    attron(COLOR_PAIR(COLOR_PAIR_NORMAL) | A_DIM);
    for (int i = 1; i <= h; i++) {
        mvhline(y + i, x + w, ' ', 2);
    }
    mvhline(y + h, x + 2, ' ', w);
    attroff(COLOR_PAIR(COLOR_PAIR_NORMAL) | A_DIM);
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

