#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <curses.h>
#include "ui_utils.h"

#define SCORE_FILE "scores.dat"

// 색상 쌍 정의 (menu.c와 동일하게)
#define COLOR_PAIR_NORMAL       1
#define COLOR_PAIR_SELECTED     2
#define COLOR_PAIR_TITLE        3
#define COLOR_PAIR_BORDER       4
#define COLOR_PAIR_ACCENT       5
#define COLOR_PAIR_STATUS       6
#define COLOR_PAIR_DECO_BLUE    7

// ScoreEntry 구조체 정의
typedef struct {
    char name[50];
    long score;
    char mode[20];
    time_t timestamp;
} ScoreEntry;

// 시스템 프로그래밍 헤더 (System Calls)
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

// --------------------------------------------------------
// 시스템 콜을 활용한 점수 관리 함수 구현
// --------------------------------------------------------

// 파일에서 모든 점수를 읽어 최고 점수를 반환 (시스템 콜 read 사용)
long get_high_score(void) {
    int fd = open(SCORE_FILE, O_RDONLY);
    if (fd == -1) return 0; // 파일이 없으면 0점

    ScoreEntry entry;
    long max_score = 0;
    ssize_t bytes_read;

    // 파일 끝까지 구조체 단위로 읽기
    while ((bytes_read = read(fd, &entry, sizeof(ScoreEntry))) == sizeof(ScoreEntry)) {
        if (entry.score > max_score) {
            max_score = entry.score;
        }
    }

    close(fd);
    return max_score;
}

// 점수 저장 (시스템 콜 write 사용, O_APPEND로 뒤에 추가)
void save_score(const char* name, int score, const char* mode) {
    // rw-r--r-- 권한으로 파일 열기/생성
    #ifdef _WIN32
    int fd = open(SCORE_FILE, O_WRONLY | O_CREAT | O_APPEND | O_BINARY, 0644);
    #else
    int fd = open(SCORE_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
    #endif

    if (fd == -1) return;

    ScoreEntry entry;
    strncpy(entry.name, name, sizeof(entry.name) - 1);
    entry.name[sizeof(entry.name) - 1] = '\0';
    entry.score = score;
    strncpy(entry.mode, mode, sizeof(entry.mode) - 1);
    entry.mode[sizeof(entry.mode) - 1] = '\0';
    entry.timestamp = time(NULL);

    write(fd, &entry, sizeof(ScoreEntry));
    close(fd);
}

// 내부 함수: 점수판 출력을 위한 모든 점수 로드
int load_all_scores(ScoreEntry** entries) {
    int fd = open(SCORE_FILE, O_RDONLY);
    if (fd == -1) {
        *entries = NULL;
        return 0;
    }

    struct stat st;
    if (fstat(fd, &st) == -1) {
        close(fd);
        *entries = NULL;
        return 0;
    }

    int count = st.st_size / sizeof(ScoreEntry);
    if (count == 0) {
        close(fd);
        *entries = NULL;
        return 0;
    }

    *entries = (ScoreEntry*)malloc(sizeof(ScoreEntry) * count);
    if (*entries == NULL) {
        close(fd);
        return 0;
    }

    read(fd, *entries, sizeof(ScoreEntry) * count);
    close(fd);
    return count;
}

// 내부 함수: 정렬 비교 함수 (내림차순)
int compare_scores(const void* a, const void* b) {
    ScoreEntry* entryA = (ScoreEntry*)a;
    ScoreEntry* entryB = (ScoreEntry*)b;
    return entryB->score - entryA->score;
}

// 점수판 UI 핸들러
void handle_scoreboard(void) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    // 스코어보드 창 크기
    int win_height = 22;
    int win_width  = 54;
    int start_y    = (max_y - win_height) / 2;
    int start_x    = (max_x - win_width)  / 2;

    // menu.c에 있는 UI 함수 재사용 (common.h에 선언되어 있어 호출 가능)
    WINDOW* win = newwin(win_height, win_width, start_y, start_x);
    draw_box_with_shadow(win, win_height, win_width, start_y, start_x);
    
    wbkgd(win, COLOR_PAIR(COLOR_PAIR_NORMAL));
    wattron(win, COLOR_PAIR(COLOR_PAIR_BORDER) | A_BOLD);
    box(win, 0, 0);
    wattroff(win, COLOR_PAIR(COLOR_PAIR_BORDER) | A_BOLD);

    // 타이틀
    const char* title = "HALL OF FAME";
    wattron(win, COLOR_PAIR(COLOR_PAIR_TITLE) | A_BOLD);
    mvwprintw(win, 1, (win_width - strlen(title))/2, "%s", title);
    wattroff(win, COLOR_PAIR(COLOR_PAIR_TITLE) | A_BOLD);

    // 헤더
    wattron(win, COLOR_PAIR(COLOR_PAIR_DECO_BLUE));
    mvwhline(win, 2, 1, ACS_HLINE, win_width - 2);
    mvwprintw(win, 3, 2, "RANK  NAME         SCORE    MODE");
    mvwhline(win, 4, 1, ACS_HLINE, win_width - 2);
    wattroff(win, COLOR_PAIR(COLOR_PAIR_DECO_BLUE));

    // 점수 로드 및 정렬
    ScoreEntry* entries = NULL;
    int count = load_all_scores(&entries);
    
    if (count > 0 && entries != NULL) {
        qsort(entries, count, sizeof(ScoreEntry), compare_scores);
    }

    // 상위 10개 출력
    int list_start_y = 5;
    for(int i = 0; i < 10; i++) {
        if(i >= count) break;
        
        // 1~3등은 색상을 다르게
        int color = (i < 3) ? COLOR_PAIR_SELECTED : COLOR_PAIR_NORMAL;
        if(i < 3) wattron(win, COLOR_PAIR(color) | A_BOLD);
        
        mvwprintw(win, list_start_y + i, 2, 
            " #%-2d  %-10s   %6ld   %-6s", 
            i+1, 
            entries[i].name, 
            entries[i].score, 
            entries[i].mode);
            
        if(i < 3) wattroff(win, COLOR_PAIR(color) | A_BOLD);
    }

    if(count == 0) {
        mvwprintw(win, 10, (win_width - 20)/2, "NO RECORDS FOUND.");
    }

    if(entries) free(entries);

    // 하단 안내
    const char* hint = "PRESS ANY KEY TO CLOSE";
    wattron(win, COLOR_PAIR(COLOR_PAIR_STATUS) | A_BLINK);
    mvwprintw(win, win_height-2, (win_width - strlen(hint))/2, "%s", hint);
    wattroff(win, COLOR_PAIR(COLOR_PAIR_STATUS) | A_BLINK);

    wrefresh(win);
    wgetch(win);
    delwin(win);
}