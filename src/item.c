#include "item.h"

void use_invincible_item(Player* player) {
    if (player->invincible_item > 0) {
        player->invincible_item--;
        player->is_invincible = 1;
        player->invincible_frames = 100;
    }
}

void use_heal_item(Player* player) {
    if (player->heal_item > 0 && player->lives < 3) {
        player->heal_item--;
        player->lives++;
    }
}

void use_slow_item(Player* player) {
    if (player->slow_item > 0) {
        player->slow_item--;
        player->is_slow = 1;
        player->slow_frames = 200;
    }
}

