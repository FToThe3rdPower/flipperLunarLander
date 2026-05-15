/*
 * Lunar Lander for Flipper Zero
 * v0.1 - menu screen only; game and tutorial are placeholders.
 *
 * Architecture:
 *   - Single ViewPort with draw + input callbacks.
 *   - Input callback only pushes events to a queue (must stay fast,
 *     it's called on the GUI thread).
 *   - Main loop pops events, mutates AppModel under a mutex, then
 *     requests a redraw with view_port_update().
 *   - Draw callback acquires the mutex briefly and renders from the model.
 */

#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>

#define SCREEN_W 128
#define SCREEN_H 64

/* ----- Model ------------------------------------------------------------- */

typedef enum {
    ThrustModeBinary = 0,   // hold = full thrust, release = off
    ThrustModeTapImpulse,   // each tap fires a fixed impulse
    ThrustModeRamp,         // hold longer -> more thrust
    ThrustModeCount,
} ThrustMode;

static const char* const thrust_mode_label[ThrustModeCount] = {
    "Binary",
    "Tap Impulse",
    "Hold to Ramp",
};

typedef enum {
    MenuRowThrust = 0,
    MenuRowButtons,
    MenuRowCount,
} MenuRow;

typedef enum {
    MenuBtnStart = 0,
    MenuBtnTutorial,
    MenuBtnCount,
} MenuBtn;

typedef enum {
    ScreenMenu,
    ScreenGame,      // TODO
    ScreenTutorial,  // TODO
} Screen;

typedef struct {
    Screen screen;
    ThrustMode thrust_mode;
    MenuRow menu_row;
    MenuBtn menu_btn;
    bool should_exit;
} AppModel;

typedef struct {
    FuriMessageQueue* input_queue;
    FuriMutex* mutex;
    Gui* gui;
    ViewPort* view_port;
    AppModel model;
} App;

/* ----- Drawing helpers --------------------------------------------------- */

/* Draws a small lander sprite anchored at the bottom-left foot, (x,y) = left foot tip.
 * Footprint is 11 wide x 9 tall. */
static void draw_lander_sprite(Canvas* canvas, int x, int y) {
    // Body: rounded box
    canvas_draw_rbox(canvas, x + 2, y - 6, 7, 4, 1);
    // Antenna
    canvas_draw_line(canvas, x + 5, y - 6, x + 5, y - 9);
    canvas_draw_dot(canvas, x + 5, y - 9);
    // Legs (splayed outward)
    canvas_draw_line(canvas, x + 3, y - 2, x, y);
    canvas_draw_line(canvas, x + 7, y - 2, x + 10, y);
    // Feet
    canvas_draw_line(canvas, x - 1, y, x + 1, y);
    canvas_draw_line(canvas, x + 9, y, x + 11, y);
}

/* Title bar: lander on the left, "LUNAR LANDER" centered next to it. */
static void draw_title(Canvas* canvas) {
    draw_lander_sprite(canvas, 4, 14);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, SCREEN_W / 2 + 8, 8, AlignCenter, AlignTop, "LUNAR LANDER");
    // Underline
    canvas_draw_line(canvas, 0, 18, SCREEN_W - 1, 18);
}

/* Thrust selector row at y_top, height 13. */
static void draw_thrust_row(Canvas* canvas, const AppModel* m, int y_top) {
    bool focused = (m->menu_row == MenuRowThrust);
    if (focused) {
        canvas_draw_rbox(canvas, 2, y_top, SCREEN_W - 4, 13, 2);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, 2, y_top, SCREEN_W - 4, 13, 2);
    }

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 7, y_top + 6, AlignLeft, AlignCenter, "<");
    canvas_draw_str_aligned(canvas, SCREEN_W - 7, y_top + 6, AlignRight, AlignCenter, ">");
    canvas_draw_str_aligned(
        canvas,
        SCREEN_W / 2,
        y_top + 6,
        AlignCenter,
        AlignCenter,
        thrust_mode_label[m->thrust_mode]);

    canvas_set_color(canvas, ColorBlack);
}

