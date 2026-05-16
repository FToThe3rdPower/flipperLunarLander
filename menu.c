#include "menu.h"
#include <gui/canvas.h>

// List of thrust modes - due to a lack of a joystick, Claude had 2 addition suggestions to make the D-pad work for our port
const char* const thrust_mode_label[ThrustModeCount] = {
    "Tap Impulse",
    "Binary",
    "Ramp",
};

// Descriptions to go with the thrust modes to explain how they work a lil more
const char* const thrust_mode_desc[ThrustModeCount] = {
    "Tap UP = one burst",
    "Hold UP = full thrust",
    "Hold UP, thrust ramps up",
};

/* ----- Drawing ----------------------------------------------------------- */

/* Small lander sprite, 11 wide x 9 tall. (x,y) = left foot. */
static void draw_menu_lander(Canvas* canvas, int x, int y) {
    canvas_draw_rbox(canvas, x + 2, y - 6, 7, 4, 1);   // body
    canvas_draw_dot(canvas, x + 5, y - 9);
    canvas_draw_line(canvas, x + 3, y - 2, x, y);      // L leg
    canvas_draw_line(canvas, x + 7, y - 2, x + 10, y); // R leg
    canvas_draw_line(canvas, x - 1, y, x + 1, y);      // L foot
    canvas_draw_line(canvas, x + 9, y, x + 11, y);     // R foot
}

// Top title bar
static void draw_title(Canvas* canvas) {
    draw_menu_lander(canvas, 4, 7);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, SCREEN_W / 2, 0, AlignCenter, AlignTop, "LUNAR LANDER");
    canvas_draw_line(canvas, 0, 8, SCREEN_W - 1, 8);
}

// Thrust mode selector
static void draw_thrust_row(Canvas* canvas, const MenuState* m, int y) {
    bool focused = (m->row == MenuRowThrust);
    if (focused) {
        canvas_draw_rbox(canvas, 2, y, SCREEN_W - 4, 12, 2);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, 2, y, SCREEN_W - 4, 12, 2);
    }
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 7, y + 6, AlignLeft, AlignCenter, "<");
    canvas_draw_str_aligned(canvas, SCREEN_W - 7, y + 6, AlignRight, AlignCenter, ">");
    canvas_draw_str_aligned(
        canvas, SCREEN_W / 2, y + 6, AlignCenter, AlignCenter,
        thrust_mode_label[m->thrust_mode]);
    canvas_set_color(canvas, ColorBlack);
}

// Thurst mode explanation
static void draw_thrust_desc(Canvas* canvas, const MenuState* m, int y) {
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas, SCREEN_W / 2, y, AlignCenter, AlignTop,
        thrust_mode_desc[m->thrust_mode]);
}

// Button draw-er
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

// Put the Start and Tutorial buttons on the bottom row
static void draw_button_row(Canvas* canvas, const MenuState* m, int y) {
    bool row_focused = (m->row == MenuRowButtons);
    const int btn_w = 56, btn_h = 12, gap = 4;
    int x0 = (SCREEN_W - (btn_w * 2 + gap)) / 2;
    draw_button(canvas, x0, y, btn_w, btn_h, "START",
                row_focused && m->btn == MenuBtnStart);
    draw_button(canvas, x0 + btn_w + gap, y, btn_w, btn_h, "TUTORIAL",
                row_focused && m->btn == MenuBtnTutorial);
}

// Draw the menu on the screen
void menu_draw(Canvas* canvas, const MenuState* m) {
    draw_title(canvas);
    draw_thrust_row(canvas, m, 20);
    draw_thrust_desc(canvas, m, 34);
    draw_button_row(canvas, m, 50);
}

/* ----- State & input ----------------------------------------------------- */

void menu_init(MenuState* m) {
    m->row = MenuRowThrust;            //default focus on launch
    m->btn = MenuBtnStart;
    m->thrust_mode = ThrustModeBinary;
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
            if (m->row == MenuRowThrust) {
                m->thrust_mode = (m->thrust_mode + ThrustModeCount - 1) % ThrustModeCount;
            } else if (m->btn > 0) {
                m->btn--;
            }
            break;
        case InputKeyRight:
            if (m->row == MenuRowThrust) {
                m->thrust_mode = (m->thrust_mode + 1) % ThrustModeCount;
            } else if (m->btn < MenuBtnCount - 1) {
                m->btn++;
            }
            break;
        case InputKeyOk:
            if (m->row == MenuRowButtons) {
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
