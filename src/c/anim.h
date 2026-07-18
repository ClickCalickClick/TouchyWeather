#pragma once
#include <pebble.h>

void anim_init(void);
void anim_deinit(void);

// Reset the decorative-animation idle deadline and ensure the ticker is
// running. Call on any user/data activity (button, touch, nav, data arrival,
// refresh-sheet open) so decorative animation resumes for another window and
// then re-settles. Cheap and idempotent.
void anim_kick(void);

uint32_t anim_get_frame(void);
