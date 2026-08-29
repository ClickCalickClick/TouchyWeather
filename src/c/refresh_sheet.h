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

// Open the sheet automatically on a COLD START — a launch that found no
// cached reading, where the alternative is a static NO DATA YET panel for
// however long the first payload takes. Runs the same OPENING -> LOADING ->
// close cycle, with three differences:
//   * it fires no fetch of its own (comm_init()'s 750ms launch request
//     already owns the first fetch; a second would double the PKJS work),
//   * a timeout slides the sheet away silently instead of accusing the phone
//     of failing, because a cold BLE wake plus three API calls legitimately
//     outruns the manual-refresh timeout, and
//   * any button or tap dismisses it, so a slow phone can never hold the
//     user's input hostage.
void refresh_sheet_show_cold_start(void);

// Input gate for the button/tap handlers: true when the sheet owns the
// input and the caller must not act on it. Replaces a bare
// refresh_sheet_is_active() check at those sites because a cold-start sheet
// is dismissable — asking whether the sheet consumes the press is also what
// tells the sheet to get out of the way.
bool refresh_sheet_consume_input(void);