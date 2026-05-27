#pragma once

#include "gfx/ui/wm.h"
#include "drivers/keyboard/keyboard.h"
#include "lib/math.h"

void game_init();
void keyboard_update_game();
void game_update(int wid);