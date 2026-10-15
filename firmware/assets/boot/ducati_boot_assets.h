/* Generated assets. Source and attribution: README.md in this directory. */
#pragma once
#include "lvgl.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define DUCATI_BOOT_MODEL_TEXT "PANIGALE V2S"
#define DUCATI_BOOT_GLYPH_COUNT 11

typedef struct { const lv_image_dsc_t *image; int16_t x; int16_t y; char character; } ducati_boot_glyph_t;
typedef struct { float x; float y; float distance; } ducati_boot_outline_point_t;

extern const lv_image_dsc_t ducati_boot_shield_base;
extern const lv_image_dsc_t ducati_boot_wordmark;
extern const lv_image_dsc_t ducati_boot_letter_0;
extern const lv_image_dsc_t ducati_boot_letter_1;
extern const lv_image_dsc_t ducati_boot_letter_2;
extern const lv_image_dsc_t ducati_boot_letter_3;
extern const lv_image_dsc_t ducati_boot_letter_4;
extern const lv_image_dsc_t ducati_boot_letter_5;
extern const lv_image_dsc_t ducati_boot_letter_6;
extern const lv_image_dsc_t ducati_boot_letter_7;
extern const lv_image_dsc_t ducati_boot_letter_8;
extern const lv_image_dsc_t ducati_boot_letter_9;
extern const lv_image_dsc_t ducati_boot_letter_10;
extern const int16_t ducati_boot_wordmark_x;
extern const int16_t ducati_boot_wordmark_y;
extern const ducati_boot_glyph_t ducati_boot_glyphs[DUCATI_BOOT_GLYPH_COUNT];
extern const ducati_boot_outline_point_t ducati_boot_outline[478];
extern const uint16_t ducati_boot_outline_count;

#ifdef __cplusplus
}
#endif
