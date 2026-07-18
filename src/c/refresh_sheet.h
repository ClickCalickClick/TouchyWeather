#pragma once
#include <pebble.h>

// Pull-to-refresh sheet. Owns its own overlay layer above the nav layers.
// Drives the touch-driven open animation, the loading state with cycling
// status phrases, and the slide-up close animation when fresh data lands
// (or the safety timeout fires).

typedef enum {
  REFRESH_IDLE = 0,
  REFRESH_TRACKING,   // finger down, sheet rubber-banding with finger
  REFRESH_OPENING,    // finger released past threshold, sheet snapping open
  REFRESH_LOADING,    // sheet fully open, waiting for inbox / timeout
  REFRESH_CLOSING,    // sliding back up
} RefreshState;

void refresh_sheet_init(Window *window);
void refresh_sheet_deinit(void);

// Touch handler hooks. Each returns true if the sheet consumed the event
// (in which case the caller should skip its own swipe/tap logic).
bool refresh_sheet_on_touchdown(int16_t x, int16_t y);
bool refresh_sheet_on_move(int16_t x, int16_t y);
bool refresh_sheet_on_liftoff(int16_t x, int16_t y);

// Notify the sheet that fresh data arrived from PKJS. Safe to call in any
// state — no-op unless the sheet is in LOADING.
void refresh_sheet_on_data_received(void);

// True whenever the sheet is non-idle. Callers (button handlers, swipe
// fallthrough) use this to lock out other input while the sheet is open
// or animating.
bool refresh_sheet_is_active(void);

// Open the sheet programmatically (no touch), running the full
// OPENING → LOADING → close feedback cycle and kicking off a weather
// refresh. Used by the button-triggered refresh (SELECT on the Main card)
// so non-touch platforms have a manual-refresh path. No-op if a sheet is
// already active.
void refresh_sheet_show_programmatic(void);