/*
 * Lunar Lander for Flipper Zero - main / dispatch
 * v0.2
 *
 * Event-driven main loop: input + tick events are pushed into a single queue.
 * 60 Hz tick timer drives game physics; menu is event-driven only.
 * All model state lives under a mutex; the draw callback uses try-lock so it
 * never blocks the GUI thread.
 */

#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#include <string.h>

#include "lunar_lander.h"
#include "menu.h"
#include "game.h"

#define TICK_HZ 60

typedef enum {
    AppEventInput = 0,
    AppEventTick,
} AppEventType;

typedef struct {
    AppEventType type;
    InputEvent input; // valid when type == AppEventInput
} AppEvent;

typedef struct {
    Screen screen;
    bool should_exit;
    MenuState menu;
    GameState game;
} AppModel;

typedef struct {
    FuriMessageQueue* queue;
    FuriMutex* mutex;
    FuriTimer* tick_timer;
    Gui* gui;
    ViewPort* view_port;
    uint32_t last_tick_ms;
    AppModel model;
} App;

/* ----- Callbacks --------------------------------------------------------- */

static void draw_callback(Canvas* canvas, void* ctx) {
    App* app = ctx;
    if (furi_mutex_acquire(app->mutex, 25) != FuriStatusOk) return;

    canvas_clear(canvas);
    switch (app->model.screen) {
        case ScreenMenu:
            menu_draw(canvas, &app->model.menu);
            break;
        case ScreenGame:
            game_draw(canvas, &app->model.game);
            break;
        case ScreenTutorial:
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str_aligned(
                canvas, SCREEN_W / 2, 24, AlignCenter, AlignCenter, "TUTORIAL");
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(
                canvas, SCREEN_W / 2, 40, AlignCenter, AlignCenter, "Coming soon. Press Back.");
            break;
        case ScreenInfo: {
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str_aligned(
                canvas, SCREEN_W / 2, 2, AlignCenter, AlignTop, "ABOUT");
            canvas_draw_line(canvas, 0, 12, SCREEN_W - 1, 12);
            canvas_set_font(canvas, FontSecondary);
            /* Credit text wrapped to ~25 chars per line at FontSecondary. */
            canvas_draw_str_aligned(canvas, SCREEN_W / 2, 16, AlignCenter, AlignTop,
                "Claude Opus 4.7 wrote");
            canvas_draw_str_aligned(canvas, SCREEN_W / 2, 24, AlignCenter, AlignTop,
                "this from prompts,");
            canvas_draw_str_aligned(canvas, SCREEN_W / 2, 32, AlignCenter, AlignTop,
                "graphics, and tweaks by");
            canvas_draw_str_aligned(canvas, SCREEN_W / 2, 40, AlignCenter, AlignTop,
                "FToThe3rdPower");
            canvas_draw_str_aligned(canvas, SCREEN_W / 2, 48, AlignCenter, AlignTop,
                "on GitHub.");
            canvas_draw_str_aligned(canvas, SCREEN_W / 2, SCREEN_H - 1,
                AlignCenter, AlignBottom, "Back to return");
            break;
        }
    }

    furi_mutex_release(app->mutex);
}

static void input_callback(InputEvent* event, void* ctx) {
    App* app = ctx;
    AppEvent ev = {.type = AppEventInput, .input = *event};
    furi_message_queue_put(app->queue, &ev, 0);
}

static void tick_timer_callback(void* ctx) {
    App* app = ctx;
    AppEvent ev = {.type = AppEventTick};
    furi_message_queue_put(app->queue, &ev, 0);
}

/* ----- Dispatch ---------------------------------------------------------- */

/* Single point of truth for screen transitions. Handles audio
 * acquire/release on entering/leaving ScreenGame, plus game state init. */
