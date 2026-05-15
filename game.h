#pragma once

#include <gui/gui.h>
#include <input/input.h>
#include "lunar_lander.h"

#define MAX_PADS 5

typedef enum {
    GameStatusFlying = 0,
    GameStatusLanded,
    GameStatusCrashed,
    GameStatusOutOfFuel,  // separate from crashed so we can show a different banner
} GameStatus;

typedef enum {
    GameActionNone = 0,
    GameActionExitToMenu,
} GameAction;

typedef struct {
    /* Progress */
    int level;            // 1..10
    int score;
    float elapsed;        // seconds since level start

    /* Lander state */
    float x, y;           // pixel coords, y+ = down
    float vx, vy;         // pixels/sec
    float angle;          // radians, 0 = up; +ve = clockwise
    float fuel;           // 0..100

    /* Input state */
    bool left_held;
    bool right_held;
    bool up_held;
    float up_hold_time;   // seconds (used by Ramp mode)
    float current_thrust; // 0..1, derived from mode + input each tick

    /* Level data */
    uint32_t rng_state;
    uint8_t terrain[SCREEN_W];     // y of ground at each column
    uint8_t pad_x[MAX_PADS];       // left edge of each pad
    uint8_t num_pads;

    /* Status */
    GameStatus status;
    float status_time;    // seconds since status changed
} GameState;

void game_init(GameState* g, int level);
GameAction game_input(GameState* g, const InputEvent* ev);
void game_tick(GameState* g, ThrustMode mode, float dt);
void game_draw(Canvas* canvas, const GameState* g);

/* Pad count for a given level number (1-indexed). */
int game_pads_for_level(int level);

/* Called by main when Up is pressed in TapImpulse mode. */
void game_apply_tap_impulse(GameState* g);
