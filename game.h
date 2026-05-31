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

    /* VGM tilt state — written by the main loop before each game_tick call.
     * pitch: 0=normal hold (up-button up), +90≈flat/screen-up.
     * roll:  0=level, positive=tilt right. */
    float tilt_pitch;
    float tilt_roll;
    float tilt_pitch_offset;  // calibration zero captured at level start
    float tilt_roll_offset;   // calibration zero captured at level start
    bool  needs_tilt_cal;     // true until the first IMU reading is captured
    bool  vgm_missing;        // VGM mode selected but IMU not detected

    /* Level data */
    uint32_t rng_state;
    uint8_t terrain[SCREEN_W];     // y of ground at each column
    uint8_t pad_x[MAX_PADS];       // left edge of each pad
    uint8_t pad_w[MAX_PADS];       // width of each pad in pixels
    uint8_t pad_mul[MAX_PADS];     // score multiplier per pad
    uint8_t num_pads;

    /* Status */
    GameStatus status;
    float status_time;    // seconds since status changed
    float land_vx;        // vx captured at moment of landing/crash
    float land_vy;        // vy captured at moment of landing/crash
    float land_angle;     // angle (radians) captured at moment of landing/crash

    /* Fuel campaign state. fuel_mode is captured at game start and used by the
     * advance/retry logic in game_input(). fuel_at_level_start is the value of
     * `fuel` at the moment this level was entered — for no-refuel modes, a
     * retry rewinds to this rather than to a clean tank. */
    FuelMode fuel_mode;
    float    fuel_at_level_start;

    /* Tutorial mode. gravity_scale=0.5 on tutorial level 1, 1.0 otherwise. */
    bool  is_tutorial;
    float gravity_scale;

    /* Sound effects (one-shot, drives both speaker and vibro briefly) */
    float sfx_remaining;  // seconds left on the current SFX, 0 = none
    uint16_t sfx_freq;    // Hz
    bool sfx_vibrate;     // whether this SFX also pulses the vibro motor
} GameState;

/* Initialize game state for the given level.
 *   score:         carry-forward score (pass 0 for a fresh game).
 *   fuel_mode:     campaign rules; stored in the state for retry/advance logic.
 *   starting_fuel: fuel to put in the tank for this level. The caller computes
 *                  this — typically fuel_mode_starting[mode] on a fresh start,
 *                  the previous level's remaining fuel on advance (no-refuel),
 *                  or fuel_at_level_start on retry. */
void game_init(GameState* g, int level, int score, FuelMode fuel_mode, int starting_fuel);
/* Tutorial variant: flat terrain, single centred pad, half-gravity on level 1. */
void game_init_tutorial(GameState* g, int tut_level, int score);
GameAction game_input(GameState* g, const InputEvent* ev);
void game_tick(GameState* g, ThrustMode mode, float dt);
void game_draw(Canvas* canvas, const GameState* g);
void game_draw_tutorial_popup(Canvas* canvas, int tut_level, ThrustMode thrust_mode);

/* Audio control. start/stop on entering/leaving the game screen; update once
 * per tick. start may quietly fail if the speaker is in use by another app —
 * in that case the game continues silently. */
void game_audio_start(void);
void game_audio_stop(void);
void game_audio_update(const GameState* g, ThrustMode mode, bool sound_on, bool vibration_on);

/* Pad count for a given level number (1-indexed). */
int game_pads_for_level(int level);

/* Called by main when Up is pressed in TapImpulse mode. */
void game_apply_tap_impulse(GameState* g);
