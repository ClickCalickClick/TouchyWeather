#pragma once
#include <pebble.h>

// Phase 4: bottom-sheet detail modal. A single reusable overlay layer that
// slides up from the bottom (mirroring refresh_sheet.c's mechanics) to show a
// deeper view of the current forecast card — vector charts only, no bitmaps.
// One instance, reused; never stacks. Mutually exclusive with the refresh sheet.

typedef enum {
  DETAIL_NONE = 0,
  DETAIL_HOURS,   // 6 Hours → temperature trend (SELECT toggles POP overlay)
  DETAIL_PRECIP,  // Precipitation → hourly rainfall amounts
  DETAIL_WEEK,    // Week → one page per day (UP/DOWN page days)
} DetailType;

void detail_modal_init(Window *window);
void detail_modal_deinit(void);

// True whenever the sheet is non-idle (opening / open / closing). Input
// handlers use this to route to the modal and lock out card navigation.
bool detail_modal_is_active(void);

// Open the sheet for `type`. Returns true if it opened (false if type is
// DETAIL_NONE or an overlay is already active). Refuses while the refresh
// sheet is up (one overlay at a time).
bool detail_modal_open(DetailType type);

// Begin the slide-down dismiss. Safe to call in any state.
void detail_modal_close(void);

// Input hooks (call only while active). Each returns true if consumed.
bool detail_modal_handle_back(void);    // dismiss
bool detail_modal_handle_up(void);      // scroll / page
bool detail_modal_handle_down(void);
bool detail_modal_handle_select(void);  // toggle secondary overlay
