#include "lander_sprite.h"
#include <math.h>

void lander_draw_static(Canvas* canvas, int cx, int cy) {
    /* Body outline (5 wide x 4 tall) */
    canvas_draw_frame(canvas, cx - 2, cy - 3, 5, 4);
    /* Antenna */
    canvas_draw_line(canvas, cx, cy - 3, cx, cy - 5);
    /* Legs */
    canvas_draw_line(canvas, cx - 2, cy, cx - 3, cy + 3);
    canvas_draw_line(canvas, cx + 2, cy, cx + 3, cy + 3);
    /* Foot pads */
    canvas_draw_line(canvas, cx - 4, cy + 3, cx - 2, cy + 3);
    canvas_draw_line(canvas, cx + 2, cy + 3, cx + 4, cy + 3);
}

void lander_draw_rotated(Canvas* canvas, float cx, float cy, float angle, float thrust) {
    float s = sinf(angle);
    float c = cosf(angle);

#define RX(lx, ly) ((int)(cx + (lx) * c - (ly) * s))
#define RY(lx, ly) ((int)(cy + (lx) * s + (ly) * c))

    /* Body rectangle: 4 edges */
    canvas_draw_line(canvas,
        RX(-LANDER_HALF_W, LANDER_BODY_TOP), RY(-LANDER_HALF_W, LANDER_BODY_TOP),
        RX( LANDER_HALF_W, LANDER_BODY_TOP), RY( LANDER_HALF_W, LANDER_BODY_TOP));
    canvas_draw_line(canvas,
        RX( LANDER_HALF_W, LANDER_BODY_TOP), RY( LANDER_HALF_W, LANDER_BODY_TOP),
        RX( LANDER_HALF_W, LANDER_BODY_BOT), RY( LANDER_HALF_W, LANDER_BODY_BOT));
    canvas_draw_line(canvas,
        RX( LANDER_HALF_W, LANDER_BODY_BOT), RY( LANDER_HALF_W, LANDER_BODY_BOT),
        RX(-LANDER_HALF_W, LANDER_BODY_BOT), RY(-LANDER_HALF_W, LANDER_BODY_BOT));
    canvas_draw_line(canvas,
        RX(-LANDER_HALF_W, LANDER_BODY_BOT), RY(-LANDER_HALF_W, LANDER_BODY_BOT),
        RX(-LANDER_HALF_W, LANDER_BODY_TOP), RY(-LANDER_HALF_W, LANDER_BODY_TOP));

    /* Antenna */
    canvas_draw_line(canvas,
        RX(0, LANDER_BODY_TOP),    RY(0, LANDER_BODY_TOP),
        RX(0, LANDER_ANTENNA_TOP), RY(0, LANDER_ANTENNA_TOP));

    /* Legs */
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

    /* Thrust flame — inverted triangle from body bottom corners to a point
     * below, length scaled by thrust intensity. */
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
