#include "view.h"
#include "game_logic.h" 
#include <unistd.h>    
#include <string.h>

void view_init() {
    initscr();                  // ncurses 모드 시작
    raw();                      // 라인 버퍼링 비활성화 (Enter 없이 즉시 입력 받음, Ctrl+C 등 시그널 전달 안 함)
    noecho();                   // 입력한 키가 화면에 출력되지 않도록 설정
    curs_set(0);                // 커서(깜빡이는 밑줄)를 숨김
    keypad(stdscr, TRUE);       // 특수 키(화살표, F1 등) 입력을 허용
    nodelay(stdscr, TRUE);      // 입력 대기 시간을 0으로 설정 (Non-blocking 입력) -> 게임 루프가 멈추지 않게 함
    timeout(0);                 // getch() 호출 시 대기 시간 없음
    start_color();              // 색상 모드 시작
    use_default_colors();       // 터미널 기본 색상 사용

    // 색상 쌍(Color Pair) 정의: init_pair(번호, 글자색, 배경색)
    if (hasColors()) {
        init_pair(1, COLOR_RED, -1);        // 1: 빨강 (위험 요소)
        init_pair(2, COLOR_RED, COLOR_RED); // 2: 배경까지 빨강 (레드존 채우기)
        init_pair(3, COLOR_YELLOW, -1);     // 3: 노랑 (내 캐릭터, 내 공격)
        init_pair(4, COLOR_GREEN, -1);      // 4: 초록 (아이템, UI 강조)
        init_pair(5, COLOR_CYAN, -1);       // 5: 청록 (슬로우 상태)
        init_pair(6, COLOR_BLUE, -1);       // 6: 파랑 (상대 캐릭터, 상대 공격)
        init_pair(7, COLOR_MAGENTA, -1);    // 7: 자홍
    }
    intrflush(stdscr, FALSE);   // 인터럽트 발생 시 출력 버퍼 비우기 방지 (화면 깨짐 방지)
}


void draw_game(const GameState* game_state, int id, int frame) {
    erase(); // 그리기 전 화면 초기화
    
    // 특수 웨이브 경고 효과
    // 10프레임 중 5프레임은 반전/굵게 표시하여 깜빡이는 효과를 줌
    if (game_state->special_wave > 0 && (frame % 10 < 5)) {
        attron(A_REVERSE | A_BOLD);
    }

    // 1. 게임 테두리
    mvhline(0, 1, ACS_HLINE, GAME_WIDTH - 2);
    mvhline(GAME_HEIGHT - 1, 1, ACS_HLINE, GAME_WIDTH - 2);
    mvvline(1, 0, ACS_VLINE, GAME_HEIGHT - 2);
    mvvline(1, GAME_WIDTH - 1, ACS_VLINE, GAME_HEIGHT - 2);
    mvaddch(0, 0, ACS_ULCORNER);                    // 좌상단
    mvaddch(0, GAME_WIDTH - 1, ACS_URCORNER);       // 우상단
    mvaddch(GAME_HEIGHT - 1, 0, ACS_LLCORNER);      // 좌하단
    mvaddch(GAME_HEIGHT - 1, GAME_WIDTH - 1, ACS_LRCORNER); // 우하단

    // 특수 웨이브 효과 끄기
    if (game_state->special_wave > 0 && (frame % 10 < 5)) {
        attroff(A_REVERSE | A_BOLD);
    }

    // 2. 레드존
    if (hasColors()) attron(COLOR_PAIR(2)); // 배경까지 빨간색
    for (int i = 0; i < MAX_REDZONES; i++) {
        if (game_state->redzone[i].active) {
            for (int y = 0; y < game_state->redzone[i].height; y++) {
                for (int x = 0; x < game_state->redzone[i].width; x++) {
                    int py = game_state->redzone[i].y + y;
                    int px = game_state->redzone[i].x + x;
                    // 화면 범위 내에만 출력
                    if (py > 0 && py < GAME_HEIGHT - 1 && px > 0 && px < GAME_WIDTH - 1)
                        mvaddch(py, px, '#');
                }
            }
        }
    }
    if (hasColors()) attroff(COLOR_PAIR(2));

    // 3. 플레이어 그리기
    for (int i = 0; i < 2; i++) {
        if (!game_state->player[i].connected) continue; // 연결된 플레이어만
        
        char p_char = (i == 0) ? '@' : '$'; // P1: @, P2: $
        int color = (i == id) ? 3 : 6; // 나: 노랑(3), 상대: 파랑(6)
        int attrs = 0;

        // 무적 상태이거나 피격 후 쿨타임 중일 때 깜빡임 효과 
        if (i == id && (game_state->player[i].invincible || game_state->player[i].damage_cooldown > 0)) {
            if (frame % 4 < 2) attrs = A_BOLD; // 밝게 표시 
        }

        if (hasColors()) attron(COLOR_PAIR(color) | attrs);
        mvaddch(game_state->player[i].y, game_state->player[i].x, p_char);
        if (hasColors()) attroff(COLOR_PAIR(color) | attrs);
    }

    // 4. 화살 그리기
    bool slow = game_state->player[0].slow || game_state->player[1].slow;
    for (int i = 0; i < MAX_ARROWS; i++) {
        if (!game_state->arrow[i].active) continue;
        
        int color = 0;
        int attr = 0;

        if (game_state->arrow[i].special == 2) { // 플레이어가 발사한 공격
            attr = A_BOLD;
            if (game_state->arrow[i].owner == id) {
                color = 3; // 내 공격 (노랑)
            } else {
                color = 6; // 상대 공격 (파랑)
            }
        }
        else if (game_state->arrow[i].special == 1) { // 특수 패턴 화살
            color = 1; // 빨강
            attr = A_BOLD; 
        } 
        else if (slow) { // 누군가 슬로우 아이템 사용 시
             color = 5; // 화살 색 변경 (청록)
        } 

        if (hasColors() && color) attron(COLOR_PAIR(color) | attr);
        mvaddch(game_state->arrow[i].y, game_state->arrow[i].x, game_state->arrow[i].symbol);
        if (hasColors() && color) attroff(COLOR_PAIR(color) | attr);
    }

    // 5. UI 및 정보 표시
    int opponent_id = (id == 0) ? 1 : 0;
    
    // 상단: 점수 및 생명력(하트) 표시
    mvprintw(0, 2, " P%d Score:%d ", id + 1, game_state->player[id].score);
    mvprintw(0, 45, " Lives:");
    for(int i=0; i<game_state->player[id].lives; i++) addstr("<3");
    
    // 상대방 생명력 표시 (연결된 경우만)
    if (game_state->player[opponent_id].connected) {
        mvprintw(0, 65, " Enemy:");
        for(int k=0; k<game_state->player[opponent_id].lives; k++) addstr("<3");
    }

    // 특수 웨이브 알림 텍스트
    if (game_state->special_wave > 0) mvprintw(1, 2, " SPECIAL WAVE! ");

    // 하단: 보유 아이템 개수 표시
    if (hasColors()) attron(COLOR_PAIR(4)); // 초록색
    mvprintw(GAME_HEIGHT-1, 2, "[1]Inv:%d [2]Heal:%d [3]Slow:%d", 
        game_state->player[id].invincible_item,
        game_state->player[id].heal_item,
        game_state->player[id].slow_item);
    if (hasColors()) attroff(COLOR_PAIR(4));
}


