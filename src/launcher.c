#include "launcher.h"
#include "menu_ui.h" 
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>

// 서버 PID (관리용)
static pid_t server_pid = -1;

// 서버 프로세스 강제종료
void cleanServer(void) {
    if (server_pid > 0) {
        kill(server_pid, SIGTERM);
        waitpid(server_pid, NULL, 0);
        server_pid = -1;
    }
}

// 게임 프로세스 실행 공통 로직
static void executeGameprocess(const char* program, char* const argv[], const char* temp_file, 
                          const char* player_name, const char* mode_str) {
    pid_t pid = fork();
    
    if (pid == 0) {
        // 자식 프로세스
        setenv("SPACEWAR_SCORE_FILE", temp_file, 1);
        execv(program, argv);
        perror("Failed to execute game");
        exit(1);
    } else if (pid > 0) {
        // 부모 프로세스
        waitpid(pid, NULL, 0);
        processGameresult(temp_file, player_name, mode_str);
    }
}

// 결과 파일 읽기 및 UI 표시
void processGameresult(const char* temp_file, const char* player_name, const char* mode_str) {
    int finalScore = 0;

    FILE* scoreFile = fopen(temp_file, "r");
    if (scoreFile) {
        fscanf(scoreFile, "%d", &finalScore);
        fclose(scoreFile);
    }
    unlink(temp_file);

    // ncurses 재초기화
    reinitNcurses(); 

    if (finalScore > 0) {
        // 점수 저장
        saveScore(player_name, finalScore, mode_str);

        int max_y, max_x;
        getmaxyx(stdscr, max_y, max_x);
        int win_height = 14;
        int win_width  = 50;
        int start_y    = (max_y - win_height) / 2;
        int start_x    = (max_x - win_width)  / 2;

        // 결과창 UI 그리기
        draw_box_with_shadow(start_y, start_x, win_height, win_width);
        WINDOW* scoreWin = newwin(win_height, win_width, start_y, start_x);
        wbkgd(scoreWin, COLOR_PAIR(COLOR_PAIR_NORMAL));

        wattron(scoreWin, COLOR_PAIR(COLOR_PAIR_BORDER) | A_BOLD);
        box(scoreWin, 0, 0);
        wattroff(scoreWin, COLOR_PAIR(COLOR_PAIR_BORDER) | A_BOLD);

        wattron(scoreWin, COLOR_PAIR(COLOR_PAIR_TITLE) | A_BOLD);
        mvwprintw(scoreWin, 2, (win_width - 13) / 2, "GAME RESULT");
        wattroff(scoreWin, COLOR_PAIR(COLOR_PAIR_TITLE) | A_BOLD);
        
        wattron(scoreWin, COLOR_PAIR(COLOR_PAIR_DECO_BLUE));
        mvwhline(scoreWin, 3, 1, ACS_HLINE, win_width - 2);
        wattroff(scoreWin, COLOR_PAIR(COLOR_PAIR_DECO_BLUE));

        wattron(scoreWin, COLOR_PAIR(COLOR_PAIR_ACCENT) | A_BOLD);
        mvwprintw(scoreWin, 5, (win_width - 30) / 2, "Final Score: %d", finalScore);
        mvwprintw(scoreWin, 6, (win_width - 30) / 2, "Score saved!");
        wattroff(scoreWin, COLOR_PAIR(COLOR_PAIR_ACCENT) | A_BOLD);
        
        wattron(scoreWin, COLOR_PAIR(COLOR_PAIR_STATUS) | A_BLINK);
        mvwprintw(scoreWin, 10, (win_width - 25) / 2, "Press any key to continue");
        wattroff(scoreWin, COLOR_PAIR(COLOR_PAIR_STATUS) | A_BLINK);

        wrefresh(scoreWin);
        nodelay(scoreWin, FALSE);
        wgetch(scoreWin);
        delwin(scoreWin);
    }
}

// 싱글 플레이 시작
void handleSingleplay(void) {
    char playerName[32];
    getPlayername(playerName, sizeof(playerName));
    
    endwin(); // 게임 실행 전 ncurses 종료
    
    char tempScorefile[] = "/tmp/spacewar_score_XXXXXX";
    int scoreFd = mkstemp(tempScorefile);
    if (scoreFd == -1) {
        perror("Failed to create temp file");
        return;
    }
    close(scoreFd);
    
    char* argv[] = {"single_play", NULL};
    executeGameprocess("./single_play", argv, tempScorefile, playerName, "SINGLE");
}

// 멀티플레이 Host 시작
void handleMultihost(void) {
    char player_name[32];
    getPlayername(player_name, sizeof(player_name));
    
    // 서버 프로세스 실행
    server_pid = fork();
    if (server_pid == 0) {
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        execl("./server", "server", NULL);
        exit(1);
    } else if (server_pid > 0) {
        sleep(1); // 서버 켜질 때까지 잠깐 대기

        // 서버 대기 UI
        int max_y, max_x;
        getmaxyx(stdscr, max_y, max_x);
        int win_h = 10, win_w = 50;
        int sy = (max_y - win_h)/2, sx = (max_x - win_w)/2;
        WINDOW* waitWin = newwin(win_h, win_w, sy, sx);
        wbkgd(waitWin, COLOR_PAIR(COLOR_PAIR_NORMAL));
        box(waitWin, 0, 0);
        wattron(waitWin, COLOR_PAIR(COLOR_PAIR_TITLE) | A_BOLD);
        mvwprintw(waitWin, 3, (win_w-20)/2, "Starting Server...");
        wattroff(waitWin, COLOR_PAIR(COLOR_PAIR_TITLE) | A_BOLD);
        wattron(waitWin, COLOR_PAIR(COLOR_PAIR_ACCENT) | A_BLINK);
        mvwprintw(waitWin, 5, (win_w-15)/2, "Please wait...");
        wattroff(waitWin, COLOR_PAIR(COLOR_PAIR_ACCENT) | A_BLINK);
        wrefresh(waitWin);
        delwin(waitWin);

        endwin();

        // 임시 파일 생성
        char temp_score_file[] = "/tmp/spacewar_score_XXXXXX";
        int score_fd = mkstemp(temp_score_file);
        if (score_fd == -1) { 
            cleanServer(); 
            return; 
        }
        close(score_fd);

        // 클라이언트 실행
        char* argv[] = {"client", "127.0.0.1", NULL};
        executeGameprocess("./client", argv, temp_score_file, player_name, "MULTI");
        cleanServer(); // 게임 끝나면 서버 끔
    }
}

// 멀티플레이 참여 Join 시작
void handleMultijoin(void) {
    char playerName[32];
    char serverIp[32];
    
    getPlayername(playerName, sizeof(playerName));
    getServerip(serverIp, sizeof(serverIp));
    
    endwin();
    
    char temp_score_file[] = "/tmp/spacewar_score_XXXXXX";
    int score_fd = mkstemp(temp_score_file);
    if (score_fd == -1) { 
        perror("mkstemp"); 
        return; 
    }
    close(score_fd);
    
    char* argv[] = {"client", serverIp, NULL};
    executeGameprocess("./client", argv, temp_score_file, playerName, "MULTI");
}