static void set_screen(AppModel* m, Screen new_screen) {
    Screen old = m->screen;
    if (old == ScreenGame && new_screen != ScreenGame) {
        game_audio_stop();
    }
    m->screen = new_screen;
    if (new_screen == ScreenGame && old != ScreenGame) {
        game_audio_start();
        FuelMode fm = m->menu.fuel_mode;
        int start_fuel = fuel_mode_starting[fm];
        game_init(&m->game, 1, 0, fm, start_fuel);  // fresh game
    }
}

static void handle_menu_action(AppModel* m, MenuAction action) {
    switch (action) {
        case MenuActionStart:    set_screen(m, ScreenGame); break;
        case MenuActionTutorial: set_screen(m, ScreenTutorial); break;
        case MenuActionInfo:     set_screen(m, ScreenInfo); break;
        case MenuActionExit:     m->should_exit = true; break;
        case MenuActionNone:     break;
    }
}

static void handle_input_event(App* app, const InputEvent* ev) {
    AppModel* m = &app->model;

    switch (m->screen) {
        case ScreenMenu: {
            MenuAction a = menu_input(&m->menu, ev);
            handle_menu_action(m, a);
            break;
        }
        case ScreenGame: {
            /* Special-case TapImpulse on Up-press, before the held-state update. */
            if (m->menu.thrust_mode == ThrustModeTapImpulse &&
                ev->key == InputKeyUp && ev->type == InputTypePress) {
                game_apply_tap_impulse(&m->game);
            }
            GameAction a = game_input(&m->game, ev);
            if (a == GameActionExitToMenu) set_screen(m, ScreenMenu);
            break;
        }
        case ScreenTutorial:
        case ScreenInfo:
            if (ev->key == InputKeyBack &&
                (ev->type == InputTypeShort || ev->type == InputTypeLong)) {
                set_screen(m, ScreenMenu);
            }
            break;
    }
}

static void handle_tick(App* app, float dt) {
    if (app->model.screen == ScreenGame) {
        game_tick(&app->model.game, app->model.menu.thrust_mode, dt);
        game_audio_update(&app->model.game, app->model.menu.thrust_mode);
    }
}

/* ----- App entrypoint ---------------------------------------------------- */

int32_t lunar_lander_app(void* p) {
    UNUSED(p);

    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));
    app->queue = furi_message_queue_alloc(16, sizeof(AppEvent));
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->tick_timer =
        furi_timer_alloc(tick_timer_callback, FuriTimerTypePeriodic, app);

    menu_init(&app->model.menu);
    app->model.screen = ScreenMenu;
    app->model.should_exit = false;

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    /* Start the 60Hz tick. period is in OS ticks; on Flipper that's 1ms each. */
    uint32_t tick_freq = furi_kernel_get_tick_frequency();
    uint32_t period = tick_freq / TICK_HZ;
    if (period < 1) period = 1;
    furi_timer_start(app->tick_timer, period);
    app->last_tick_ms = furi_get_tick();

    while (!app->model.should_exit) {
        AppEvent ev;
        if (furi_message_queue_get(app->queue, &ev, FuriWaitForever) != FuriStatusOk) continue;

        furi_mutex_acquire(app->mutex, FuriWaitForever);

        if (ev.type == AppEventInput) {
            handle_input_event(app, &ev.input);
        } else { // AppEventTick
            uint32_t now = furi_get_tick();
            float dt = (float)(now - app->last_tick_ms) / (float)tick_freq;
            app->last_tick_ms = now;
            /* Guard against giant dt after a stall. */
            if (dt > 0.1f) dt = 0.1f;
            handle_tick(app, dt);
        }

        furi_mutex_release(app->mutex);
        view_port_update(app->view_port);
    }

    furi_timer_stop(app->tick_timer);
    furi_timer_free(app->tick_timer);
    /* Belt-and-suspenders: if somehow we exit while still in-game, release
     * the speaker and turn off vibration so we don't leave it stuck. */
    game_audio_stop();
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(app->queue);
    furi_mutex_free(app->mutex);
    free(app);
    return 0;
}
