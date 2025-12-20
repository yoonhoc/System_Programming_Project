#ifndef COMMON_H
#define COMMON_H

// =========================================================
// [1] 시스템 설정 및 라이브러리 포함
// =========================================================
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

// =========================================================
// [2] 매크로 상수 정의 (상수 성격별 그룹화)
// =========================================================

// --- 게임 설정 (Game Settings) ---
#define MAX_ARROWS      50
#define MAX_REDZONES    10
#define MAX_PLAYERS     2

// --- 네트워크 설정 (Network Settings) ---
#define PORT            8888

// --- 파일 경로 (File Paths) ---
#define SCORE_FILE      "scores.dat"

// --- 색상 정의 (Color Definitions) ---
#define COLOR_PAIR_NORMAL       1
#define COLOR_PAIR_SELECTED     2 
#define COLOR_PAIR_TITLE        3
#define COLOR_PAIR_BORDER       4
#define COLOR_PAIR_ACCENT       5
#define COLOR_PAIR_STATUS       6
#define COLOR_PAIR_DECO_BLUE    7 
#define COLOR_PAIR_STAR         8 
#define COLOR_PAIR_CREDITS      9 

// =========================================================
// [3] 열거형 정의 (Enums)
// =========================================================

// 패킷 종류 식별
typedef enum {
    INITIAL_STATE,   // 게임 초기 상태
    PLAYER_MOVE,     // 플레이어 이동
    PLAYER_STATUS,   // 플레이어 상태 변경 (아이템 획득 등)
    ARROW_UPDATE,    // 화살 위치 업데이트
    REDZONE_UPDATE,  // 레드존 생성/삭제
    ITEM_USE,        // 아이템 사용
    GAME_OVER        // 게임 종료
} PacketType;

// =========================================================
// [4] 게임 객체 구조체 (Game Objects)
// =========================================================

// 화살 (Arrow)
typedef struct {
    int x, y;
    int dx, dy;
    char symbol;
    int active;
    int special;        // 0: 일반, 1: 특수 웨이브, 2: 플레이어 공격
    int owner;          // 공격을 발사한 플레이어 ID (-1: 환경 공격)
} Arrow;

// 레드존 (RedZone)
typedef struct {
    int x, y;
    int width, height;
    int active;
    int lifetime;
} RedZone;

// 플레이어 (Player)
typedef struct {
    // 위치 및 기본 정보
    int id;
    int x, y;
    int connected;
    int score;
    
    // 상태 정보 (Status)
    int lives;
    int damage_cooldown;    // 피격 무적 시간
    
    // 아이템 보유 현황
    int invincible_item;
    int heal_item;
    int slow_item;
    
    // 아이템 효과 상태
    int is_invincible;      // 무적 상태 여부
    int invincible_frames;  // 무적 지속 프레임
    int is_slow;            // 감속 상태 여부
    int slow_frames;        // 감속 지속 프레임
} Player;

// =========================================================
// [5] 전체 게임 상태 구조체 (Game State)
// =========================================================

// 서버와 클라이언트가 공유하는 전체 게임 월드 데이터
typedef struct {
    Arrow arrow[MAX_ARROWS];
    RedZone redzone[MAX_REDZONES];
    Player player[MAX_PLAYERS];
    int frame;
    int special_wave;
    bool multiplay;
} GameState;

// =========================================================
// [6] 네트워크 패킷 구조체 (Network Packet)
// =========================================================

// 데이터 통신용 패킷
// (GameState 및 모든 객체 구조체에 의존하므로 가장 아래에 배치)
typedef struct {
    PacketType type;
    int id;
    
    // 개별 업데이트용 필드
    int x, y;
    int item_type; // PACKET_ITEM_USE 시 사용
    
    // 대규모 데이터 동기화용 필드
    Arrow arrows[MAX_ARROWS];
    RedZone redzones[MAX_REDZONES];
    Player player;    
    GameState game_state; 
} Packet;

// =========================================================
// [7] 기타 유틸리티 구조체
// =========================================================

// 점수 저장용
typedef struct {
    char name[20];
    int score;
    char mode[10]; // "SINGLE" or "MULTI"
    time_t timestamp;
} ScoreEntry;

#endif // COMMON_H