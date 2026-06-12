#ifndef GOS_GAME_H
#define GOS_GAME_H

#include "gfx/wm.h"
#include "drivers/keyboard.h"
#include "lib/math.h"

void game_init(int wid);
void game_update(int wid);
void game_draw(int wid);

#endif // GOS_GAME_H