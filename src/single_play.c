#include <curses.h>
#include <unistd.h>
#include <signal.h>
#include "game_logic.h"
#include "view.h"
#include "item.h"

volatile sig_atomic_t special_wave_triggered = 0;
volatile sig_atomic_t redzone_triggered = 0;

void alarm_handler(int sig) {
    signal(SIGALRM, alarm_handler);
    static int alarm_count = 0;
    alarm_count++;

    // Trigger special wave every 10 seconds
    if (alarm_count % 1 == 0) {
        special_wave_triggered = 1;
    }

    // Trigger redzone every 20 seconds
    if (alarm_count % 2 == 0) {
        redzone_triggered = 1;
    }
}

int main(void) {
    GameState game_state;
    int my_player_id = 0; // In single player, we are always player 0

    // Initialization
    view_init();
    init_game_state(&game_state, false); // false for single player

    signal(SIGALRM, alarm_handler);
    alarm(10); // Alarm every 10 seconds

    // Game loop
    while (game_state.players[my_player_id].status.lives > 0) {
        // 1. Handle Input
        int ch = getch();
        Player* player = &game_state.players[my_player_id];

        switch(ch) {
            case KEY_LEFT:  if (player->x > 1) player->x--; break;
            case KEY_RIGHT: if (player->x < GAME_WIDTH - 2) player->x++; break;
            case KEY_UP:    if (player->y > 1) player->y--; break;
            case KEY_DOWN:  if (player->y < GAME_HEIGHT - 2) player->y++; break;
            case '1': use_invincible_item(&player->status); break;
            case '2': use_heal_item(&player->status); break;
            case '3': use_slow_item(&player->status); break;
            case 'q': case 'Q':
                player->status.lives = 0; // End game
                break;
        }
        flushinp();

        // 2. Update Game State
        if (special_wave_triggered) {
            special_wave_triggered = 0;
            trigger_special_wave(&game_state);
            alarm(10); // Reset alarm
        }

        if (redzone_triggered) {
            redzone_triggered = 0;
            spawn_red_zone(&game_state, GAME_WIDTH, GAME_HEIGHT);
        }

        update_game_world(&game_state, GAME_WIDTH, GAME_HEIGHT);

        // 3. Draw
        view_draw_game_state(&game_state, my_player_id, game_state.frame);
        view_refresh();

        // 4. Timing
        usleep(50000); // ~20 FPS
    }

    // Game Over
    alarm(0);
    int level = game_state.players[my_player_id].score / 100;
    view_draw_single_player_game_over(game_state.players[my_player_id].score, level);

    view_teardown();

    return 0;
}
