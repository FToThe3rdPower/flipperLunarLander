/*
 * Lunar Lander for Flipper Zero - main / dispatch
 *
 * Event-driven main loop: input + tick events are pushed into a single queue.
 * 60 Hz tick timer drives game physics; menu is event-driven only.
 * All model state lives under a mutex; the draw callback waits at most 25 ms
 * for it so it never stalls the GUI thread for long.
 */

#include <furi.h>
#include <furi_hal_cortex.h>
#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define HIGH_SCORE_PATH EXT_PATH("apps_data/lunar_lander/score.bin")
#define SETTINGS_PATH   EXT_PATH("apps_data/lunar_lander/lunarLanderSettings.bin")
#define SETTINGS_VERSION 1

#include "lunar_lander.h"
#include "menu.h"
#include "game.h"
#include "vgm_tilt.h"

#define TICK_HZ 60

/* In-game redraw caps, in ms between frames. Each frame goes to the LCD over
 * SPI and, with a Video Game Module on HDMI, to the TV over a 1.84 Mbaud
 * serial link (~6 ms per frame), and the firmware busy-waits on both. That
 * load starves its lowest-priority thread, which delivers our ticks and key
 * releases. Physics still runs every tick; the VGM repeats each frame on
 * the TV. 28 ms = every 2nd 16 ms tick (~31 fps); 66 ms (~15 fps) still
 * covers the 2.5-3 Hz banner and crash-flash blinking. */
#define FRAME_MS       28
#define STILL_FRAME_MS 66

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
    int settings_focus;   // 0 sound, 1 vibration, 2 difficulty, 3 debug HUD; reset on entering ScreenSettings
    VgmTilt* vgm;         // non-NULL while a VGM tilt mode is active
    int  tutorial_level;          // 1 or 2 (valid when screen == ScreenTutorial)
    bool tutorial_popup_showing;  // physics paused; waiting for OK to dismiss
    int  high_score;
    bool game_complete_new_record;

    /* Debug overlay — toggled from Settings screen. */
    bool     debug_hud;
    uint32_t debug_tick_count;
    uint32_t debug_free_heap;
    uint8_t  debug_queue_depth;
    uint8_t  debug_peak_queue;
    /* Longest in-game draw callback and longest gap between processed ticks:
     * the _max fields collect the current 1 s window, the others show the
     * last one. */
    uint32_t debug_draw_us;
    uint32_t debug_draw_us_max;
    uint32_t debug_gap_ms;
    uint32_t debug_gap_ms_max;
} AppModel;

typedef struct {
    FuriMessageQueue* queue;
    FuriMutex* mutex;
    FuriTimer* tick_timer;
    Gui* gui;
    ViewPort* view_port;
    uint32_t last_tick_ms;
    uint32_t last_draw_tick;      // kernel tick of the last in-game redraw request
    volatile bool tick_pending;   // a tick is already in the queue; set by the timer, cleared by the loop
    AppModel model;
} App;

/* ----- High score persistence -------------------------------------------- */

static void high_score_load(int* hs) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    *hs = 0;
    if(storage_file_open(file, HIGH_SCORE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        /* A short or damaged file reads as no score rather than garbage. */
        if(storage_file_read(file, hs, sizeof(int)) != sizeof(int) || *hs < 0) *hs = 0;
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void high_score_save(int hs) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, EXT_PATH("apps_data/lunar_lander"));
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, HIGH_SCORE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, &hs, sizeof(int));
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

/* ----- Settings persistence ---------------------------------------------- */

typedef struct {
    uint8_t version;
    uint8_t thrust_mode;
    uint8_t fuel_mode;
    uint8_t difficulty;
    uint8_t sound_level;
    uint8_t vibration_level;
    uint8_t debug_hud;    // 0/1; uint8_t so a damaged byte can't become an invalid bool
} SavedSettings;

