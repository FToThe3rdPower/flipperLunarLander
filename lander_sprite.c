#include "lander_sprite.h"
#include <math.h>

/* Sprite layout — matches the reference PNG, with the corner-pixel feet
 * swapped for the v0.5 horizontal foot pads.
 *
 *        ###       y=-3   body top (3 wide)
 *       #   #      y=-2   body sides (5 wide outline,
 *       #   #      y=-1     four corner pixels intentionally cut)
 *        ###       y= 0   body bottom (3 wide)
 *       #####      y=+1   platform (5 wide, legs & flame attach)
 *       #   #      y=+2   legs splay out
 *      ### ###     y=+3   foot pads (3 px each, ±1 around the foot tip)
 *
 * No antenna. The "taper" the body shows is the four cut corners — top and
 * bottom rows are 3 wide, middle rows are 5 wide, giving the body a slightly
 * rounded silhouette.
 */

void lander_draw_static(Canvas* canvas, int cx, int cy) {
    /* Body outline — rounded rectangle, 4 corner pixels left blank */
    canvas_draw_line(canvas, cx - 1, cy - 3, cx + 1, cy - 3);   // top
    canvas_draw_line(canvas, cx - 2, cy - 2, cx - 2, cy - 1);   // left side
    canvas_draw_line(canvas, cx + 2, cy - 2, cx + 2, cy - 1);   // right side
    canvas_draw_line(canvas, cx - 1, cy,     cx + 1, cy);       // bottom
    /* Platform — 5-wide horizontal below the body */
    canvas_draw_line(canvas, cx - 2, cy + 1, cx + 2, cy + 1);
    /* Legs — diagonal from platform corners out to foot tips */
    canvas_draw_line(canvas, cx - 2, cy + 1, cx - 3, cy + 3);
    canvas_draw_line(canvas, cx + 2, cy + 1, cx + 3, cy + 3);
    /* Foot pads — short horizontal segments centered on each foot tip */
    canvas_draw_line(canvas, cx - 4, cy + 3, cx - 2, cy + 3);
    canvas_draw_line(canvas, cx + 2, cy + 3, cx + 4, cy + 3);
}

void lander_draw_rotated(Canvas* canvas, float cx, float cy, float angle, float thrust) {
    float s = sinf(angle);
    float c = cosf(angle);

#define RX(lx, ly) ((int)(cx + (lx) * c - (ly) * s))
#define RY(lx, ly) ((int)(cy + (lx) * s + (ly) * c))

    /* Body outline — same 4 segments as the static version, rotated. The cut
     * corners stay visually cut at small angles; at extreme tilts the
     * Bresenham pixel rounding gets approximate, which is fine at this scale. */
    canvas_draw_line(canvas, RX(-1, -3), RY(-1, -3), RX(+1, -3), RY(+1, -3));   // top
    canvas_draw_line(canvas, RX(-2, -2), RY(-2, -2), RX(-2, -1), RY(-2, -1));   // L side
    canvas_draw_line(canvas, RX(+2, -2), RY(+2, -2), RX(+2, -1), RY(+2, -1));   // R side
    canvas_draw_line(canvas, RX(-1,  0), RY(-1,  0), RX(+1,  0), RY(+1,  0));   // bottom

    /* Platform */
    canvas_draw_line(canvas,
        RX(-LANDER_HALF_W, LANDER_BODY_BOT), RY(-LANDER_HALF_W, LANDER_BODY_BOT),
        RX( LANDER_HALF_W, LANDER_BODY_BOT), RY( LANDER_HALF_W, LANDER_BODY_BOT));

    /* Legs — platform corners to foot tips */
    canvas_draw_line(canvas,
        RX(-LANDER_HALF_W, LANDER_BODY_BOT), RY(-LANDER_HALF_W, LANDER_BODY_BOT),
        RX(-LANDER_FOOT_DX, LANDER_FOOT_DY), RY(-LANDER_FOOT_DX, LANDER_FOOT_DY));
    canvas_draw_line(canvas,
        RX( LANDER_HALF_W, LANDER_BODY_BOT), RY( LANDER_HALF_W, LANDER_BODY_BOT),
        RX( LANDER_FOOT_DX, LANDER_FOOT_DY), RY( LANDER_FOOT_DX, LANDER_FOOT_DY));

    /* Foot pads — short horizontal segments centered on each foot tip */
    canvas_draw_line(canvas,
        RX(-LANDER_FOOT_DX - 1.0f, LANDER_FOOT_DY),
        RY(-LANDER_FOOT_DX - 1.0f, LANDER_FOOT_DY),
        RX(-LANDER_FOOT_DX + 1.0f, LANDER_FOOT_DY),
        RY(-LANDER_FOOT_DX + 1.0f, LANDER_FOOT_DY));
    canvas_draw_line(canvas,
        RX( LANDER_FOOT_DX - 1.0f, LANDER_FOOT_DY),
        RY( LANDER_FOOT_DX - 1.0f, LANDER_FOOT_DY),
        RX( LANDER_FOOT_DX + 1.0f, LANDER_FOOT_DY),
        RY( LANDER_FOOT_DX + 1.0f, LANDER_FOOT_DY));

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
