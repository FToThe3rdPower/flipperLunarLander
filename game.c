#include "game.h"
#include "lander_sprite.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <gui/canvas.h>
#include <furi_hal_speaker.h>
#include <furi_hal_vibro.h>

/* ----- Tunables (all in pixels & seconds) -------------------------------- */

/* NOTE: lander geometry (LANDER_HALF_W, LANDER_BODY_TOP, LANDER_BODY_BOT,
 * LANDER_FOOT_DX, LANDER_FOOT_DY) lives in
 * lander_sprite.h — single source of truth shared with the menu. The
 * collision math below uses LANDER_FOOT_DX / LANDER_FOOT_DY from that
 * header. */

#define PAD_W              16      // 2x lander total width
#define TERRAIN_TOP_Y      26      // highest peak (smallest y) after normalization
#define TERRAIN_BOT_Y      (SCREEN_H - 1)  // deepest valley = bottom row of screen
#define DESPIKE_HEIGHT     7       // min height (px) for a feature to count as a spike
#define DESPIKE_NEAR_TOL   5       // px tolerance for "neighbor is part of the peak"

#define HIGHEST_LEVEL      10      // bump this when adding levels; scoring depends on it
/* Distance thresholds (from spawn center) for multiplier tiers 1x/2x/3x/5x */
#define MUL_1X_THRESH      8              // basically right below spawn
#define MUL_2X_THRESH      (SCREEN_W / 5) // ~25 px
#define MUL_3X_THRESH      (SCREEN_W / 3) // ~42 px — beyond this → 5x

#define GRAVITY            6.0f    // pixels/sec^2 downward
#define THRUST_MAX         18.0f   // pixels/sec^2 along lander up-axis at full thrust
#define IMPULSE_DV         5.0f    // velocity change per tap (TapImpulse mode)
#define IMPULSE_FUEL       2.5f
#define RAMP_TIME          0.7f    // seconds from 0 -> full thrust in Ramp mode was 0.7
#define FUEL_BURN_RATE     12.0f   // units/sec at full thrust
#define ROT_RATE           1.8f    // radians/sec while Left/Right held
#define WRAP_X             1       // wrap horizontally (classic)

#define SAFE_VY            8.0f
#define SAFE_VX            4.0f
#define SAFE_ANGLE         0.22f   // ~12.6 degrees

/* VGM tilt control parameters */
#define TILT_STEER_DEAD    5.0f    // roll dead-zone (degrees) before steering starts
#define TILT_STEER_MAX    45.0f    // roll degrees that yield full ROT_RATE
#define TILT_THRUST_DEAD   5.0f    // pitch dead-zone (degrees) before thrust starts
#define TILT_THRUST_MAX   75.0f    // pitch degrees that yield 100% thrust

#define START_FUEL         100.0f

/* ----- Audio / feedback tunables ----------------------------------------- */
#define AUDIO_VOLUME       1.0f    // 0.0 to 1.0
#define THRUST_FREQ_MIN    220      // Hz at near-zero thrust
#define THRUST_FREQ_MAX    440     // Hz at full thrust
#define SFX_LAND_FREQ      440     // Hz
#define SFX_LAND_DUR       0.25f   // sec
#define SFX_CRASH_FREQ     100      // Hz
#define SFX_CRASH_DUR      0.50f   // sec
#define SFX_TAP_FREQ       220     // Hz
#define SFX_TAP_DUR        0.06f   // sec
#define FLASH_DURATION     1.6f    // sec - crash inversion flashing

/* ----- RNG (xorshift32, seeded from level) ------------------------------- */

static uint32_t game_rand(GameState* g) {
    uint32_t x = g->rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g->rng_state = x ? x : 0xDEADBEEFu;
    return g->rng_state;
}

static int rand_range(GameState* g, int lo, int hi) {
    if (hi <= lo) return lo;
    return lo + (int)(game_rand(g) % (uint32_t)(hi - lo + 1));
}

