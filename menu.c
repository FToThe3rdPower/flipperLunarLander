#include "menu.h"
#include "lander_sprite.h"
#include <gui/canvas.h>

/* ----- Mode tables -------------------------------------------------------
 * Order here must match the enums in lunar_lander.h. The Tap-first ordering
 * for thrust was a deliberate choice (easiest mode first); fuel modes go
 * easiest-to-hardest top to bottom. */

const char* const thrust_mode_label[ThrustModeCount] = {
    "Tap Impulse",
    "Binary",
    "Ramp",
    "Vidya module Tilt+Tap",
    "Vidya module Tilt+Binary",
    "Vidya module Tilt+Ramp",
    "Vidya module Full Tilt",
};

const char* const thrust_mode_desc[ThrustModeCount] = {
    "Tap UP = one burst",
    "Hold UP = full thrust",
    "Hold UP, thrust ramps up",
    "Tilt steers, tap UP=burst",
    "Tilt steers, hold UP=thrust",
    "Tilt steers, UP ramps thrust",
    "Tilt steers + fires thrusters",
};

const char* const fuel_mode_label[FuelModeCount] = {
    "Full fuel each lvl",
    "No refuel, Easy: 500",
    "No refuel, Med: 350",
    "No refuel, Hard: 200",
};

const char* const fuel_mode_desc[FuelModeCount] = {
    "Refills to 100 per lvl",
    "500 total, no top-ups",
    "350 total, no top-ups",
    "200 total, no top-ups",
};

const int fuel_mode_starting[FuelModeCount] = {
    100,
    500,
    350,
    200,
};

/* ----- Drawing ----------------------------------------------------------- */

/* 7x8 wrench icon, centered at (cx, cy).
 *   ###....    head: top bridge
 *   ..#....    head: right wall (C opens left — 2px gap = jaw)
 *   ..#....    head: right wall
 *   ###....    head: bottom bridge
 *   ..##...    handle (diagonal down-right)
 *   ...##..
 *   ....##.
 *   .....##
 */
static void draw_wrench_icon(Canvas* canvas, int cx, int cy) {
    /* Head: C-shape opening to the left (jaw for gripping) */
    canvas_draw_line(canvas, cx - 3, cy - 4, cx - 1, cy - 4);  // top bridge
    canvas_draw_dot(canvas,  cx - 1, cy - 3);                   // right wall
    canvas_draw_dot(canvas,  cx - 1, cy - 2);                   // right wall
    canvas_draw_line(canvas, cx - 3, cy - 1, cx - 1, cy - 1);  // bottom bridge
    /* Handle: diagonal 2px-wide line */
    canvas_draw_line(canvas, cx - 1, cy,     cx,     cy);
    canvas_draw_line(canvas, cx,     cy + 1, cx + 1, cy + 1);
    canvas_draw_line(canvas, cx + 1, cy + 2, cx + 2, cy + 2);
    canvas_draw_line(canvas, cx + 2, cy + 3, cx + 3, cy + 3);
}

/* Title bar layout:
 *   [ lander  "LUNAR LANDER" ]  [ gear ]
 *   x=0..109 = About button   |  x=112..127 = Settings button
 *   separator line at y=10, content lives in y=0..9
 *
 * Each button is independently focusable via MenuTitleSel. Focus is shown
 * as a filled rbox behind the contents (matching the selector-row style),
 * with the contents drawn in white. When the title row isn't focused at
 * all (MenuRow != Title) neither button has a frame — the bar looks like
 * its old self. */
static void draw_title(Canvas* canvas, const MenuState* m) {
    bool about_focused    = (m->row == MenuRowTitle && m->title_sel == MenuTitleSelAbout);
    bool settings_focused = (m->row == MenuRowTitle && m->title_sel == MenuTitleSelSettings);

    /* Lander sprite — decorative, sits left of the about button */
    lander_draw_static(canvas, 6, 5);

    /* About button: title text only, starts after the sprite */
    if (about_focused) {
        canvas_draw_rbox(canvas, 13, 0, 97, 10, 2);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 61, 5, AlignCenter, AlignCenter, "LUNAR LANDER");
    if (about_focused) {
        canvas_set_color(canvas, ColorBlack);
    }

    /* Settings button: 5x5 gear icon at (119, 4) */
    if (settings_focused) {
        canvas_draw_rbox(canvas, 112, 0, 16, 10, 2);
        canvas_set_color(canvas, ColorWhite);
    }
    draw_wrench_icon(canvas, 119, 5);
    if (settings_focused) {
        canvas_set_color(canvas, ColorBlack);
    }

    canvas_draw_line(canvas, 0, 10, SCREEN_W - 1, 10);
}

/* Selector row factored so thrust and fuel share the same chrome.
 * `label` is what shows in the middle; `focused` controls the inverted look. */
