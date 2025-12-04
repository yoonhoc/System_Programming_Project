#include "item.h"

void use_invincible_item(PlayerStatus* status) {
    if (status->invincible_item > 0) {
        status->invincible_item--;
        status->is_invincible = 1;
        status->invincible_frames = 100;
    }
}

void use_heal_item(PlayerStatus* status) {
    if (status->heal_item > 0 && status->lives < 3) {
        status->heal_item--;
        status->lives++;
    }
}

void use_slow_item(PlayerStatus* status) {
    if (status->slow_item > 0) {
        status->slow_item--;
        status->is_slow = 1;
        status->slow_frames = 200;
    }
}

void take_damage(PlayerStatus* status) {
    if (status->is_invincible || status->damage_cooldown > 0) {
        return;
    }
    status->lives--;
    status->damage_cooldown = 40;
}