/* ----- Terrain ----------------------------------------------------------- */

static int clamp_int(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void terrain_generate(GameState* g) {
    /* Random walk with mild step size, then a single smoothing pass, then
     * linearly stretched so the lowest point sits at TERRAIN_BOT_Y (the very
     * bottom row of the screen) and the highest peak at TERRAIN_TOP_Y. The
     * normalization guarantees consistent vertical range regardless of how
     * the walk happened to drift. */

    int y = rand_range(g, 30, 60);
    for (int x = 0; x < SCREEN_W; x++) {
        int step = rand_range(g, -3, 3);
        y = clamp_int(y + step, 0, 200);
        g->terrain[x] = (uint8_t)y;
    }

    /* Light smoothing - just one pass to keep local variation visible. */
    for (int x = 1; x < SCREEN_W - 1; x++) {
        g->terrain[x] = (uint8_t)(
            (g->terrain[x - 1] + g->terrain[x] + g->terrain[x + 1]) / 3);
    }

    /* Normalize to [TERRAIN_TOP_Y, TERRAIN_BOT_Y]. */
    int min_y = 255, max_y = 0;
    for (int x = 0; x < SCREEN_W; x++) {
        if (g->terrain[x] < min_y) min_y = g->terrain[x];
        if (g->terrain[x] > max_y) max_y = g->terrain[x];
    }
    int src_range = max_y - min_y;
    if (src_range < 1) src_range = 1;
    int dst_range = TERRAIN_BOT_Y - TERRAIN_TOP_Y;
    for (int x = 0; x < SCREEN_W; x++) {
        int v = TERRAIN_TOP_Y + ((int)g->terrain[x] - min_y) * dst_range / src_range;
        g->terrain[x] = (uint8_t)v;
    }
}

/* Flatten peaks that are taller than DESPIKE_HEIGHT pixels and thinner than 3
 * pixels wide. Operates on already-normalized terrain so thresholds are in
 * screen-pixel units. A column is a "spike" when terrain[x] is at least
 * DESPIKE_HEIGHT smaller than both terrain[x-2] and terrain[x+2], AND at most
 * one immediate neighbor is within DESPIKE_NEAR_TOL of terrain[x] — i.e. the
 * feature width is 1 or 2. Doesn't change min(terrain) so the bottom-of-screen
 * guarantee from normalization is preserved (we only flatten peaks, never
 * valleys). */
static void terrain_despike(GameState* g) {
    uint8_t out[SCREEN_W];
    memcpy(out, g->terrain, SCREEN_W);

    for (int x = 2; x < SCREEN_W - 2; x++) {
        int t = (int)g->terrain[x];
        int dl = (int)g->terrain[x - 2] - t;
        int dr = (int)g->terrain[x + 2] - t;
        if (dl > DESPIKE_HEIGHT && dr > DESPIKE_HEIGHT) {
            int near = 0;
            if (abs((int)g->terrain[x - 1] - t) <= DESPIKE_NEAR_TOL) near++;
            if (abs((int)g->terrain[x + 1] - t) <= DESPIKE_NEAR_TOL) near++;
            if (near <= 1) {
                out[x] = (uint8_t)(((int)g->terrain[x - 2] + (int)g->terrain[x + 2]) / 2);
            }
        }
    }

    memcpy(g->terrain, out, SCREEN_W);
}

int game_pads_for_level(int level) {
    if (level <= 3)  return 5;
    if (level <= 6)  return 4;
    if (level <= 8)  return 3;
    if (level == 9)  return 2;
    return 1;
}

static void pads_place(GameState* g) {
    g->num_pads = (uint8_t)game_pads_for_level(g->level);

    /* Divide the playable strip into num_pads regions and place one pad per
     * region with random horizontal jitter. Guarantees no overlap. */
    int margin = 6;
    int usable = SCREEN_W - 2 * margin;
    int region_w = usable / g->num_pads;

    for (int i = 0; i < g->num_pads; i++) {
        int region_start = margin + i * region_w;
        int max_offset = region_w - PAD_W;
        int offset = (max_offset > 0) ? rand_range(g, 0, max_offset) : 0;
        int px = region_start + offset;
        if (px < 0) px = 0;
        if (px + PAD_W > SCREEN_W) px = SCREEN_W - PAD_W;

        /* Flatten terrain across the pad. Use the lowest (largest-y) value
         * under the pad span so the pad sits flush with surrounding ground. */
        int flat_y = g->terrain[px];
        for (int dx = 1; dx < PAD_W; dx++) {
            int t = g->terrain[px + dx];
            if (t > flat_y) flat_y = t;
        }
        for (int dx = 0; dx < PAD_W; dx++) {
            g->terrain[px + dx] = (uint8_t)flat_y;
        }
        g->pad_x[i] = (uint8_t)px;
    }

    /* Assign multipliers based on distance of each pad's center from the
     * lander spawn (screen center). Farther = higher reward. */
    int spawn_cx = SCREEN_W / 2;
    for (int i = 0; i < g->num_pads; i++) {
        int pad_cx = g->pad_x[i] + PAD_W / 2;
        int dist = abs(pad_cx - spawn_cx);
        uint8_t mul;
        if      (dist <= MUL_1X_THRESH) mul = 1;
        else if (dist <= MUL_2X_THRESH) mul = 2;
        else if (dist <= MUL_3X_THRESH) mul = 3;
        else                            mul = 5;
        g->pad_mul[i] = mul;
    }
}

/* ----- Init -------------------------------------------------------------- */

void game_init(GameState* g, int level, int score, FuelMode fuel_mode, int starting_fuel) {
    memset(g, 0, sizeof(*g));
    g->level = level;
    g->score = score;
    g->fuel_mode = fuel_mode;
    /* Seed depends on level. Mixing constants keep adjacent levels visually
     * different rather than near-identical. */
    g->rng_state = 0xA5C3F00Du ^ ((uint32_t)level * 0x9E3779B1u);

    terrain_generate(g);
    terrain_despike(g);
    pads_place(g);

    g->x = (float)SCREEN_W / 2.0f;
    g->y = 6.0f;
    /* Random initial Vx in [-5, +5] pixels/sec. Interpreting "±5" as a range;
     * if you wanted strictly ±5 (one or the other, never in between), the
     * fix is `g->vx = rand_range(g, 0, 1) ? 5.0f : -5.0f;`. */
    g->vx = (float)rand_range(g, -5, 5);
    g->vy = 0.0f;
    g->angle = 0.0f;
    g->fuel = (float)starting_fuel;
    g->fuel_at_level_start = (float)starting_fuel;
    g->status = GameStatusFlying;
    g->status_time = 0.0f;
    g->elapsed = 0.0f;
    g->up_hold_time = 0.0f;
    g->current_thrust = 0.0f;
    g->needs_tilt_cal = true;
    g->tilt_pitch_offset = 0.0f;
    g->tilt_roll_offset  = 0.0f;
}

/* ----- Input ------------------------------------------------------------- */

GameAction game_input(GameState* g, const InputEvent* ev) {
    /* Back from a status screen, or from flying, returns to menu. */
    if (ev->key == InputKeyBack &&
        (ev->type == InputTypeShort || ev->type == InputTypeLong)) {
        return GameActionExitToMenu;
    }

    /* Track held state for Left/Right/Up. The Flipper sends Press + Release
     * for every keypress, plus Short/Long/Repeat in between. Held state lives
     * between Press and Release. */
    if (ev->type == InputTypePress) {
        switch (ev->key) {
            case InputKeyLeft:  g->left_held = true; break;
            case InputKeyRight: g->right_held = true; break;
            case InputKeyUp:
                g->up_held = true;
                g->up_hold_time = 0.0f;
                break;
            default: break;
        }
    } else if (ev->type == InputTypeRelease) {
        switch (ev->key) {
            case InputKeyLeft:  g->left_held = false; break;
            case InputKeyRight: g->right_held = false; break;
            case InputKeyUp:    g->up_held = false; break;
            default: break;
        }
    }

    /* On status screens, OK retries (crash) or advances (landed).
     * Score carries forward across levels and across retries.
     * Fuel behavior depends on fuel_mode:
     *   - FuelModeFull: every level starts with START_FUEL (classic).
     *   - No-refuel:    advance carries remaining fuel forward;
     *                   retry rewinds to fuel_at_level_start. */
    if (ev->type == InputTypeShort && ev->key == InputKeyOk) {
        if (g->status == GameStatusLanded) {
            int next = g->level + 1;
            if (next > HIGHEST_LEVEL) next = 1;  // wrap; later: "You win!" screen
            /* Capture campaign state before memset wipes it. */
            FuelMode mode = g->fuel_mode;
            int score = g->score;
            int next_fuel = (mode == FuelModeFull) ? (int)START_FUEL : (int)g->fuel;
            game_init(g, next, score, mode, next_fuel);
        } else if (g->status == GameStatusCrashed || g->status == GameStatusOutOfFuel) {
            FuelMode mode = g->fuel_mode;
            int score = g->score;
            int level = g->level;
            int retry_fuel =
                (mode == FuelModeFull) ? (int)START_FUEL : (int)g->fuel_at_level_start;
            game_init(g, level, score, mode, retry_fuel);
        }
    }

    return GameActionNone;
}

/* ----- Physics ----------------------------------------------------------- */

/* Returns the index of the pad that fully contains [x_left, x_right], or -1. */
static int on_pad_idx(const GameState* g, int x_left, int x_right) {
    for (int i = 0; i < g->num_pads; i++) {
        int px = g->pad_x[i];
        if (x_left >= px && x_right <= px + PAD_W - 1) return i;
    }
    return -1;
}

static void check_collision(GameState* g) {
    /* Rotate both foot positions into world space. */
    float s = sinf(g->angle);
    float c = cosf(g->angle);

    /* Local foot coords: (-LANDER_FOOT_DX, LANDER_FOOT_DY) and (+LANDER_FOOT_DX, LANDER_FOOT_DY) */
    float lfx = g->x + (-LANDER_FOOT_DX) * c - LANDER_FOOT_DY * s;
    float lfy = g->y + (-LANDER_FOOT_DX) * s + LANDER_FOOT_DY * c;
    float rfx = g->x +  LANDER_FOOT_DX  * c - LANDER_FOOT_DY * s;
    float rfy = g->y +  LANDER_FOOT_DX  * s + LANDER_FOOT_DY * c;

    int lxi = clamp_int((int)lfx, 0, SCREEN_W - 1);
    int rxi = clamp_int((int)rfx, 0, SCREEN_W - 1);

    bool left_contact  = lfy >= (float)g->terrain[lxi];
    bool right_contact = rfy >= (float)g->terrain[rxi];

    if (!left_contact && !right_contact) return;

    /* Something touched. Decide outcome.
     * We don't require BOTH feet to be in contact in the same frame —
     * with sub-pixel motion and any tilt, one foot always touches first.
     * The `upright` check already enforces that the lander is level enough. */
    int xmin = lxi < rxi ? lxi : rxi;
    int xmax = lxi > rxi ? lxi : rxi;
    int pad_idx = on_pad_idx(g, xmin, xmax);
    bool on_flat = (pad_idx >= 0);
    bool slow_enough = (fabsf(g->vy) < SAFE_VY) && (fabsf(g->vx) < SAFE_VX);
    bool upright = fabsf(g->angle) < SAFE_ANGLE;

    if (on_flat && slow_enough && upright) {
        g->status = GameStatusLanded;
        /* score = fuel_left * multiplier * (HIGHEST_LEVEL - level + 1) */
        int mult = (int)g->pad_mul[pad_idx];
        int level_bonus = HIGHEST_LEVEL - g->level + 1;
        if (level_bonus < 1) level_bonus = 1;  // safety if user plays beyond HIGHEST_LEVEL
        g->score += (int)g->fuel * mult * level_bonus;
        g->sfx_remaining = SFX_LAND_DUR;
        g->sfx_freq = SFX_LAND_FREQ;
        g->sfx_vibrate = false;
    } else {
        g->status = GameStatusCrashed;
        g->sfx_remaining = SFX_CRASH_DUR;
        g->sfx_freq = SFX_CRASH_FREQ;
        g->sfx_vibrate = true;
    }
    g->status_time = 0.0f;
    g->vx = g->vy = 0.0f;
}

void game_tick(GameState* g, ThrustMode mode, float dt) {
    g->status_time += dt;

    /* SFX timer runs regardless of game status so crash/land SFX can play out. */
    if (g->sfx_remaining > 0.0f) {
        g->sfx_remaining -= dt;
        if (g->sfx_remaining < 0.0f) g->sfx_remaining = 0.0f;
    }

    if (g->status != GameStatusFlying) {
        return;
    }

    g->elapsed += dt;

    /* Rotation — buttons for non-VGM modes; direct angle mapping for VGM modes. */
    if(mode == ThrustModeVidyaTap    ||
       mode == ThrustModeVidyaBinary ||
       mode == ThrustModeVidyaRamp   ||
       mode == ThrustModeVidyaFull) {
        float cal_roll = g->tilt_roll - g->tilt_roll_offset;
        g->angle = -cal_roll * (3.14159265f / 180.0f);
    } else {
        if(g->left_held)  g->angle -= ROT_RATE * dt;
        if(g->right_held) g->angle += ROT_RATE * dt;
    }

    /* Thrust level (0..1) per mode. */
    g->current_thrust = 0.0f;
    if(mode == ThrustModeBinary || mode == ThrustModeVidyaBinary) {
        g->current_thrust = g->up_held ? 1.0f : 0.0f;
    } else if(mode == ThrustModeRamp || mode == ThrustModeVidyaRamp) {
        if(g->up_held) {
            g->up_hold_time += dt;
            float t = g->up_hold_time / RAMP_TIME;
            if(t > 1.0f) t = 1.0f;
            g->current_thrust = t;
        } else {
            g->up_hold_time = 0.0f;
            g->current_thrust = 0.0f;
        }
    } else if(mode == ThrustModeVidyaFull) {
        /* Calibrated pitch drives thrust: dead-zone → 0, TILT_THRUST_MAX → 100%. */
        float cal_pitch = g->tilt_pitch - g->tilt_pitch_offset;
        float t = (cal_pitch - TILT_THRUST_DEAD) / (TILT_THRUST_MAX - TILT_THRUST_DEAD);
        if(t < 0.0f) t = 0.0f;
        if(t > 1.0f) t = 1.0f;
        g->current_thrust = t;
    }
    /* TapImpulse and VidyaTap handled by game_input on the press event. */

    /* Apply thrust acceleration along lander up-axis (-y when angle=0). */
    if (g->current_thrust > 0.0f && g->fuel > 0.0f) {
        float a = THRUST_MAX * g->current_thrust;
        g->vx += sinf(g->angle) * a * dt;
        g->vy += -cosf(g->angle) * a * dt;
        g->fuel -= FUEL_BURN_RATE * g->current_thrust * dt;
        if (g->fuel < 0.0f) g->fuel = 0.0f;
    }

    /* Gravity */
    g->vy += GRAVITY * dt;

    /* Integrate */
    g->x += g->vx * dt;
    g->y += g->vy * dt;

    /* Horizontal wrap */
    if (WRAP_X) {
        if (g->x < 0.0f) g->x += SCREEN_W;
        if (g->x >= SCREEN_W) g->x -= SCREEN_W;
    } else {
        if (g->x < 0.0f) { g->x = 0.0f; g->vx = 0.0f; }
        if (g->x >= SCREEN_W) { g->x = SCREEN_W - 1; g->vx = 0.0f; }
    }

    /* Out-of-fuel doesn't immediately end the game (you can still glide), but
     * if you've also got no upward chance, we'll let collision handle it. */
    if (g->fuel <= 0.0f && g->vy > 0.0f && g->y > SCREEN_H - 20) {
        /* Soft signal — let the crash detection determine outcome; this flag
         * is unused for now. */
    }

    /* Collision with terrain or ceiling */
    if (g->y < 0.0f) { g->y = 0.0f; if (g->vy < 0.0f) g->vy = 0.0f; }

    check_collision(g);
}

/* ----- TapImpulse handling -----------------------------------------------
 * Tap impulses fire instantly on press, separate from the per-tick thrust
 * level. The main app dispatches this via game_input_tap() below.
 * For modularity we handle it inside game_input() when the user presses Up
 * AND the mode is TapImpulse.
 */
void game_apply_tap_impulse(GameState* g) {
    if (g->status != GameStatusFlying || g->fuel <= 0.0f) return;
    g->vx += sinf(g->angle) * IMPULSE_DV;
    g->vy += -cosf(g->angle) * IMPULSE_DV;
    g->fuel -= IMPULSE_FUEL;
    if (g->fuel < 0.0f) g->fuel = 0.0f;
    g->sfx_remaining = SFX_TAP_DUR;
    g->sfx_freq = SFX_TAP_FREQ;
    g->sfx_vibrate = true;
}

/* ----- Drawing ----------------------------------------------------------- */

static void draw_terrain(Canvas* canvas, const GameState* g) {
    for (int x = 0; x < SCREEN_W - 1; x++) {
        canvas_draw_line(canvas, x, g->terrain[x], x + 1, g->terrain[x + 1]);
    }
    /* Pads:
     *   - 2 px thick (add an extra row of pixels just below the surface line)
     *   - multiplier label is drawn UNDER the pad when there's room; falls
     *     back to ABOVE the pad for pads near the bottom of the screen.
     * Reads g->pad_mul[i] so when we eventually vary multipliers per pad
     * (instead of all DEFAULT_PAD_MUL), nothing else has to change here. */
    canvas_set_font(canvas, FontSecondary);
    const int label_h = 7;          // approx FontSecondary height
    for (int i = 0; i < g->num_pads; i++) {
        int px = g->pad_x[i];
        int py = g->terrain[px];

        /* Second pixel row immediately below the pad surface */
        if (py + 1 < SCREEN_H) {
            canvas_draw_line(canvas, px, py + 1, px + PAD_W - 1, py + 1);
        }

        char buf[8];
        snprintf(buf, sizeof(buf), "%dx", (int)g->pad_mul[i]);

        /* Prefer below; fall back to above if it would clip. */
        int label_top_below = py + 3;                 // pad surface = py, +1 thickness, +1 gap
        int label_top_above = py - 2 - label_h;       // baseline 2 above pad, text grows up
        if (label_top_below + label_h <= SCREEN_H) {
            canvas_draw_str_aligned(
                canvas, px + PAD_W / 2, label_top_below, AlignCenter, AlignTop, buf);
        } else if (label_top_above >= 0) {
            canvas_draw_str_aligned(
                canvas, px + PAD_W / 2, py - 2, AlignCenter, AlignBottom, buf);
        }
        /* No label at all when both positions would clip; the thick pad
         * itself still marks the landing zone. */
    }
}

/* Thin wrapper around the shared sprite. Flame draws when current_thrust > 0
 * AND fuel remains AND we're still flying — gated here, not inside the
 * sprite, so the sprite stays a pure renderer. */
static void draw_lander(Canvas* canvas, const GameState* g) {
    float thrust_for_draw = 0.0f;
    if (g->fuel > 0.0f && g->status == GameStatusFlying) {
        thrust_for_draw = g->current_thrust;
    }
    lander_draw_rotated(canvas, g->x, g->y, g->angle, thrust_for_draw);
}

static void draw_hud(Canvas* canvas, const GameState* g) {
    canvas_set_font(canvas, FontSecondary);

    char buf[16];

    /* Top center: current level. Hidden while the lander is still high enough
     * to visually overlap with the top text row — appears once the lander
     * has fallen below the bottom of the top line of HUD text (~y=7).
     * Body top is at g->y + LANDER_BODY_TOP (=g->y - 3); the +1 margin keeps
     * it from popping in the moment the body grazes the text line. */
    if (g->y > 11.0f) {
        snprintf(buf, sizeof(buf), "L%d", g->level);
        canvas_draw_str_aligned(canvas, SCREEN_W / 2, 0, AlignCenter, AlignTop, buf);
    }

    /* Left column - AlignTop so successive lines stack predictably. */
    snprintf(buf, sizeof(buf), "S:%04d", g->score);
    canvas_draw_str_aligned(canvas, 0, 0, AlignLeft, AlignTop, buf);

    snprintf(buf, sizeof(buf), "T:%04d", (int)g->elapsed);
    canvas_draw_str_aligned(canvas, 0, 8, AlignLeft, AlignTop, buf);

    snprintf(buf, sizeof(buf), "F:%04d", (int)g->fuel);
    canvas_draw_str_aligned(canvas, 0, 16, AlignLeft, AlignTop, buf);

    /* Right column - %+4d keeps width stable as values change sign / magnitude. */
    int xi = clamp_int((int)g->x, 0, SCREEN_W - 1);
    int alt = (int)((float)g->terrain[xi] - g->y);
    if (alt < 0) alt = 0;

    snprintf(buf, sizeof(buf), "Alt:%3d", alt);
    canvas_draw_str_aligned(canvas, SCREEN_W, 0, AlignRight, AlignTop, buf);

    snprintf(buf, sizeof(buf), "Vx:%+4d", (int)g->vx);
    canvas_draw_str_aligned(canvas, SCREEN_W, 8, AlignRight, AlignTop, buf);

    snprintf(buf, sizeof(buf), "Vy:%+4d", (int)g->vy);
    canvas_draw_str_aligned(canvas, SCREEN_W, 16, AlignRight, AlignTop, buf);
}

static void draw_status_banner(Canvas* canvas, const GameState* g) {
    if (g->status == GameStatusFlying) return;
    /* Hold off the banner while the crash flash is still going. */
    if ((g->status == GameStatusCrashed || g->status == GameStatusOutOfFuel) &&
        g->status_time < FLASH_DURATION) return;

    const char* line1 = "";
    const char* line2 = "";
    switch (g->status) {
        case GameStatusLanded:
            line1 = "LANDED!";
            line2 = "OK: next  Back: menu";
            break;
        case GameStatusCrashed:
            line1 = "CRASHED";
            line2 = "OK: retry  Back: menu";
            break;
        case GameStatusOutOfFuel:
            line1 = "NO FUEL";
            line2 = "OK: retry  Back: menu";
            break;
        default: break;
    }

    /* Banner box, centered. */
    int bx = 14, by = 22, bw = SCREEN_W - 28, bh = 20;
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, bx, by, bw, bh);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rframe(canvas, bx, by, bw, bh, 2);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, SCREEN_W / 2, by + 8, AlignCenter, AlignCenter, line1);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, SCREEN_W / 2, by + 16, AlignCenter, AlignCenter, line2);
}

