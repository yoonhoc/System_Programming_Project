#ifndef ITEM_H
#define ITEM_H

#include "game_structs.h"

void use_invincible_item(PlayerStatus* status);
void use_heal_item(PlayerStatus* status);
void use_slow_item(PlayerStatus* status);
void take_damage(PlayerStatus* status);

#endif