static void settings_load(AppModel* m) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, SETTINGS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        SavedSettings s;
        /* Every value indexes a label/volume table, so a damaged file must
         * not get through: out-of-range values keep the defaults. */
        if(storage_file_read(file, &s, sizeof(s)) == sizeof(s) &&
           s.version == SETTINGS_VERSION &&
           s.thrust_mode < ThrustModeCount && s.fuel_mode < FuelModeCount &&
           s.difficulty < DifficultyCount && s.sound_level < SoundCount &&
           s.vibration_level < VibrationCount) {
            m->menu.thrust_mode  = (ThrustMode)s.thrust_mode;
            m->menu.fuel_mode    = (FuelMode)s.fuel_mode;
            m->menu.difficulty   = (Difficulty)s.difficulty;
            m->menu.sound_level     = (SoundLevel)s.sound_level;
            m->menu.vibration_level = (VibrationLevel)s.vibration_level;
            m->debug_hud         = s.debug_hud != 0;
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void settings_save(const AppModel* m) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, EXT_PATH("apps_data/lunar_lander"));
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, SETTINGS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        SavedSettings s = {
            .version      = SETTINGS_VERSION,
            .thrust_mode  = (uint8_t)m->menu.thrust_mode,
            .fuel_mode    = (uint8_t)m->menu.fuel_mode,
            .difficulty   = (uint8_t)m->menu.difficulty,
            .sound_level     = (uint8_t)m->menu.sound_level,
            .vibration_level = (uint8_t)m->menu.vibration_level,
            .debug_hud    = m->debug_hud,
        };
        storage_file_write(file, &s, sizeof(s));
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

/* ----- Callbacks --------------------------------------------------------- */

static void draw_debug_overlay(Canvas* canvas, const AppModel* m) {
    const GameState* g = &m->game;

    /* Draw debug stats in the top 24 rows where the HUD normally lives.
     * No background box — terrain and lander show through beneath. */
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);

    char buf[32];

    /* Row 0: free heap + tick counter */
    snprintf(buf, sizeof(buf), "HP:%lu TK:%lu",
             (unsigned long)m->debug_free_heap,
             (unsigned long)(m->debug_tick_count % 100000UL));
    canvas_draw_str(canvas, 0, 7, buf);

    /* Row 1: queue depth (current/peak), longest frame draw (ms) and longest
     * gap between game ticks (ms) over the last ~1 s. Ticks are due every
     * 16 ms; a big TG means they ran late, usually because the Flipper's
     * timer thread (which also delivers key releases) got no CPU time. */
    snprintf(buf, sizeof(buf), "Q:%u/%u DR:%lu.%lu TG:%lu",
             m->debug_queue_depth, m->debug_peak_queue,
             (unsigned long)(m->debug_draw_us / 1000),
             (unsigned long)(m->debug_draw_us % 1000 / 100),
             (unsigned long)m->debug_gap_ms);
    canvas_draw_str(canvas, 0, 15, buf);

    /* Row 2: angle, velocities and speed, or a NaN/runaway warning */
    bool bad_a = (g->angle != g->angle) || (g->angle > 1e6f) || (g->angle < -1e6f);
    bool bad_v = (g->vx != g->vx) || (g->vy != g->vy)
              || (g->vx > 1e6f) || (g->vx < -1e6f)
              || (g->vy > 1e6f) || (g->vy < -1e6f);
    if(bad_a || bad_v) {
        snprintf(buf, sizeof(buf), "!BAD:%s%s",
                 bad_a ? " ANG" : "", bad_v ? " VEL" : "");
    } else {
        int deg = (int)(g->angle * (180.0f / 3.14159265f));
        int speed = (int)sqrtf(g->vx * g->vx + g->vy * g->vy);
        snprintf(buf, sizeof(buf), "A:%+d X:%+d Y:%+d S:%d",
                 deg, (int)g->vx, (int)g->vy, speed);
    }
    canvas_draw_str(canvas, 0, 23, buf);
}

