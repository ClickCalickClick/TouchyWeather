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

// --- Radar platform carve-out (Phase 5) ---
// The radar card needs ~51KB of peak heap (a 25.6KB staging buffer plus a
// 25.6KB GBitmap coexist while assembling a frame). Only the 128KB App RAM
// platforms — emery and gabbro — can spare it; a measured basalt build has
// just 14KB free heap, so the buffer malloc would fail. B&W platforms can't
// use a color radar image anyway. On every non-128KB platform we therefore
// carve radar out of the carousel (settings_get_effective_enabled forces it
// off, so nav skips it exactly like a user-disabled card, and the card is
// never navigated to -> its buffer is never allocated -> no OOM).
//
// TOGGLE_RADAR keeps its id and persist key (KEY_TOGGLE_BASE + 8) on ALL
// platforms so a user's settings survive if their data roams across watches.
#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
#  define TW_RADAR_SUPPORTED 1
#endif

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

// Big Mode (Phase 3.1 Stage B): accessibility mode for reduced eyesight. When
// on, the ui_font_* accessors return larger fonts, several cards switch to a
// simplified "fewer, bigger elements" layout, and theme.c forces high-contrast
// colors (muted/secondary grays collapse to full-contrast fg). Default false —
// the "Normal" look is pixel-identical until the user opts in. Unlike the
// compile-time screen-class axis, this is a RUNTIME toggle read on the draw
// path, so it repaints live when changed. Persisted (key 207).
bool settings_get_big_mode(void);
void settings_set_big_mode(bool enabled);

// Opt-in: when true, the phone (Clay) controls per-card visibility — incoming
// Clay CardEnabled* toggles are applied to the on-watch enable flags. Default
// false, so on-watch card management is unaffected unless the user opts in
// (this avoids a Clay save silently wiping carefully-curated on-watch config).
// Card REORDER always stays on-watch regardless. Persisted.
bool settings_get_phone_manages_cards(void);
void settings_set_phone_manages_cards(bool enabled);

// Opt-in (Phase 3.2): auto-hide the Precipitation + Radar cards when no rain is
// expected soon, reclaiming carousel space when it's dry. Default off. Persisted.
bool settings_get_auto_hide_precip(void);
void settings_set_auto_hide_precip(bool enabled);

// Runtime auto-hidden state, tracked SEPARATELY from the persisted user enable
// flags so auto-hide never mutates the user's preference. Effective visibility
// = user_enabled AND NOT auto_hidden (see settings_get_effective_enabled).
// Not persisted — recomputed from weather data each launch/update.
bool settings_get_auto_hidden(ToggleId id);
void settings_set_auto_hidden(ToggleId id, bool hidden);

// Effective card visibility fed to nav: the user's enable flag AND not
// auto-hidden. This is what the traversal/indicator should reflect.
bool settings_get_effective_enabled(ToggleId id);

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

// Apply a full visual order from a CSV of ToggleIds ("9,0,1,…" — the format
// the Clay card-order control sends and the watch's state seed pushes).
// Strictly validated: exactly SETTINGS_TOGGLEABLE_COUNT entries forming a
// permutation, no junk. Persists on change. Returns true only when the order
// was valid AND different (callers re-sync the nav traversal on true).
bool settings_apply_order_csv(const char *csv);

// Visible toggleable rows on the Settings card. On radar-capable platforms
// this is identical to the full toggleable set. On carved-out platforms the
// radar row is filtered out of the visible list (radar is force-disabled
// there, so an on-screen RADAR checkbox would be inert) — the cursor, reorder
// and row draw all operate in this radar-free "visible" space. The underlying
// size-10 arrays and persist keys stay full on every platform so a user's
// settings survive roaming across watches.
int settings_visible_count(void);
ToggleId settings_visible_id(int visible_pos);

// Card + input handlers select the visible view on carved-out platforms and
// the verbatim full view where radar is supported. The radar-capable branch
// expands to the exact original tokens so emery/gabbro compile byte-identical.
#if defined(TW_RADAR_SUPPORTED)
#  define SETTINGS_VIS_COUNT SETTINGS_TOGGLEABLE_COUNT
#  define SETTINGS_VIS_ID(i) settings_visual_id(i)
#else
#  define SETTINGS_VIS_COUNT settings_visible_count()
#  define SETTINGS_VIS_ID(i) settings_visible_id(i)
#endif