/* One button. Filled when selected, frame-only otherwise. */
static void draw_button(Canvas* canvas, int x, int y, int w, int h, const char* label, bool selected) {
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

static void draw_button_row(Canvas* canvas, const AppModel* m, int y_top) {
    bool row_focused = (m->menu_row == MenuRowButtons);
    int btn_w = 56;
    int btn_h = 13;
    int gap = 4;
    int total = btn_w * 2 + gap;
    int x0 = (SCREEN_W - total) / 2;

    draw_button(canvas, x0, y_top, btn_w, btn_h, "START",
                row_focused && m->menu_btn == MenuBtnStart);
    draw_button(canvas, x0 + btn_w + gap, y_top, btn_w, btn_h, "TUTORIAL",
                row_focused && m->menu_btn == MenuBtnTutorial);
}

static void draw_menu(Canvas* canvas, const AppModel* m) {
    draw_title(canvas);
    draw_thrust_row(canvas, m, 24);
    draw_button_row(canvas, m, 48);
}

/* ----- Callbacks --------------------------------------------------------- */

static void draw_callback(Canvas* canvas, void* ctx) {
    App* app = ctx;
    // Don't block the GUI thread if the model is busy; just skip this frame.
    if (furi_mutex_acquire(app->mutex, 25) != FuriStatusOk) return;

    canvas_clear(canvas);
    switch (app->model.screen) {
        case ScreenMenu:
            draw_menu(canvas, &app->model);
            break;
        case ScreenGame:
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str_aligned(canvas, SCREEN_W / 2, 24, AlignCenter, AlignCenter, "GAME");
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(canvas, SCREEN_W / 2, 38, AlignCenter, AlignCenter,
                                    "Coming soon. Press Back.");
            break;
        case ScreenTutorial:
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str_aligned(canvas, SCREEN_W / 2, 24, AlignCenter, AlignCenter, "TUTORIAL");
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(canvas, SCREEN_W / 2, 38, AlignCenter, AlignCenter,
                                    "Coming soon. Press Back.");
            break;
    }

    furi_mutex_release(app->mutex);
}

static void input_callback(InputEvent* event, void* ctx) {
    App* app = ctx;
    // Drop on overflow rather than block the GUI thread.
    furi_message_queue_put(app->input_queue, event, 0);
}

/* ----- Input handling ---------------------------------------------------- */

static void handle_menu_input(AppModel* m, const InputEvent* ev) {
    // Only react to short presses and key repeats.
    if (ev->type != InputTypeShort && ev->type != InputTypeRepeat) return;

    switch (ev->key) {
        case InputKeyUp:
            if (m->menu_row > 0) m->menu_row--;
            break;
        case InputKeyDown:
            if (m->menu_row < MenuRowCount - 1) m->menu_row++;
            break;
        case InputKeyLeft:
            if (m->menu_row == MenuRowThrust) {
                m->thrust_mode = (m->thrust_mode + ThrustModeCount - 1) % ThrustModeCount;
            } else {
                if (m->menu_btn > 0) m->menu_btn--;
            }
            break;
        case InputKeyRight:
            if (m->menu_row == MenuRowThrust) {
                m->thrust_mode = (m->thrust_mode + 1) % ThrustModeCount;
            } else {
                if (m->menu_btn < MenuBtnCount - 1) m->menu_btn++;
            }
            break;
        case InputKeyOk:
            if (m->menu_row == MenuRowButtons) {
                m->screen = (m->menu_btn == MenuBtnStart) ? ScreenGame : ScreenTutorial;
            }
            break;
        case InputKeyBack:
            m->should_exit = true;
            break;
        default:
            break;
    }
}

/* ----- App entrypoint ---------------------------------------------------- */

int32_t lunar_lander_app(void* p) {
    UNUSED(p);

    App* app = malloc(sizeof(App));
    app->input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->model.screen = ScreenMenu;
    app->model.thrust_mode = ThrustModeBinary;
    app->model.menu_row = MenuRowButtons;
    app->model.menu_btn = MenuBtnStart;
    app->model.should_exit = false;

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    InputEvent ev;
    while (!app->model.should_exit) {
        if (furi_message_queue_get(app->input_queue, &ev, 100) != FuriStatusOk) continue;

        furi_mutex_acquire(app->mutex, FuriWaitForever);

        if (app->model.screen == ScreenMenu) {
            handle_menu_input(&app->model, &ev);
        } else {
            // On placeholder game/tutorial screens, Back returns to menu.
            if (ev.key == InputKeyBack &&
                (ev.type == InputTypeShort || ev.type == InputTypeRepeat)) {
                app->model.screen = ScreenMenu;
            }
        }

        furi_mutex_release(app->mutex);
        view_port_update(app->view_port);
    }

    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(app->input_queue);
    furi_mutex_free(app->mutex);
    free(app);
    return 0;
}
