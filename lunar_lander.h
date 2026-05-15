#pragma once

#include <stdint.h>
#include <stdbool.h>

#define SCREEN_W 128
#define SCREEN_H 64

/* Chosen in the menu, used by the game. */
typedef enum {
    ThrustModeBinary = 0,   // hold UP = full thrust, release = off
    ThrustModeTapImpulse,   // each tap = one fixed impulse
    ThrustModeRamp,         // hold UP, thrust ramps up over time
    ThrustModeCount,
} ThrustMode;

extern const char* const thrust_mode_label[ThrustModeCount];
extern const char* const thrust_mode_desc[ThrustModeCount];

typedef enum {
    ScreenMenu = 0,
    ScreenGame,
    ScreenTutorial,
} Screen;
