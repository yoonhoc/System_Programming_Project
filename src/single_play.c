#include <curses.h>
#include <unistd.h>
#include <signal.h>
#include "game_logic.h"
#include "view.h"
#include "item.h"
#include "common.h"

volatile sig_atomic_t special_wave = 0;
volatile sig_atomic_t redzone = 0;
GameState state;

//화살 증가, 레드존 이벤트 처리를 위한 알람 핸들러
void event(int sig) {

    signal(SIGALRM, event);
    static int alarmCount = 0;
    alarmCount++;

    // 10초 마다 화살 증가
    if (alarmCount % 1 == 0) {
        state.special_wave = 60;
    }

    // 20초마다 레드존
    if (alarmCount % 2 == 0) {
        
        //레드존 생성
        for (int i = 0; i < MAX_REDZONES; i++) {
            if (!state.redzone[i].active) {
                state.redzone[i].width = 5 + rand() % 8;
                state.redzone[i].height = 3 + rand() % 5;
                state.redzone[i].x = 2 + rand() % (GAME_WIDTH - state.redzone[i].width - 3);
                state.redzone[i].y = 2 + rand() % (GAME_HEIGHT - state.redzone[i].height - 3);
                state.redzone[i].lifetime = 200;
                state.redzone[i].active = 1;
                break;
            }
        }
    }
    alarm(10);
}

int main() {

    int id = 0; 

    view_init();
    init_state(&state, false);

    signal(SIGALRM, event);
    alarm(10); 

    //게임 루프
    while (state.player[id].lives > 0) {  // status 제거

        int ch = getch();
       
        switch(ch) {

            case KEY_LEFT: 
                if (state.player[id].x > 1) {  // 배열 접근으로 수정
                    state.player[id].x--; 
                }
                break;  // if 밖으로 이동
                
            case KEY_RIGHT: 
                if (state.player[id].x < GAME_WIDTH - 2) {
                    state.player[id].x++; 
                }
                break;
                
            case KEY_UP:    
                if (state.player[id].y > 1) {  
                    state.player[id].y--; 
                }
                break;
                
            case KEY_DOWN:  
                if (state.player[id].y < GAME_HEIGHT - 2) {
                    state.player[id].y++; 
                }
                break;
                
            case '1': 
                invincibleItem(&state.player[id]);  // status 제거, 배열 접근
                break;
                
            case '2': 
                healItem(&state.player[id]);  // status 제거, 배열 접근
                break;
                
            case '3': 
                slowItem(&state.player[id]);  // status 제거, 배열 접근
                break;
                
            case 'q': 
            case 'Q':
                state.player[id].lives = 0;  // status 제거, 배열 접근
                break;
        }
        flushinp();

        update_game_world(&state, GAME_WIDTH, GAME_HEIGHT);

        // 3. Draw
        view_draw_state(&state, id, state.frame);
        view_refresh();

        usleep(50000); // ~20 FPS
    }

    // Game Over
    alarm(0);
    int level = state.player[id].score / 100;
    view_draw_single_player_game_over(state.player[id].score, level);

    view_teardown();

    return 0;
}