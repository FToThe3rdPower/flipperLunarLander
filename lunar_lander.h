#pragma once

#include <stdint.h>
#include <stdbool.h>

#define SCREEN_W 128
#define SCREEN_H 64

/* Chosen in the menu, used by the game. */
typedef enum {
    ThrustModeTapImpulse = 0,   // each tap = one fixed impulse
    ThrustModeBinary,           // hold UP = full thrust, release = off
    ThrustModeRamp,             // hold UP, thrust ramps up over time
    ThrustModeVidyaTap,         // VGM tilt steering + tap impulse
    ThrustModeVidyaBinary,      // VGM tilt steering + binary (hold UP) thrust
    ThrustModeVidyaRamp,        // VGM tilt steering + ramped (hold UP) thrust
    ThrustModeVidyaFull,        // VGM full tilt: tilt steers AND fires thrusters
    ThrustModeCount,
} ThrustMode;

extern const char* const thrust_mode_label[ThrustModeCount];
extern const char* const thrust_mode_desc[ThrustModeCount];

/* Fuel modes — affects starting fuel and whether fuel refills between levels.
 * "Full" is the classic arcade behavior. The no-refuel modes give you a fixed
 * total tank for the entire game; crashing rewinds to that level's starting
 * fuel rather than draining what's left. */
typedef enum {
    FuelModeFull = 0,    // 100 fuel, refilled every level
    FuelModeEasy,        // 500 fuel total, no top-up
    FuelModeMed,         // 350 fuel total, no top-up
    FuelModeHard,        // 200 fuel total, no top-up
    FuelModeCount,
} FuelMode;

extern const char* const fuel_mode_label[FuelModeCount];
extern const char* const fuel_mode_desc[FuelModeCount];
extern const int        fuel_mode_starting[FuelModeCount];

typedef enum {
    ScreenMenu = 0,
    ScreenGame,
    ScreenTutorial,
    ScreenInfo,
    ScreenSettings,
} Screen;

typedef enum {
    DifficultyEasy = 0,  // safe thresholds doubled
    DifficultyMedium,    // baseline (original values)
    DifficultyHard,      // safe thresholds halved
    DifficultyRealistic, // Vy<1, Vx<1, angle<3°
    DifficultyCount,
} Difficulty;

extern const char* const difficulty_label[DifficultyCount];
