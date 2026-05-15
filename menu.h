#pragma once

#include <gui/gui.h>
#include <input/input.h>
#include "lunar_lander.h"

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
    MenuActionNone = 0,
    MenuActionStart,
    MenuActionTutorial,
    MenuActionExit,
} MenuAction;

typedef struct {
    MenuRow row;
    MenuBtn btn;
    ThrustMode thrust_mode;
} MenuState;

void menu_init(MenuState* m);
MenuAction menu_input(MenuState* m, const InputEvent* ev);
void menu_draw(Canvas* canvas, const MenuState* m);
