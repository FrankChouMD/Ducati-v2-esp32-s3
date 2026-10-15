#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { DUCATI_BOOT_ANIMATION_DURATION_MS = 2400 };

/* Call with the LVGL lock held, before the navigation UI has been created.
 * Starts on a black frame; the LVGL worker drives the animation thereafter.
 * Deleting the screen's children cancels the owned animation timer. */
void ducati_boot_animation_show(void);

#ifdef __cplusplus
}
#endif
