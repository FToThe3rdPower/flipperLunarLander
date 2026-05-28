/*
 * VGM IMU wrapper — implementation via the flipperdevices/flipperzero-game-engine.
 *
 * Build dependency: the game engine must be available as a library or its
 * source files included alongside this app.  The relevant header is:
 *   <sensors/imu.h>
 * which provides Imu*, imu_alloc(), imu_free(), imu_present(),
 * imu_pitch_get(), imu_roll_get().
 */

#include "vgm_tilt.h"
#include "sensors/imu.h"
#include <stdlib.h>

struct VgmTilt {
    Imu* imu;
    bool present;
};

VgmTilt* vgm_tilt_alloc(void) {
    VgmTilt* v = malloc(sizeof(VgmTilt));
    v->imu     = imu_alloc();
    v->present = imu_present(v->imu);
    return v;
}

void vgm_tilt_free(VgmTilt* v) {
    if(v) {
        imu_free(v->imu);
        free(v);
    }
}

bool vgm_tilt_present(const VgmTilt* v) {
    return v && v->present;
}

float vgm_tilt_pitch(VgmTilt* v) {
    return (v && v->present) ? imu_pitch_get(v->imu) : 0.0f;
}

float vgm_tilt_roll(VgmTilt* v) {
    return (v && v->present) ? imu_roll_get(v->imu) : 0.0f;
}
