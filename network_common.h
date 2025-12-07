#ifndef NETWORK_COMMON_H
#define NETWORK_COMMON_H

#define MAX_ARROWS 50
#define MAX_REDZONES 10
#define PORT 8888
#define MAX_PLAYERS 2

// 패킷 타입
typedef enum {
    PACKET_PLAYER_MOVE,
    PACKET_ARROW_UPDATE,
    PACKET_REDZONE_UPDATE,
    PACKET_PLAYER_STATUS,
    PACKET_GAME_OVER,
    PACKET_ITEM_USE,
    PACKET_INITIAL_STATE
} PacketType;

// 화살 구조체
typedef struct {
    int x, y;
    int dx, dy;
    char symbol;
    int active;
    int is_special;  // 0: 일반, 1: 특수 웨이브, 2: 플레이어 공격
    int owner;       // 공격을 발사한 플레이어 ID (-1: 환경 공격)
} Arrow;

// 레드존 구조체
typedef struct {
    int x, y;
    int width, height;
    int active;
    int lifetime;
} RedZone;

// 플레이어 상태 구조체
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

// 플레이어 정보
typedef struct {
    int x, y;
    int player_id;
    PlayerStatus status;
    int connected;
    int score;
} Player;

// 게임 상태 (서버에서 관리)
typedef struct {
    Arrow arrows[MAX_ARROWS];
    RedZone redzones[MAX_REDZONES];
    Player players[MAX_PLAYERS];
    int frame;
    int special_wave_frame;
} GameState;

// 네트워크 패킷
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

#endif