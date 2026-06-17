#pragma once
#include <pebble.h>

// One-time "What's New" modal. Shows the release notes for the current build
// (generated from CHANGELOG.md into version_gen.h) the first time the user
// launches a newer version, then never again until the next update. Safe to
// call once during normal-launch init, after the main window is pushed.
void update_notes_maybe_show(void);

// Destroy the modal window if it was created. Call during app deinit.
void update_notes_deinit(void);
