#pragma once
#include <pebble.h>

// One-time "What's New" modal. Shows the release notes for the current build
// (generated from CHANGELOG.md into version_gen.h) the first time the user
// launches a newer version, then never again until the next update. Safe to
// call once during normal-launch init, after the main window is pushed.
void update_notes_maybe_show(void);

// Destroy the modal window if it was created. Call during app deinit.
void update_notes_deinit(void);

// Touch hooks (mirrors refresh_sheet_on_*). While the What's New screen is
// showing, a vertical drag scrolls the notes. Each returns true if the screen
// consumed the event, so the global touch handler skips its own nav/tap/refresh.
bool update_notes_on_touchdown(int16_t x, int16_t y);
bool update_notes_on_move(int16_t x, int16_t y);
bool update_notes_on_liftoff(int16_t x, int16_t y);
