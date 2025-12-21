#include <fcntl.h>  
#include <unistd.h>     
#include <sys/types.h>
#include <sys/stat.h>
#include "common.h"

#define SCORE_FILE_MODE 0644
#define TOP_SCORES_DISPLAY 10

// 파일에서 최고 점수 조회
long highScore(void) {
    int fd = open(SCORE_FILE, O_RDONLY);
    if (fd == -1) return 0; 

    ScoreEntry entry;
    long maxScore = 0;
    ssize_t bytesRead;

    // 최대값 갱신
    while ((bytesRead = read(fd, &entry, sizeof(ScoreEntry))) == sizeof(ScoreEntry)) {
        if (entry.score > maxScore) {
            maxScore = entry.score;
        }
    }

    close(fd);
    return maxScore;
}

// 점수판에 새 기록 추가
void saveScore(const char* name, int score, const char* mode) {

    int fd = open(SCORE_FILE, O_WRONLY | O_CREAT | O_APPEND, SCORE_FILE_MODE);

    if (fd == -1) return;

    ScoreEntry entry;
    strncpy(entry.name, name, sizeof(entry.name) - 1);
    entry.name[sizeof(entry.name) - 1] = '\0';
    entry.score = score;
    strncpy(entry.mode, mode, sizeof(entry.mode) - 1);
    entry.mode[sizeof(entry.mode) - 1] = '\0';
    entry.timestamp = time(NULL);

    write(fd, &entry, sizeof(ScoreEntry)); // 파일에 데이터 기록
    close(fd);
}

// 파일의 모든 점수 데이터 로드 및 메모리 할당
int loadScores(ScoreEntry** entries) {
    int fd = open(SCORE_FILE, O_RDONLY);
    if (fd == -1) {
        *entries = NULL;
        return 0;
    }

    // 파일 끝으로 이동하여 크기 계산
    off_t fileSize = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET); // 다시 처음으로

    int count = fileSize / sizeof(ScoreEntry);
    if (count == 0) {
        close(fd);
        *entries = NULL;
        return 0;
    }

    // 데이터 개수만큼 동적 할당
    *entries = (ScoreEntry*)malloc(sizeof(ScoreEntry) * count);
    if (*entries == NULL) {
        close(fd);
        return 0;
    }

    // 한 번에 다 읽지 않고 반복문으로 읽기
    int i = 0;
    while (i < count && read(fd, &(*entries)[i], sizeof(ScoreEntry)) == sizeof(ScoreEntry)) {
        i++;
    }

    close(fd);
    return i;
}

// 점수 내림차순 정렬
int compareScores(const void* a, const void* b) {
    ScoreEntry* entryA = (ScoreEntry*)a;
    ScoreEntry* entryB = (ScoreEntry*)b;
    
    if (entryB->score != entryA->score)
        return entryB->score - entryA->score;  // 높은 점수 먼저
    
    return entryB->timestamp - entryA->timestamp;  // 같은 점수면 최신 먼저
}

// 점수판 헤더 그리기
void drawScoreboard(WINDOW* win, int width) {
    // 타이틀
    const char* title = "HALL OF FAME";
    wattron(win, COLOR_PAIR(COLOR_PAIR_TITLE) | A_BOLD);
    mvwprintw(win, 1, (width - strlen(title))/2, "%s", title);
    wattroff(win, COLOR_PAIR(COLOR_PAIR_TITLE) | A_BOLD);

    // 헤더
    wattron(win, COLOR_PAIR(COLOR_PAIR_DECO_BLUE));
    mvwhline(win, 2, 1, ACS_HLINE, width - 2);
    mvwprintw(win, 3, 2, "RANK  NAME         SCORE    MODE");
    mvwhline(win, 4, 1, ACS_HLINE, width - 2);
    wattroff(win, COLOR_PAIR(COLOR_PAIR_DECO_BLUE));
}

// 점수판 UI 전체 출력
void handleScoreboard(void) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    // 점수판 창 크기
    int win_height = 22;
    int win_width  = 54;
    int start_y    = (max_y - win_height) / 2;
    int start_x    = (max_x - win_width)  / 2;

    // 배경 그림자 및 메인 윈도우 생성
    draw_box_with_shadow(start_y, start_x, win_height, win_width);
    WINDOW* win = newwin(win_height, win_width, start_y, start_x);
    
    wbkgd(win, COLOR_PAIR(COLOR_PAIR_NORMAL));
    wattron(win, COLOR_PAIR(COLOR_PAIR_BORDER) | A_BOLD);
    box(win, 0, 0);
    wattroff(win, COLOR_PAIR(COLOR_PAIR_BORDER) | A_BOLD);

    drawScoreboard(win, win_width);

    // 점수 로드 및 정렬
    ScoreEntry* entries = NULL;
    int count = loadScores(&entries);
    
    if (count > 0 && entries != NULL) {
        qsort(entries, count, sizeof(ScoreEntry), compareScores);
    }

    // 점수판 순위 출력
    int listStart_y = 5;
    for(int i = 0; i < TOP_SCORES_DISPLAY; i++) {
        if(i >= count) break;
        
        // 1~3등은 색상을 다르게
        int color = (i < 3) ? COLOR_PAIR_SELECTED : COLOR_PAIR_NORMAL;
        if(i < 3) wattron(win, COLOR_PAIR(color) | A_BOLD);
        
        mvwprintw(win, listStart_y + i, 2, 
            " #%-2d  %-10s   %6d   %-6s", 
            i+1, 
            entries[i].name, 
            entries[i].score, 
            entries[i].mode);
            
        if(i < 3) wattroff(win, COLOR_PAIR(color) | A_BOLD);
    }

    // 데이터가 없을 때 안내
    if(count == 0) {
        mvwprintw(win, 10, (win_width - 20)/2, "NO RECORDS FOUND.");
    }

    if(entries) free(entries);

    // 하단 종료 안내
    const char* hint = "PRESS ANY KEY TO CLOSE";
    wattron(win, COLOR_PAIR(COLOR_PAIR_STATUS) | A_BLINK);
    mvwprintw(win, win_height-2, (win_width - strlen(hint))/2, "%s", hint);
    wattroff(win, COLOR_PAIR(COLOR_PAIR_STATUS) | A_BLINK);

    wrefresh(win);
    wgetch(win); // 키 입력 대기
    delwin(win);
}