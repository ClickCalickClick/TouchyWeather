#pragma once
#include <pebble.h>

// Persisted user preferences. Card visibility + theme survive
// across app close/reopen via persist_*.
//
// The first card (index 0 = Main) is PERMANENT. It is not represented
// in this module; callers should always treat it as enabled.
//
// The Settings card itself (last in the carousel) is also always
// enabled — there'd be no escape hatch otherwise. It is NOT in the
// toggleable list.

// Number of cards that can be toggled on the Settings screen.
#define SETTINGS_TOGGLEABLE_COUNT 10

// Toggleable card identifiers (must match the registration order in
// TouchWeather.c skipping Main and Settings).
//
// TOGGLE_ADVICE was added last (rather than at the top of the enum)
// so existing persisted toggle values at KEY_TOGGLE_BASE + 0..8 keep
// their meaning across upgrades. Visual order in the settings card
// follows this enum order.
typedef enum {
  TOGGLE_HOURS = 0,
  TOGGLE_WEEK,
  TOGGLE_PRECIP,
  TOGGLE_UV,
  TOGGLE_AQ,
  TOGGLE_SUN,
  TOGGLE_NIGHT,
  TOGGLE_GOLDEN,
  TOGGLE_RADAR,
  TOGGLE_ADVICE,
} ToggleId;

void settings_load(void);

// Card-navigation loop preference. When true (default), pressing UP on the
// first card or DOWN on the last card wraps around the carousel. When false,
// reaching either boundary and stepping past it exits the app (Quick Launch
// style). Persisted across launches.
bool settings_get_loop_nav(void);
void settings_set_loop_nav(bool loop);

// Background update interval in seconds. 0 = disabled, 1800 = 30 mins, 3600 = 1 hour.
// Default is 3600 (1 hour). Persisted across launches.
int settings_get_background_interval(void);
void settings_set_background_interval(int interval_secs);

// Decorative animation master switch (hero icon, rotating banners, settings
// footer hint). Default true. When false, decorative animation never runs —
// the hero icon renders a single static frame. The refresh-sheet spinner is
// unaffected (it's functional feedback, not decorative). Persisted.
bool settings_get_animations_enabled(void);
void settings_set_animations_enabled(bool enabled);

// Whether a short/long SELECT press toggles light/dark theme on ordinary
// cards. Default true (preserves the reflexive-press behavior). When false,
// SELECT no longer flips the theme on ordinary cards (theme stays reachable
// via Clay); the Main-card refresh, Radar refresh, and Settings row-toggle
// are unaffected. Persisted.
bool settings_get_select_toggles_theme(void);
void settings_set_select_toggles_theme(bool enabled);

// Opt-in: when true, the phone (Clay) controls per-card visibility — incoming
// Clay CardEnabled* toggles are applied to the on-watch enable flags. Default
// false, so on-watch card management is unaffected unless the user opts in
// (this avoids a Clay save silently wiping carefully-curated on-watch config).
// Card REORDER always stays on-watch regardless. Persisted.
bool settings_get_phone_manages_cards(void);
void settings_set_phone_manages_cards(bool enabled);

bool settings_get_enabled(ToggleId id);
void settings_set_enabled(ToggleId id, bool enabled);

// Returns the display label for a toggleable card.
const char *settings_label(ToggleId id);

// Cursor on the Settings screen. Wraps within toggleable rows.
int  settings_cursor(void);
void settings_cursor_advance(void);

// Maps a visual row position (0 = first toggleable row) to the
// ToggleId that should be drawn/toggled there. Decouples the on-screen
// order from the enum order (which is fixed for persistence compat).
ToggleId settings_visual_id(int visual_pos);

// Reordering. Swaps the row at visual_pos with its neighbor and
// persists the new order. Clamps at the ends (no wrap). The cursor
// follows the moved row so the user can chain holds. Returns true if
// a swap occurred.
bool settings_move_up(int visual_pos);
bool settings_move_down(int visual_pos);