void gameOverScreen(int winner_id, int id, int score) {
    clear();
    box(stdscr, 0, 0); // 테두리
    attron(A_BOLD);
    
    // 승패 메시지 중앙 정렬
    if (winner_id == id) mvprintw(GAME_HEIGHT/2, (GAME_WIDTH-10)/2, "YOU WIN!");
    else if (winner_id == -1) mvprintw(GAME_HEIGHT/2, (GAME_WIDTH-10)/2, "DRAW!");
    else mvprintw(GAME_HEIGHT/2, (GAME_WIDTH-10)/2, "YOU LOSE!");
    attroff(A_BOLD);
    
    mvprintw(GAME_HEIGHT/2 + 2, (GAME_WIDTH-30)/2, "Final Score: %d", score);
}


void singleGameOverScreen(int score, int level) {
    clear();
    if (hasColors()) attron(COLOR_PAIR(1) | A_BOLD); // 빨간색 강조
    box(stdscr, 0, 0);
    
    // 아스키 아트 스타일 UI
    mvprintw(GAME_HEIGHT / 2 - 4, (GAME_WIDTH - 50) / 2, "================================================");
    mvprintw(GAME_HEIGHT / 2 - 3, (GAME_WIDTH - 50) / 2, "||                                            ||");
    mvprintw(GAME_HEIGHT / 2 - 2, (GAME_WIDTH - 50) / 2, "||              G A M E   O V E R             ||");
    mvprintw(GAME_HEIGHT / 2 - 1, (GAME_WIDTH - 50) / 2, "||                                            ||");
    mvprintw(GAME_HEIGHT / 2,     (GAME_WIDTH - 50) / 2, "================================================");
    if (hasColors()) attroff(COLOR_PAIR(1) | A_BOLD);
    
    // 최종 점수 및 도달 레벨 표시
    mvprintw(GAME_HEIGHT / 2 + 2, (GAME_WIDTH - 30) / 2, "Final Score: %d", score);
    mvprintw(GAME_HEIGHT / 2 + 3, (GAME_WIDTH - 30) / 2, "Level Reached: %d", level);
    mvprintw(GAME_HEIGHT / 2 + 5, (GAME_WIDTH - 30) / 2, "Press any key to exit...");
    
    refresh();
    nodelay(stdscr, FALSE); // 종료 화면에서는 키 입력을 기다림 (Blocking 모드 전환)
    getch();                // 키 입력 대기
}