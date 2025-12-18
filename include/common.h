#ifndef COMMON_H
#define COMMON_H

#define _CRT_SECURE_NO_WARNINGS
#define _XOPEN_SOURCE_EXTENDED 1

#include <stdbool.h>
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

// --- 게임 기본 상수 ---
#define MAX_ARROWS 50
#define MAX_REDZONES 10
#define PORT 8888
#define MAX_PLAYERS 2

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

// --- 파일 경로 ---
#define SCORE_FILE "scores.dat"

// --- 패킷 타입 열거형 ---
typedef enum {
    PACKET_PLAYER_MOVE,
    PACKET_ARROW_UPDATE,
    PACKET_REDZONE_UPDATE,
    PACKET_PLAYER_STATUS,
    PACKET_GAME_OVER,
    PACKET_ITEM_USE,
    PACKET_INITIAL_STATE
} PacketType;

// --- 화살 구조체 ---
typedef struct {
    int x, y;
    int dx, dy;
    char symbol;
    int active;
    int special;  // 0: 일반, 1: 특수 웨이브, 2: 플레이어 공격
    int owner;       // 공격을 발사한 플레이어 ID (-1: 환경 공격)
} Arrow;

// --- 레드존 구조체 ---
typedef struct {
    int x, y;
    int width, height;
    int active;
    int lifetime;
} RedZone;


// --- 플레이어 정보 구조체 ---
typedef struct {
    // 위치 정보
    int x, y;
    int id;
    int connected;
    int score;
    
    // 상태 정보 (기존 PlayerStatus 내용)
    int lives;
    int invincible_item;
    int heal_item;
    int slow_item;
    int is_invincible;
    int invincible_frames;//무적타이민ㅇ
    int is_slow;
    int slow_frames;
    int damage_cooldown;
} Player;

// --- 게임 상태 구조체 (서버에서 관리) ---
typedef struct {
    Arrow arrow[MAX_ARROWS];
    RedZone redzone[MAX_REDZONES];
    Player player[MAX_PLAYERS];
    int frame;
    int special_wave;
    bool multiplay;

} GameState;

// --- 네트워크 패킷 구조체 ---
typedef struct {
    PacketType type;
    int player_id;
    union {
        struct {
            int x, y;
        } move;
        
        struct {
            Arrow arrows[MAX_ARROWS];
        } arrows;
        
        struct {
            RedZone redzones[MAX_REDZONES];
        } redzones;
        
        struct {
            Player player;
        } player_status;
        
        struct {
            int item_type;  // 1: invincible, 2: heal, 3: slow
        } item_use;
        
        struct {
            GameState game_state;
        } initial_state;
    } data;
} Packet;

// --- 점수 저장 구조체 ---
typedef struct {
    char name[20];
    int score;
    char mode[10]; // "SINGLE" or "MULTI"
    time_t timestamp;
} ScoreEntry;

#endif