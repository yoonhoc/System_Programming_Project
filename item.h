#ifndef ITEM_H
#define ITEM_H

#include "network_common.h"  // game_structs.h → network_common.h로 변경

void use_invincible_item(PlayerStatus* status);
void use_heal_item(PlayerStatus* status);
void use_slow_item(PlayerStatus* status);
void take_damage(PlayerStatus* status);

#endif