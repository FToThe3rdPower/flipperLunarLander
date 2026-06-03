#pragma once

#include <gui/gui.h>
#include <input/input.h>
#include "lunar_lander.h"

/* Menu rows top-to-bottom. Title is at the top — Up from Thrust reaches it.
 * The title row has two focusable sub-elements (see MenuTitleSel). */
typedef enum {
    MenuRowTitle = 0,
    MenuRowThrust,
    MenuRowFuel,
    MenuRowButtons,
    MenuRowCount,
} MenuRow;

/* Sub-selection within the title row: lander sprite (score), title text
 * (about), gear (settings). Left/Right cycles left-to-right. */
typedef enum {
    MenuTitleSelScore = 0,
    MenuTitleSelAbout,
    MenuTitleSelSettings,
    MenuTitleSelCount,
} MenuTitleSel;

typedef enum {
    MenuBtnStart = 0,
    MenuBtnTutorial,
    MenuBtnCount,
} MenuBtn;

typedef enum {
    MenuActionNone = 0,
    MenuActionStart,
    MenuActionTutorial,
    MenuActionScore,
    MenuActionInfo,
    MenuActionSettings,
    MenuActionExit,
} MenuAction;

/* MenuState carries everything the menu owns, including user preferences
 * (modes + sound/vibration toggles) that persist across screen transitions. */
typedef struct {
    MenuRow      row;
    MenuBtn      btn;
    MenuTitleSel title_sel;
    ThrustMode   thrust_mode;
    FuelMode     fuel_mode;
    Difficulty   difficulty;
    bool         sound_on;
    bool         vibration_on;
} MenuState;

void menu_init(MenuState* m);
MenuAction menu_input(MenuState* m, const InputEvent* ev);
void menu_draw(Canvas* canvas, const MenuState* m);