static void draw_selector_row(Canvas* canvas, int y, const char* label, bool focused) {
    if (focused) {
        canvas_draw_rbox(canvas, 2, y, SCREEN_W - 4, 12, 2);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, 2, y, SCREEN_W - 4, 12, 2);
    }
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 7, y + 6, AlignLeft, AlignCenter, "<");
    canvas_draw_str_aligned(canvas, SCREEN_W - 7, y + 6, AlignRight, AlignCenter, ">");
    canvas_draw_str_aligned(canvas, SCREEN_W / 2, y + 6, AlignCenter, AlignCenter, label);
    canvas_set_color(canvas, ColorBlack);
}

/* Single description line shown beneath the selectors. Whichever row is
 * focused, its description shows. Title/Buttons rows → no description. */
static void draw_focused_desc(Canvas* canvas, const MenuState* m, int y) {
    const char* desc = NULL;
    if (m->row == MenuRowThrust)      desc = thrust_mode_desc[m->thrust_mode];
    else if (m->row == MenuRowFuel)   desc = fuel_mode_desc[m->fuel_mode];
    if (!desc) return;
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, SCREEN_W / 2, y, AlignCenter, AlignTop, desc);
}

static void draw_button(
    Canvas* canvas, int x, int y, int w, int h, const char* label, bool selected) {
    if (selected) {
        canvas_draw_rbox(canvas, x, y, w, h, 2);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, x, y, w, h, 2);
    }
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, x + w / 2, y + h / 2, AlignCenter, AlignCenter, label);
    canvas_set_color(canvas, ColorBlack);
}

static void draw_button_row(Canvas* canvas, const MenuState* m, int y) {
    bool row_focused = (m->row == MenuRowButtons);
    const int btn_w = 56, btn_h = 12, gap = 4;
    int x0 = (SCREEN_W - (btn_w * 2 + gap)) / 2;
    draw_button(canvas, x0,                    y, btn_w, btn_h, "START",
                row_focused && m->btn == MenuBtnStart);
    draw_button(canvas, x0 + btn_w + gap,      y, btn_w, btn_h, "TUTORIAL",
                row_focused && m->btn == MenuBtnTutorial);
}

void menu_draw(Canvas* canvas, const MenuState* m) {
    draw_title(canvas, m);
    /* Layout:
     *   0..10   title bar + separator
     *   12..23  thrust selector
     *   25..36  fuel selector
     *   38..45  description for focused row (blank on Title/Buttons)
     *   52..63  Start / Tutorial buttons
     */
    draw_selector_row(canvas, 12, thrust_mode_label[m->thrust_mode], m->row == MenuRowThrust);
    draw_selector_row(canvas, 25, fuel_mode_label[m->fuel_mode],     m->row == MenuRowFuel);
    draw_focused_desc(canvas, m, 38);
    draw_button_row(canvas, m, 52);
}

/* ----- State & input ----------------------------------------------------- */

void menu_init(MenuState* m) {
    m->thrust_mode  = ThrustModeTapImpulse;
    m->fuel_mode    = FuelModeFull;
    m->btn          = MenuBtnStart;
    m->row          = MenuRowThrust;
    m->title_sel    = MenuTitleSelAbout;
    m->sound_on     = true;
    m->vibration_on = true;
}

MenuAction menu_input(MenuState* m, const InputEvent* ev) {
    if (ev->type != InputTypeShort && ev->type != InputTypeRepeat) return MenuActionNone;

    switch (ev->key) {
        case InputKeyUp:
            if (m->row > 0) m->row--;
            break;
        case InputKeyDown:
            if (m->row < MenuRowCount - 1) m->row++;
            break;
        case InputKeyLeft:
            if (m->row == MenuRowTitle) {
                if (m->title_sel > 0) m->title_sel--;
            } else if (m->row == MenuRowThrust) {
                m->thrust_mode = (m->thrust_mode + ThrustModeCount - 1) % ThrustModeCount;
            } else if (m->row == MenuRowFuel) {
                m->fuel_mode = (m->fuel_mode + FuelModeCount - 1) % FuelModeCount;
            } else if (m->row == MenuRowButtons && m->btn > 0) {
                m->btn--;
            }
            break;
        case InputKeyRight:
            if (m->row == MenuRowTitle) {
                if (m->title_sel < MenuTitleSelCount - 1) m->title_sel++;
            } else if (m->row == MenuRowThrust) {
                m->thrust_mode = (m->thrust_mode + 1) % ThrustModeCount;
            } else if (m->row == MenuRowFuel) {
                m->fuel_mode = (m->fuel_mode + 1) % FuelModeCount;
            } else if (m->row == MenuRowButtons && m->btn < MenuBtnCount - 1) {
                m->btn++;
            }
            break;
        case InputKeyOk:
            if (m->row == MenuRowTitle) {
                return (m->title_sel == MenuTitleSelAbout)
                    ? MenuActionInfo
                    : MenuActionSettings;
            } else if (m->row == MenuRowButtons) {
                return (m->btn == MenuBtnStart) ? MenuActionStart : MenuActionTutorial;
            }
            break;
        case InputKeyBack:
            return MenuActionExit;
        default:
            break;
    }
    return MenuActionNone;
}
