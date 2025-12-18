#include "view.h"
#include "game_logic.h" // For GAME_WIDTH, GAME_HEIGHT
#include <unistd.h>     // For sleep()
#include <string.h>

void view_init() {
    initscr();
    raw();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    timeout(0);
    start_color();
    use_default_colors();

    if (has_colors()) {
        init_pair(1, COLOR_RED, -1);
        init_pair(2, COLOR_RED, COLOR_RED);
        init_pair(3, COLOR_YELLOW, -1);
        init_pair(4, COLOR_GREEN, -1);
        init_pair(5, COLOR_CYAN, -1);
        init_pair(6, COLOR_BLUE, -1);
        init_pair(7, COLOR_MAGENTA, -1);
    }
    intrflush(stdscr, FALSE);
}

void view_teardown() {
    endwin();
}

void view_clear() {
    erase();
}

void view_refresh() {
    refresh();
}

void view_draw_game_state(const GameState* game_state, int my_player_id, int frame) {
    erase();
    
    if (game_state->special_wave > 0 && (frame % 10 < 5)) {
        attron(A_REVERSE | A_BOLD);
    }

    // Draw game border manually to fit GAME_WIDTH and GAME_HEIGHT
    mvhline(0, 1, ACS_HLINE, GAME_WIDTH - 2);
    mvhline(GAME_HEIGHT - 1, 1, ACS_HLINE, GAME_WIDTH - 2);
    mvvline(1, 0, ACS_VLINE, GAME_HEIGHT - 2);
    mvvline(1, GAME_WIDTH - 1, ACS_VLINE, GAME_HEIGHT - 2);
    mvaddch(0, 0, ACS_ULCORNER);
    mvaddch(0, GAME_WIDTH - 1, ACS_URCORNER);
    mvaddch(GAME_HEIGHT - 1, 0, ACS_LLCORNER);
    mvaddch(GAME_HEIGHT - 1, GAME_WIDTH - 1, ACS_LRCORNER);

    if (game_state->special_wave > 0 && (frame % 10 < 5)) {
        attroff(A_REVERSE | A_BOLD);
    }

    // Red Zones
    if (has_colors()) attron(COLOR_PAIR(2));
    for (int i = 0; i < MAX_REDZONES; i++) {
        if (game_state->redzone[i].active) {
            for (int ry = 0; ry < game_state->redzone[i].height; ry++) {
                for (int rx = 0; rx < game_state->redzone[i].width; rx++) {
                    int py = game_state->redzone[i].y + ry;
                    int px = game_state->redzone[i].x + rx;
                    if (py > 0 && py < GAME_HEIGHT - 1 && px > 0 && px < GAME_WIDTH - 1)
                        mvaddch(py, px, '#');
                }
            }
        }
    }
    if (has_colors()) attroff(COLOR_PAIR(2));

    // player
    for (int i = 0; i < 2; i++) {
        if (!game_state->player[i].connected) continue;
        
        char p_char = (i == 0) ? '@' : '$';
        int color = (i == my_player_id) ? 3 : 6;
        int attrs = 0;

        // status 제거
        if (i == my_player_id && (game_state->player[i].is_invincible || game_state->player[i].damage_cooldown > 0)) {
            if (frame % 4 < 2) attrs = A_BOLD; // Blinking effect
        }

        if (has_colors()) attron(COLOR_PAIR(color) | attrs);
        mvaddch(game_state->player[i].y, game_state->player[i].x, p_char);
        if (has_colors()) attroff(COLOR_PAIR(color) | attrs);
    }

    // Arrows
    // status 제거
    bool is_any_slow = game_state->player[0].is_slow || game_state->player[1].is_slow;
    for (int i = 0; i < MAX_ARROWS; i++) {
        if (!game_state->arrow[i].active) continue;
        
        int color = 0;
        int attr = 0;

        if (game_state->arrow[i].is_special == 2) { // Player attack
            attr = A_BOLD;
            if (game_state->arrow[i].owner == my_player_id) {
                color = 3; // 내 공격 색상 (노랑)
            } else {
                color = 6; // 상대 공격 색상 (파랑)
            }
        }
        else if (game_state->arrow[i].is_special == 1) { color = 1; attr = A_BOLD; } // Special
        else if (is_any_slow) { color = 5; } // Slow

        if (has_colors() && color) attron(COLOR_PAIR(color) | attr);
        mvaddch(game_state->arrow[i].y, game_state->arrow[i].x, game_state->arrow[i].symbol);
        if (has_colors() && color) attroff(COLOR_PAIR(color) | attr);
    }

    // UI
    int opponent_id = (my_player_id == 0) ? 1 : 0;
    // status 제거
    mvprintw(0, 2, " P%d Score:%d ", my_player_id + 1, game_state->player[my_player_id].score);
    mvprintw(0, 45, " Lives:");
    for(int k=0; k<game_state->player[my_player_id].lives; k++) addstr("<3");
    
    if (game_state->player[opponent_id].connected) {
        mvprintw(0, 65, " Enemy:");
        for(int k=0; k<game_state->player[opponent_id].lives; k++) addstr("<3");
    }

    if (game_state->special_wave > 0) mvprintw(1, 2, " SPECIAL WAVE! ");

    // status 제거
    if (has_colors()) attron(COLOR_PAIR(4));
    mvprintw(GAME_HEIGHT-1, 2, "[1]Inv:%d [2]Heal:%d [3]Slow:%d", 
        game_state->player[my_player_id].invincible_item,
        game_state->player[my_player_id].heal_item,
        game_state->player[my_player_id].slow_item);
    if (has_colors()) attroff(COLOR_PAIR(4));
}

