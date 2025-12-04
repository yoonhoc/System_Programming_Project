#ifndef COMMON_H
#define COMMON_H

#define _CRT_SECURE_NO_WARNINGS
#define _XOPEN_SOURCE_EXTENDED 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <time.h>

#ifdef _WIN32
#include <pdcurses/curses.h>
#else
#include <ncursesw/curses.h>
#endif

// --- 색상 정의 ---
#define COLOR_PAIR_NORMAL       1
#define COLOR_PAIR_SELECTED     2 
#define COLOR_PAIR_TITLE        3
#define COLOR_PAIR_BORDER       4
#define COLOR_PAIR_ACCENT       5
#define COLOR_PAIR_STATUS       6
#define COLOR_PAIR_DECO_BLUE    7 
#define COLOR_PAIR_STAR         8 
#define COLOR_PAIR_CREDITS      9 

#define SCORE_FILE "scores.dat"

// --- 구조체 정의 ---
typedef struct {
    char name[20];
    int score;
    char mode[10]; // "SINGLE" or "MULTI"
    time_t timestamp;
} ScoreEntry;

// --- 함수 프로토타입 선언 (구현은 .c 파일에) ---

// UI 관련 (menu.c에 구현)
void draw_box_with_shadow(int y, int x, int h, int w);
void show_message(const char* title, const char* message);

// 점수 관련 (score.c에 구현)
long get_high_score(void);
void save_score(const char* name, int score, const char* mode);
void handle_scoreboard(void);

#endif