void game_draw(Canvas* canvas, const GameState* g) {
    draw_terrain(canvas, g);
    draw_lander(canvas, g);

    /* Crash flash: XOR-invert the scene (terrain + lander) on alternating
     * phases for FLASH_DURATION. FLASH_PHASE_HZ controls how often it flips —
     * lower number = slower flashing. 5 Hz = 200ms per phase. HUD draws on
     * top unaffected so the player can still read what's going on. */
    if ((g->status == GameStatusCrashed || g->status == GameStatusOutOfFuel) &&
        g->status_time < FLASH_DURATION) {
        const float FLASH_PHASE_HZ = 2.5f;
        int phase = (int)(g->status_time * FLASH_PHASE_HZ) % 2;
        if (phase == 1) {
            canvas_set_color(canvas, ColorXOR);
            canvas_draw_box(canvas, 0, 0, SCREEN_W, SCREEN_H);
            canvas_set_color(canvas, ColorBlack);
        }
    }

    draw_hud(canvas, g);

    if(g->vgm_missing) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas, SCREEN_W / 2, SCREEN_H / 2 - 4, AlignCenter, AlignCenter,
            "VGM not found");
    }

    draw_status_banner(canvas, g);
}

/* ----- Audio + vibration -------------------------------------------------
 * The speaker is acquired when entering the game screen and released on
 * leaving. While acquired, we set its frequency once per tick based on game
 * state. furi_hal_speaker_start() with a new freq while already playing
 * appears to smoothly change pitch on Flipper hardware — I haven't seen
 * clicking during ramp-mode sweeps, but tell me if you hear it.
 *
 * Vibration is on during continuous thrust (Binary/Ramp) and pulsed for
 * tap impulse and crashes. Landing intentionally has sound only.
 */