void view_draw_game_over_screen(int winner_id, int my_player_id, int score) {
    clear();
    box(stdscr, 0, 0);
    attron(A_BOLD);
    if (winner_id == my_player_id) mvprintw(GAME_HEIGHT/2, (GAME_WIDTH-10)/2, "YOU WIN!");
    else if (winner_id == -1) mvprintw(GAME_HEIGHT/2, (GAME_WIDTH-10)/2, "DRAW!");
    else mvprintw(GAME_HEIGHT/2, (GAME_WIDTH-10)/2, "YOU LOSE!");
    attroff(A_BOLD);
    
    mvprintw(GAME_HEIGHT/2 + 2, (GAME_WIDTH-30)/2, "Final Score: %d", score);
}

void view_draw_single_player_game_over(int score, int level) {
    clear();
    if (has_colors()) attron(COLOR_PAIR(1) | A_BOLD);
    box(stdscr, 0, 0);
    mvprintw(GAME_HEIGHT / 2 - 4, (GAME_WIDTH - 50) / 2, "================================================");
    mvprintw(GAME_HEIGHT / 2 - 3, (GAME_WIDTH - 50) / 2, "||                                            ||");
    mvprintw(GAME_HEIGHT / 2 - 2, (GAME_WIDTH - 50) / 2, "||              G A M E   O V E R             ||");
    mvprintw(GAME_HEIGHT / 2 - 1, (GAME_WIDTH - 50) / 2, "||                                            ||");
    mvprintw(GAME_HEIGHT / 2,     (GAME_WIDTH - 50) / 2, "================================================");
    if (has_colors()) attroff(COLOR_PAIR(1) | A_BOLD);
    
    mvprintw(GAME_HEIGHT / 2 + 2, (GAME_WIDTH - 30) / 2, "Final Score: %d", score);
    mvprintw(GAME_HEIGHT / 2 + 3, (GAME_WIDTH - 30) / 2, "Level Reached: %d", level);
    mvprintw(GAME_HEIGHT / 2 + 5, (GAME_WIDTH - 30) / 2, "Press any key to exit...");
    refresh();
    nodelay(stdscr, FALSE);
    getch();
}

void view_draw_countdown(int seconds) {
    clear();
    mvprintw(GAME_HEIGHT / 2, (GAME_WIDTH - 20) / 2, "Game Starting...");
    refresh();
    for (int i = seconds; i > 0; i--) {
        mvprintw(GAME_HEIGHT / 2 + 2, (GAME_WIDTH - 10) / 2, " %d ", i);
        refresh();
        sleep(1);
    }
}

void view_draw_message_centered(const char* message, int line_offset) {
    mvprintw(GAME_HEIGHT / 2 + line_offset, (GAME_WIDTH - strlen(message)) / 2, "%s", message);
}