static void draw_callback(Canvas* canvas, void* ctx) {
    App* app = ctx;
    if (furi_mutex_acquire(app->mutex, 25) != FuriStatusOk) return;
    /* Times the in-game draw for the debug HUD's DR — i.e. how long the
     * mutex is held. The cortex timer's .start is the CPU cycle counter. */
    uint32_t draw_start = furi_hal_cortex_timer_get(0).start;

    canvas_clear(canvas);
    switch (app->model.screen) {
        case ScreenMenu:
            menu_draw(canvas, &app->model.menu);
            break;
        case ScreenGame: {
            /* The debug overlay replaces the HUD, except while the
             * landed/crashed banner is up: it would be drawn over it. */
            bool overlay = app->model.debug_hud && !game_banner_visible(&app->model.game);
            app->model.game.hud_hidden = overlay;
            game_draw(canvas, &app->model.game);
            if(overlay) draw_debug_overlay(canvas, &app->model);
            break;
        }
        case ScreenTutorial: {
            bool overlay = app->model.debug_hud && !app->model.tutorial_popup_showing &&
                           !game_banner_visible(&app->model.game);
            app->model.game.hud_hidden = overlay;
            game_draw(canvas, &app->model.game);
            if(app->model.tutorial_popup_showing) {
                game_draw_tutorial_popup(canvas,
                                         app->model.tutorial_level,
                                         app->model.menu.thrust_mode,
                                         &app->model.game);
            }
            if(overlay) draw_debug_overlay(canvas, &app->model);
            break;
        }
        case ScreenInfo: {
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str_aligned(
                canvas, SCREEN_W / 2, 2, AlignCenter, AlignTop, "ABOUT");
            canvas_draw_line(canvas, 0, 12, SCREEN_W - 1, 12);
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(canvas, SCREEN_W / 2, 16, AlignCenter, AlignTop,
                "Clauded 'from-scratch,'");
            canvas_draw_str_aligned(canvas, SCREEN_W / 2, 24, AlignCenter, AlignTop,
                "tweaked by");
            canvas_draw_str_aligned(canvas, SCREEN_W / 2, 32, AlignCenter, AlignTop,
                "FToThe3rdPower,");
            canvas_draw_str_aligned(canvas, SCREEN_W / 2, 40, AlignCenter, AlignTop,
                "inspired by the");
            canvas_draw_str_aligned(canvas, SCREEN_W / 2, 48, AlignCenter, AlignTop,
                "1979 Atari game.");
            canvas_draw_str_aligned(canvas, SCREEN_W / 2, SCREEN_H - 1,
                AlignCenter, AlignBottom, "Back to return");
            break;
        }
        case ScreenScore: {
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str_aligned(
                canvas, SCREEN_W / 2, 8, AlignCenter, AlignCenter, "HIGH SCORE");
            canvas_draw_line(canvas, 0, 14, SCREEN_W - 1, 14);
            if(app->model.high_score > 0) {
                char hs_buf[16];
                snprintf(hs_buf, sizeof(hs_buf), "%d", app->model.high_score);
                canvas_draw_str_aligned(
                    canvas, SCREEN_W / 2, 36, AlignCenter, AlignCenter, hs_buf);
            } else {
                canvas_set_font(canvas, FontSecondary);
                canvas_draw_str_aligned(
                    canvas, SCREEN_W / 2, 36, AlignCenter, AlignCenter, "No score yet");
            }
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(canvas, SCREEN_W / 2, SCREEN_H - 1,
                AlignCenter, AlignBottom, "Back to return");
            break;
        }
        case ScreenGameComplete: {
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str_aligned(
                canvas, SCREEN_W / 2, 8, AlignCenter, AlignCenter, "YOU WIN!");
            canvas_draw_line(canvas, 0, 14, SCREEN_W - 1, 14);
            canvas_set_font(canvas, FontSecondary);
            char buf[24];
            snprintf(buf, sizeof(buf), "Score: %d", app->model.game.score);
            canvas_draw_str_aligned(canvas, SCREEN_W / 2, 28, AlignCenter, AlignCenter, buf);
            if(app->model.game_complete_new_record) {
                canvas_draw_str_aligned(
                    canvas, SCREEN_W / 2, 40, AlignCenter, AlignCenter, "New high score!");
            } else {
                snprintf(buf, sizeof(buf), "Best: %d", app->model.high_score);
                canvas_draw_str_aligned(
                    canvas, SCREEN_W / 2, 40, AlignCenter, AlignCenter, buf);
            }
            canvas_draw_str_aligned(
                canvas, SCREEN_W / 2, SCREEN_H - 1,
                AlignCenter, AlignBottom, "OK / Back: menu");
            break;
        }
        case ScreenSettings: {
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str_aligned(
                canvas, SCREEN_W / 2, 2, AlignCenter, AlignTop, "SETTINGS");
            canvas_draw_line(canvas, 0, 12, SCREEN_W - 1, 12);
            char buf[24];
            snprintf(buf, sizeof(buf), "Sound: %s",
                     sound_level_label[app->model.menu.sound_level]);
            draw_selector_row(canvas, 14, buf, app->model.settings_focus == 0);
            snprintf(buf, sizeof(buf), "Vibration: %s",
                     vibration_level_label[app->model.menu.vibration_level]);
            draw_selector_row(canvas, 26, buf, app->model.settings_focus == 1);
            snprintf(buf, sizeof(buf), "Difficulty: %s",
                     difficulty_label[app->model.menu.difficulty]);
            draw_selector_row(canvas, 38, buf, app->model.settings_focus == 2);
            draw_selector_row(canvas, 50,
                app->model.debug_hud ? "Debug HUD: On" : "Debug HUD: Off",
                app->model.settings_focus == 3);
            break;
        }
    }

    if(app->model.screen == ScreenGame || app->model.screen == ScreenTutorial) {
        uint32_t us = (furi_hal_cortex_timer_get(0).start - draw_start) /
                      furi_hal_cortex_instructions_per_microsecond();
        if(us > app->model.debug_draw_us_max) app->model.debug_draw_us_max = us;
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
    /* Keep at most one tick in the queue. If the loop falls behind, extra
     * ticks would fill it and input_callback's zero-timeout put would drop
     * key Press/Release events, leaving Up/Left/Right stuck held (or never
     * held). Skipped ticks don't slow physics: dt comes from the clock. */
    if (app->tick_pending) return;
    app->tick_pending = true;
    AppEvent ev = {.type = AppEventTick};
    if (furi_message_queue_put(app->queue, &ev, 0) != FuriStatusOk) app->tick_pending = false;
}

/* ----- Dispatch ---------------------------------------------------------- */

/* Single point of truth for screen transitions. Handles audio
 * acquire/release on entering/leaving ScreenGame, plus game state init,
 * plus settings-screen focus reset. */
static void set_screen(App* app, Screen new_screen) {
    AppModel* m = &app->model;
    Screen old = m->screen;
    bool old_live = (old == ScreenGame || old == ScreenTutorial);
    bool new_live = (new_screen == ScreenGame || new_screen == ScreenTutorial);

    if(old_live && !new_live) {
        game_audio_stop();
        furi_timer_stop(app->tick_timer);
        if(m->vgm) { vgm_tilt_free(m->vgm); m->vgm = NULL; }
    }
    m->screen = new_screen;
    if(new_live && !old_live) {
        game_audio_start();
        if(new_screen == ScreenTutorial) {
            m->tutorial_level = 1;
            game_init_tutorial(&m->game, 1, 0, m->menu.difficulty);
            m->tutorial_popup_showing = true;
        } else {
            FuelMode fm = m->menu.fuel_mode;
            int start_fuel = fuel_mode_starting[fm];
            game_init(&m->game, 1, 0, fm, start_fuel, m->menu.difficulty);
        }
        bool is_vidya = (m->menu.thrust_mode >= ThrustModeVidyaTap);
        if(is_vidya) {
            m->vgm = vgm_tilt_alloc();
            m->game.vgm_missing = !vgm_tilt_present(m->vgm);
        }
        m->debug_tick_count  = 0;
        m->debug_peak_queue  = 0;
        m->debug_queue_depth = 0;
        m->debug_free_heap   = 0;
        m->debug_draw_us     = 0;
        m->debug_draw_us_max = 0;
        m->debug_gap_ms      = 0;
        m->debug_gap_ms_max  = 0;
        app->last_tick_ms = furi_get_tick();
        app->last_draw_tick = app->last_tick_ms - furi_kernel_get_tick_frequency(); // first tick draws
        uint32_t period = furi_kernel_get_tick_frequency() / TICK_HZ;
        if(period < 1) period = 1;
        furi_timer_start(app->tick_timer, period);
    }
    if(new_screen == ScreenSettings && old != ScreenSettings) {
        m->settings_focus = 0;
    }
}

static void handle_menu_action(App* app, MenuAction action) {
    AppModel* m = &app->model;
    switch (action) {
        case MenuActionStart:    set_screen(app, ScreenGame);     break;
        case MenuActionTutorial: set_screen(app, ScreenTutorial); break;
        case MenuActionScore:    set_screen(app, ScreenScore);    break;
        case MenuActionInfo:     set_screen(app, ScreenInfo);     break;
        case MenuActionSettings: set_screen(app, ScreenSettings); break;
        case MenuActionExit:     m->should_exit = true; break;
        case MenuActionNone:     break;
    }
}

static void handle_input_event(App* app, const InputEvent* ev) {
    AppModel* m = &app->model;

    switch (m->screen) {
        case ScreenMenu: {
            ThrustMode prev_thrust = m->menu.thrust_mode;
            FuelMode   prev_fuel   = m->menu.fuel_mode;
            MenuAction a = menu_input(&m->menu, ev);
            if(m->menu.thrust_mode != prev_thrust || m->menu.fuel_mode != prev_fuel) {
                settings_save(&app->model);
            }
            handle_menu_action(app, a);
            break;
        }
        case ScreenGame: {
            if((m->menu.thrust_mode == ThrustModeTapImpulse ||
                m->menu.thrust_mode == ThrustModeVidyaTap) &&
               ev->key == InputKeyUp && ev->type == InputTypePress) {
                game_apply_tap_impulse(&m->game);
            }
            GameAction a = game_input(&m->game, ev);
            if(a == GameActionExitToMenu) {
                if(m->game.score > m->high_score) {
                    m->high_score = m->game.score;
                    high_score_save(m->high_score);
                }
                set_screen(app, ScreenMenu);
            } else if(a == GameActionWin) {
                m->game_complete_new_record = (m->game.score > m->high_score);
                if(m->game_complete_new_record) {
                    m->high_score = m->game.score;
                    high_score_save(m->high_score);
                }
                set_screen(app, ScreenGameComplete);
            }
            break;
        }
        case ScreenTutorial: {
            /* No bursts while a popup is up: physics is paused, so a tap
             * would only burn fuel and launch the lander once it closes. */
            if(!m->tutorial_popup_showing &&
               (m->menu.thrust_mode == ThrustModeTapImpulse ||
                m->menu.thrust_mode == ThrustModeVidyaTap) &&
               ev->key == InputKeyUp && ev->type == InputTypePress) {
                game_apply_tap_impulse(&m->game);
            }
            /* Dismiss intro or transition popup. */
            if(m->tutorial_popup_showing &&
               ev->type == InputTypeShort && ev->key == InputKeyOk) {
                m->tutorial_popup_showing = false;
                break;
            }
            /* Level advance / retry. */
            if(ev->type == InputTypeShort && ev->key == InputKeyOk &&
               m->game.status != GameStatusFlying) {
                if(m->game.status == GameStatusLanded) {
                    if(m->tutorial_level < 2) {
                        m->tutorial_level++;
                        game_init_tutorial(&m->game, m->tutorial_level, m->game.score, m->menu.difficulty);
                        m->tutorial_popup_showing = true;
                    } else {
                        set_screen(app, ScreenMenu);
                    }
                } else {
                    game_init_tutorial(&m->game, m->tutorial_level, m->game.score, m->menu.difficulty);
                }
                break;
            }
            GameAction a = game_input(&m->game, ev);
            if(a == GameActionExitToMenu) set_screen(app, ScreenMenu);
            break;
        }
        case ScreenSettings: {
            if (ev->type != InputTypeShort && ev->type != InputTypeRepeat) break;
            SoundLevel     prev_sound = m->menu.sound_level;
            VibrationLevel prev_vibro = m->menu.vibration_level;
            Difficulty     prev_diff  = m->menu.difficulty;
            bool           prev_debug = m->debug_hud;
            switch (ev->key) {
                case InputKeyUp:
                    if (m->settings_focus > 0) m->settings_focus--;
                    break;
                case InputKeyDown:
                    if (m->settings_focus < 3) m->settings_focus++;
                    break;
                case InputKeyLeft:
                    if (m->settings_focus == 0)
                        m->menu.sound_level = (SoundLevel)((m->menu.sound_level + SoundCount - 1) % SoundCount);
                    else if (m->settings_focus == 1)
                        m->menu.vibration_level = (VibrationLevel)((m->menu.vibration_level + VibrationCount - 1) % VibrationCount);
                    else if (m->settings_focus == 2)
                        m->menu.difficulty = (Difficulty)((m->menu.difficulty + DifficultyCount - 1) % DifficultyCount);
                    else if (m->settings_focus == 3)
                        m->debug_hud = !m->debug_hud;
                    break;
                case InputKeyRight:
                    if (m->settings_focus == 0)
                        m->menu.sound_level = (SoundLevel)((m->menu.sound_level + 1) % SoundCount);
                    else if (m->settings_focus == 1)
                        m->menu.vibration_level = (VibrationLevel)((m->menu.vibration_level + 1) % VibrationCount);
                    else if (m->settings_focus == 2)
                        m->menu.difficulty = (Difficulty)((m->menu.difficulty + 1) % DifficultyCount);
                    else if (m->settings_focus == 3)
                        m->debug_hud = !m->debug_hud;
                    break;
                case InputKeyOk:
                    if (m->settings_focus == 0)
                        m->menu.sound_level = (SoundLevel)((m->menu.sound_level + 1) % SoundCount);
                    else if (m->settings_focus == 1)
                        m->menu.vibration_level = (VibrationLevel)((m->menu.vibration_level + 1) % VibrationCount);
                    else if (m->settings_focus == 3)
                        m->debug_hud = !m->debug_hud;
                    break;
                case InputKeyBack:
                    set_screen(app, ScreenMenu);
                    break;
                default:
                    break;
            }
            /* Only touch the SD card when a value actually changed. */
            if(m->menu.sound_level != prev_sound || m->menu.vibration_level != prev_vibro ||
               m->menu.difficulty != prev_diff || m->debug_hud != prev_debug) {
                settings_save(&app->model);
            }
            break;
        }
        case ScreenInfo:
        case ScreenScore:
            if(ev->key == InputKeyBack &&
               (ev->type == InputTypeShort || ev->type == InputTypeLong)) {
                set_screen(app, ScreenMenu);
            }
            break;
        case ScreenGameComplete:
            if(ev->type == InputTypeShort &&
               (ev->key == InputKeyOk || ev->key == InputKeyBack)) {
                set_screen(app, ScreenMenu);
            }
            break;
    }
}

static void handle_tick(App* app, float dt) {
    if(app->model.screen == ScreenGame || app->model.screen == ScreenTutorial) {
        AppModel* m = &app->model;
        /* Kept up to date here because game_init's memset clears it on
         * every retry and level change. */
        m->game.vgm_missing = m->vgm && !vgm_tilt_present(m->vgm);
        if(m->tutorial_popup_showing) return;
        if(m->vgm && vgm_tilt_present(m->vgm)) {
            m->game.tilt_pitch = vgm_tilt_roll(m->vgm);
            m->game.tilt_roll  = vgm_tilt_pitch(m->vgm);
            if(m->game.needs_tilt_cal) {
                m->game.tilt_pitch_offset = m->game.tilt_pitch;
                m->game.tilt_roll_offset  = m->game.tilt_roll;
                m->game.needs_tilt_cal    = false;
            }
        }
        game_tick(&m->game, m->menu.thrust_mode, dt);
        game_audio_update(&m->game, m->menu.thrust_mode,
                          m->menu.sound_level, m->menu.vibration_level);

        /* Debug metrics — always updated so the overlay is current even if
         * logging is off. Serial log fires at 6 Hz to stay readable. */
        m->debug_tick_count++;
        uint32_t qd = furi_message_queue_get_count(app->queue);
        if((uint8_t)qd > m->debug_peak_queue) m->debug_peak_queue = (uint8_t)qd;
        m->debug_queue_depth = (uint8_t)(qd < 255 ? qd : 255);
        m->debug_free_heap   = (uint32_t)memmgr_get_free_heap();
        if(m->debug_hud && (m->debug_tick_count % 10 == 0)) {
            int deg = (int)(m->game.angle * (180.0f / 3.14159265f));
            FURI_LOG_I("LunarDbg", "HP:%lu TK:%lu Q:%u DR:%luus TG:%lums A:%d X:%d Y:%d",
                       (unsigned long)m->debug_free_heap,
                       (unsigned long)m->debug_tick_count,
                       (unsigned)m->debug_queue_depth,
                       (unsigned long)m->debug_draw_us,
                       (unsigned long)m->debug_gap_ms,
                       deg, (int)m->game.vx, (int)m->game.vy);
        }
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
    settings_load(&app->model);
    app->model.screen = ScreenMenu;
    app->model.should_exit = false;
    high_score_load(&app->model.high_score);

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    uint32_t tick_freq = furi_kernel_get_tick_frequency();
    app->last_tick_ms = furi_get_tick();

    while (!app->model.should_exit) {
        AppEvent ev;
        if (furi_message_queue_get(app->queue, &ev, FuriWaitForever) != FuriStatusOk) continue;
        /* Out of the queue, so let the timer post the next tick, even if we
         * now wait for the mutex while a frame draws. */
        if (ev.type == AppEventTick) app->tick_pending = false;

        furi_mutex_acquire(app->mutex, FuriWaitForever);

        if (ev.type == AppEventInput) {
            handle_input_event(app, &ev.input);
        } else { // AppEventTick
            uint32_t now = furi_get_tick();
            uint32_t gap_ms = (now - app->last_tick_ms) * 1000 / tick_freq;
            AppModel* m = &app->model;
            if (gap_ms > m->debug_gap_ms_max) m->debug_gap_ms_max = gap_ms;
            /* Roll the debug window over each wall-clock second. Done here
             * rather than in handle_tick so it also runs while the tutorial
             * popup pauses the game. */
            if (now / tick_freq != app->last_tick_ms / tick_freq) {
                m->debug_draw_us     = m->debug_draw_us_max;
                m->debug_gap_ms      = m->debug_gap_ms_max;
                m->debug_draw_us_max = 0;
                m->debug_gap_ms_max  = 0;
            }
            float dt = (float)(now - app->last_tick_ms) / (float)tick_freq;
            app->last_tick_ms = now;
            /* Guard against giant dt after a stall. */
            if (dt > 0.1f) dt = 0.1f;
            handle_tick(app, dt);
        }

        furi_mutex_release(app->mutex);
        /* Off the game screens, redraw after every event. In-game, redraw only
         * on ticks, capped by FRAME_MS (STILL_FRAME_MS while a banner or the
         * tutorial popup is up); a key event shows up on the next frame. */
        bool live = (app->model.screen == ScreenGame || app->model.screen == ScreenTutorial);
        if (!live) {
            view_port_update(app->view_port);
        } else if (ev.type == AppEventTick) {
            bool still = app->model.tutorial_popup_showing ||
                         app->model.game.status != GameStatusFlying;
            uint32_t now = furi_get_tick();
            uint32_t min_gap = (still ? STILL_FRAME_MS : FRAME_MS) * tick_freq / 1000;
            if (now - app->last_draw_tick >= min_gap) {
                app->last_draw_tick = now;
                view_port_update(app->view_port);
            }
        }
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
