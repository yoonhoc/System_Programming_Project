#ifndef GAME_STRUCTS_H
#define GAME_STRUCTS_H

// --- 상수 정의 (서버/클라이언트 공통) ---
#define MAX_PLAYERS 2
#define MAX_ARROWS 50
#define MAX_REDZONES 10
#define PORT 3000  // 포트 번호 설정

// --- 기본 게임 객체 구조체 ---
typedef struct {
    int x, y;
    int dx, dy;
    char symbol;
    int active;
    int is_special;
} Arrow;

typedef struct {
    int x, y;
    int width, height;
    int active;
    int lifetime;
} RedZone;

typedef struct {
    int lives;
    int invincible_item;
    int heal_item;
    int slow_item;
    int is_invincible;
    int invincible_frames;
    int is_slow;
    int slow_frames;
    int damage_cooldown;
} PlayerStatus;

// --- 멀티플레이용 추가 구조체 ---

// 플레이어 전체 정보 (위치 + 상태 + 네트워크 정보)
typedef struct {
    int player_id;
    int x, y;
    int connected;
    int score;
    PlayerStatus status;
} Player;

// 게임 전체 상태 (서버에서 관리)
typedef struct {
    Player players[MAX_PLAYERS];
    Arrow arrows[MAX_ARROWS];
    RedZone redzones[MAX_REDZONES];
    long frame;
    int special_wave_frame;
} GameState;

// --- 네트워크 패킷 정의 ---

// 패킷 타입 열거형
typedef enum {
    PACKET_INITIAL_STATE,
    PACKET_PLAYER_MOVE,
    PACKET_ITEM_USE,
    PACKET_ARROW_UPDATE,
    PACKET_REDZONE_UPDATE,
    PACKET_PLAYER_STATUS,
    PACKET_GAME_OVER
} PacketType;

// 패킷 데이터 구조체
typedef struct {
    PacketType type;
    int player_id;
    union {
        struct {
            GameState game_state;
        } initial_state;
        
        struct {
            int x, y;
        } move;
        
        struct {
            int item_type; // 1: 무적, 2: 힐, 3: 슬로우
        } item_use;
        
        struct {
            Arrow arrows[MAX_ARROWS];
        } arrows;
        
        struct {
            RedZone redzones[MAX_REDZONES];
        } redzones;
        
        struct {
            Player player;
        } player_status;
    } data;
} Packet;

#endif