static bool      audio_acquired = false;
static uint16_t  audio_current_freq = 0;   // 0 = silent
static bool      audio_vibrating = false;

void game_audio_start(void) {
    if (audio_acquired) return;
    if (furi_hal_speaker_acquire(100)) {  // 100ms timeout; if taken, run silent
        audio_acquired = true;
    }
    audio_current_freq = 0;
    audio_vibrating = false;
}

void game_audio_stop(void) {
    if (audio_acquired) {
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
        audio_acquired = false;
    }
    audio_current_freq = 0;
    if (audio_vibrating) {
        furi_hal_vibro_on(false);
        audio_vibrating = false;
    }
}

static void audio_set_freq(uint16_t freq) {
    if (!audio_acquired) return;
    if (freq == audio_current_freq) return;
    if (freq == 0) {
        furi_hal_speaker_stop();
    } else {
        furi_hal_speaker_start((float)freq, AUDIO_VOLUME);
    }
    audio_current_freq = freq;
}

static void audio_set_vibro(bool on) {
    if (audio_vibrating == on) return;
    furi_hal_vibro_on(on);
    audio_vibrating = on;
}

void game_audio_update(const GameState* g, ThrustMode mode, bool sound_on, bool vibration_on) {
    bool continuous_thrust = (mode == ThrustModeBinary    ||
                              mode == ThrustModeRamp       ||
                              mode == ThrustModeVidyaBinary||
                              mode == ThrustModeVidyaRamp  ||
                              mode == ThrustModeVidyaFull);
    uint16_t target_freq = 0;
    bool target_vibro = false;

    if (g->sfx_remaining > 0.0f) {
        target_freq = g->sfx_freq;
        target_vibro = g->sfx_vibrate;
    } else if (continuous_thrust &&
               g->current_thrust > 0.05f &&
               g->fuel > 0.0f &&
               g->status == GameStatusFlying) {
        target_freq = (uint16_t)(THRUST_FREQ_MIN +
                                 g->current_thrust * (THRUST_FREQ_MAX - THRUST_FREQ_MIN));
        target_vibro = true;
    }

    /* Settings gates — applied after target computation so the rest of the
     * logic stays unchanged. Hits both continuous thrust and one-shot SFX. */
    if (!sound_on)     target_freq = 0;
    if (!vibration_on) target_vibro = false;

    audio_set_freq(target_freq);
    audio_set_vibro(target_vibro);
}
