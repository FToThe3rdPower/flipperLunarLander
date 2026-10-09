#pragma once
/*
 * VGM tone channel, version 1.
 *
 * A game hides its current tone in the bottom row of its 128x64 screen, so
 * a Video Game Module running the VGM480 firmware can play it over HDMI.
 * The same header is used by the game (to draw the code) and by the module
 * firmware (to read it), so keep both copies identical.
 *
 * Bottom row (y = 63):
 *   x = 0..115    reference: the game keeps these a solid colour. A frame
 *                 counts only if at least 105 of them match.
 *   x = 116..127  code: pixels that differ from the reference colour.
 *                 None differ  -> silence.
 *                 Exactly two  -> a tone. Their positions a < b give an
 *                                 index 0..65, k = index + 1, f = 20 * k Hz.
 *                 Otherwise    -> not a tone-channel frame.
 *
 * The reference is normally black (ground). A white reference is reported
 * as inverted, e.g. during a crash flash; the module only trusts it right
 * after normal frames.
 *
 * Frame buffer layout (as streamed by the Flipper): byte (y / 8) * 128 + x,
 * bit y % 8, set = black pixel.
 */
#include <stdbool.h>
#include <stdint.h>

#define VTC_ROW        63
#define VTC_CODE_X0    116
#define VTC_CODE_W     12
#define VTC_REF_W      VTC_CODE_X0
#define VTC_REF_MATCH  105
#define VTC_HZ_STEP    20
#define VTC_CODES      66   /* C(12, 2) */

typedef enum {
    VtcInvalid = 0, /* not a tone-channel frame */
    VtcSilent,
    VtcTone,
} VtcKind;

typedef struct {
    VtcKind  kind;
    bool     inverted; /* reference row was white */
    uint16_t hz;       /* VtcTone only */
} VtcResult;

/* The two code pixels (offsets 0..11 from VTC_CODE_X0) for a tone.
 * Returns false for silence (hz == 0). Out-of-range pitches clamp. */
static inline bool vtc_encode(uint32_t hz, uint8_t* a, uint8_t* b) {
    if(hz == 0) return false;
    uint32_t k = (hz + VTC_HZ_STEP / 2) / VTC_HZ_STEP;
    if(k < 1) k = 1;
    if(k > VTC_CODES) k = VTC_CODES;
    uint32_t index = k - 1;
    for(uint8_t i = 0; i < VTC_CODE_W - 1; i++) {
        uint32_t row = (uint32_t)(VTC_CODE_W - 1 - i); /* pairs starting at i */
        if(index < row) {
            *a = i;
            *b = (uint8_t)(i + 1 + index);
            return true;
        }
        index -= row;
    }
    return false; /* unreachable */
}

static inline bool vtc_pixel(const uint8_t* fb, int x, int y) {
    return (fb[(y / 8) * 128 + x] >> (y % 8)) & 1;
}

static inline VtcResult vtc_decode(const uint8_t* fb) {
    VtcResult r = {VtcInvalid, false, 0};
    int black = 0;
    for(int x = 0; x < VTC_REF_W; x++) black += vtc_pixel(fb, x, VTC_ROW);
    bool ref_black;
    if(black >= VTC_REF_MATCH) {
        ref_black = true;
    } else if(VTC_REF_W - black >= VTC_REF_MATCH) {
        ref_black = false;
        r.inverted = true;
    } else {
        return r;
    }
    int found = 0;
    uint8_t pos[2] = {0, 0};
    for(int i = 0; i < VTC_CODE_W; i++) {
        if(vtc_pixel(fb, VTC_CODE_X0 + i, VTC_ROW) != ref_black) {
            if(found < 2) pos[found] = (uint8_t)i;
            found++;
        }
    }
    if(found == 0) {
        r.kind = VtcSilent;
    } else if(found == 2) {
        uint32_t index = 0;
        for(uint8_t i = 0; i < pos[0]; i++) index += (uint32_t)(VTC_CODE_W - 1 - i);
        index += (uint32_t)(pos[1] - pos[0] - 1);
        r.kind = VtcTone;
        r.hz = (uint16_t)((index + 1) * VTC_HZ_STEP);
    }
    return r;
}
