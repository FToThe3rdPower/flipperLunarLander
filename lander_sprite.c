#include "lander_sprite.h"
#include <math.h>

/* Sprite layout — matches the reference PNG exactly (7×7 with 2-px feet).
 *
 *        ###       y=-3   body top (3 wide)
 *       #   #      y=-2   ┐ body sides (5 wide outline,
 *       #   #      y=-1   ┘   four corner pixels intentionally cut)
 *        ###       y= 0   body bottom (3 wide)
 *       #####      y=+1   platform (5 wide, where legs attach)
 *       #   #      y=+2   legs straight down at ±2
 *      ##   ##     y=+3   2-pixel feet at each corner
 *
 * No antenna. The "taper" is the four cut corner pixels of the body outline.
 * Foot tips (outermost pixels) sit at ±LANDER_FOOT_DX = ±3, so the collision
 * footprint in game.c is unchanged from earlier versions.
 */

void lander_draw_static(Canvas* canvas, int cx, int cy) {
    /* Body outline — rounded rectangle, four corner pixels left blank */
    canvas_draw_line(canvas, cx - 1, cy - 3, cx + 1, cy - 3);   // top
    canvas_draw_line(canvas, cx - 2, cy - 2, cx - 2, cy - 1);   // left side
    canvas_draw_line(canvas, cx + 2, cy - 2, cx + 2, cy - 1);   // right side
    canvas_draw_line(canvas, cx - 1, cy,     cx + 1, cy);       // bottom
    /* Platform — 5-wide horizontal under the body */
    canvas_draw_line(canvas, cx - 2, cy + 1, cx + 2, cy + 1);
    /* Legs — vertical from platform corners straight down to foot height */
    canvas_draw_line(canvas, cx - 2, cy + 1, cx - 2, cy + 3);
    canvas_draw_line(canvas, cx + 2, cy + 1, cx + 2, cy + 3);
    /* Feet — 2 pixels each, extending one pixel outward from each leg */
    canvas_draw_line(canvas, cx - 3, cy + 3, cx - 2, cy + 3);
    canvas_draw_line(canvas, cx + 2, cy + 3, cx + 3, cy + 3);
}

void lander_draw_rotated(Canvas* canvas, float cx, float cy, float angle, float thrust) {
    float s = sinf(angle);
    float c = cosf(angle);

#define RX(lx, ly) ((int)(cx + (lx) * c - (ly) * s))
#define RY(lx, ly) ((int)(cy + (lx) * s + (ly) * c))

    /* Body outline — four segments forming the rounded rectangle */
    canvas_draw_line(canvas, RX(-1, -3), RY(-1, -3), RX(+1, -3), RY(+1, -3));   // top
    canvas_draw_line(canvas, RX(-2, -2), RY(-2, -2), RX(-2, -1), RY(-2, -1));   // L side
    canvas_draw_line(canvas, RX(+2, -2), RY(+2, -2), RX(+2, -1), RY(+2, -1));   // R side
    canvas_draw_line(canvas, RX(-1,  0), RY(-1,  0), RX(+1,  0), RY(+1,  0));   // bottom

    /* Platform */
    canvas_draw_line(canvas,
        RX(-LANDER_HALF_W, LANDER_BODY_BOT), RY(-LANDER_HALF_W, LANDER_BODY_BOT),
        RX( LANDER_HALF_W, LANDER_BODY_BOT), RY( LANDER_HALF_W, LANDER_BODY_BOT));

    /* Vertical legs — platform corners straight down to foot height */
    canvas_draw_line(canvas,
        RX(-LANDER_HALF_W, LANDER_BODY_BOT), RY(-LANDER_HALF_W, LANDER_BODY_BOT),
        RX(-LANDER_HALF_W, LANDER_FOOT_DY),  RY(-LANDER_HALF_W, LANDER_FOOT_DY));
    canvas_draw_line(canvas,
        RX( LANDER_HALF_W, LANDER_BODY_BOT), RY( LANDER_HALF_W, LANDER_BODY_BOT),
        RX( LANDER_HALF_W, LANDER_FOOT_DY),  RY( LANDER_HALF_W, LANDER_FOOT_DY));

    /* 2-px feet — from leg base (HALF_W) outward to foot tip (FOOT_DX) */
    canvas_draw_line(canvas,
        RX(-LANDER_FOOT_DX, LANDER_FOOT_DY), RY(-LANDER_FOOT_DX, LANDER_FOOT_DY),
        RX(-LANDER_HALF_W,  LANDER_FOOT_DY), RY(-LANDER_HALF_W,  LANDER_FOOT_DY));
    canvas_draw_line(canvas,
        RX( LANDER_HALF_W,  LANDER_FOOT_DY), RY( LANDER_HALF_W,  LANDER_FOOT_DY),
        RX( LANDER_FOOT_DX, LANDER_FOOT_DY), RY( LANDER_FOOT_DX, LANDER_FOOT_DY));

    /* Thrust flame — inverted triangle from platform corners to a point
     * below. Attached at LANDER_BODY_BOT (= the platform). */
    if (thrust > 0.05f) {
        float flame_len = 2.0f + 4.0f * thrust;
        canvas_draw_line(canvas,
            RX(-1.5f, LANDER_BODY_BOT), RY(-1.5f, LANDER_BODY_BOT),
            RX(0, LANDER_BODY_BOT + flame_len), RY(0, LANDER_BODY_BOT + flame_len));
        canvas_draw_line(canvas,
            RX( 1.5f, LANDER_BODY_BOT), RY( 1.5f, LANDER_BODY_BOT),
            RX(0, LANDER_BODY_BOT + flame_len), RY(0, LANDER_BODY_BOT + flame_len));
    }

#undef RX
#undef RY
}
