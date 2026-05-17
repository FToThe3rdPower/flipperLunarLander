#pragma once

#include <gui/gui.h>

/* Lander geometry — these constants are the source of truth for the lander's
 * visual *and* collision footprint. If you tweak the sprite, the collision
 * math in game.c is keyed off LANDER_FOOT_DX / LANDER_FOOT_DY. */
#define LANDER_HALF_W       2.0f    // body half-width
#define LANDER_BODY_TOP    -3.0f    // y of body top (above center)
#define LANDER_BODY_BOT     0.0f    // y of body bottom (where flame attaches)
#define LANDER_ANTENNA_TOP -5.0f    // y of antenna tip
#define LANDER_FOOT_DX      3.0f    // foot tip x offset from center
#define LANDER_FOOT_DY      3.0f    // foot tip y offset from center

/* Draw the lander upright (no rotation) centered at integer pixel (cx, cy).
 * Fast path used by the menu and info screen. */
void lander_draw_static(Canvas* canvas, int cx, int cy);

/* Draw the lander rotated by `angle` (radians, 0 = upright, +ve = clockwise)
 * with the body center at float (cx, cy). If `thrust` > 0.05, a flame is
 * drawn from the engine, scaled by thrust intensity in [0, 1]. */
void lander_draw_rotated(Canvas* canvas, float cx, float cy, float angle, float thrust);
