#include "item.h"

void invincible_item(Player* player) {
    if (player->invincible_item > 0) {
        player->invincible_item--;
        player->invincible = 1;
        player->invincible_frames = 100;
    }
}

void heal_item(Player* player) {
    if (player->heal_item > 0 && player->lives < 3) {
        player->heal_item--;
        player->lives++;
    }
}

void slow_item(Player* player) {
    if (player->slow_item > 0) {
        player->slow_item--;
        player->slow = 1;
        player->slow_frames = 200;
    }
}

