#pragma once
#include <pebble.h>

// --- Screen-class axis (Phase 5) ---
//
// The second axis the Stage A accessors branch on (scale/Big Mode is the
// third, added later). Determined ENTIRELY at compile time from the SDK's
// own per-platform defines (PBL_ROUND + PBL_DISPLAY_WIDTH), so it costs
// nothing at runtime and matches the existing PBL_IF_ROUND_ELSE idiom.
//
// Four classes, per the master report:
//   UI_SCREEN_SMALL_RECT   144x168  aplite / basalt / diorite / flint
//   UI_SCREEN_SMALL_ROUND  180x180  chalk            (genuinely new class)
//   UI_SCREEN_LARGE_RECT   200x228  emery            (the old "rect" values)
//   UI_SCREEN_LARGE_ROUND  260x260  gabbro           (the old "round" values)
//
// Exactly one of these is defined per build. emery keeps LARGE_RECT and
// gabbro keeps LARGE_ROUND, so as long as those two branches reproduce the
// pre-Phase-5 PBL_IF_ROUND_ELSE values, emery+gabbro stay pixel-identical
// by construction — only the two NEW small classes get new values.
#if defined(PBL_ROUND)
#  if PBL_DISPLAY_WIDTH <= 200   // chalk 180 (large-round gabbro is 260)
#    define UI_SCREEN_SMALL_ROUND 1
#  else
#    define UI_SCREEN_LARGE_ROUND 1
#  endif
#else
#  if PBL_DISPLAY_WIDTH <= 144   // aplite/basalt/diorite/flint
#    define UI_SCREEN_SMALL_RECT 1
#  else                          // emery 200
#    define UI_SCREEN_LARGE_RECT 1
#  endif
#endif

// --- Layout metric accessors (Phase 3.1 Stage A) ---
//
// The shared layout constants below now route through accessor functions
// (defined in ui.c) so Big Mode (Stage B) and the per-screen-class layout
// work (Phase 5) have ONE place to scale margins/header geometry. The
// UI_* macros are kept as zero-cost aliases so no call site had to change
// — every card still reads UI_MARGIN_X / UI_HEADER_Y / UI_HEADER_HEIGHT
// and gets the identical value it did before. The current values live in
// the function bodies and are the protected "Normal" layout.

// Symmetric horizontal margin used by every card so left and right
// padding always match. Tighter on rectangular displays where bezel
// is smaller, looser on round to avoid the corner curve. [round 20 / rect 12]
int ui_margin_x(void);
// Standardized header y-offset used by all cards. Tight on rect
// (8px from top) to reclaim vertical space; round needs more clearance
// for the bezel curve. [round 24 / rect 8]
int ui_header_y(void);
// Standardized header band height used by all cards. [24]
int ui_header_height(void);

#define UI_MARGIN_X (ui_margin_x())
#define UI_HEADER_Y (ui_header_y())
#define UI_HEADER_HEIGHT (ui_header_height())

// --- Font role accessors (Phase 3.1 Stage A) ---
//
// Cards fetch their fonts through these accessors instead of calling
// fonts_get_system_font(FONT_KEY_...) directly. There is exactly one
// accessor per distinct font the cards use today, so migrating a call
// site is a pure 1:1 rename with no behaviour change.
//
// The point of the indirection is future scaling: Big Mode (Stage B) and
// the per-screen-class layout work (Phase 5) can swap sizes in ONE place
// — the body of each accessor — without touching a single call site. Today
// every accessor returns the exact system font the cards already used, so
// this layer is a verified no-op (screenshots stay pixel-identical). The
// FONT_KEY each one wraps is named in the comment; that mapping is the
// current, protected "Normal" look and must never regress.
GFont ui_font_header(void);   // GOTHIC_18_BOLD  — card titles + primary values
GFont ui_font_body(void);     // GOTHIC_24_BOLD  — large body copy (FEELS, quips)
GFont ui_font_title(void);    // GOTHIC_28_BOLD  — hero titles (clock)
GFont ui_font_label(void);    // GOTHIC_14_BOLD  — small bold labels / badges
GFont ui_font_caption(void);  // GOTHIC_14       — muted captions / subtitles
GFont ui_font_number(void);   // LECO_42_NUMBERS — hero numerals (temp, UV)

// --- Status-pill geometry (one source of truth) ---
//
// Three files used to carry three different, mutually inconsistent models of
// where the bottom status pill sits: ui.c's real geometry (pad 18/20/35),
// week.c's `PBL_IF_ROUND_ELSE(40, 22)` and advice.c's
// `PBL_IF_ROUND_ELSE(70, 56)`. Cards clamped their content to their own guess
// and then drew the pill ON TOP, which is the direct cause of the Golden Hour
// 4th row, Night Sky's "72% LIT" and the Advice quip all disappearing under it.
//
// These accessors mirror ui_draw_status_banner()'s geometry exactly, so a card
// can reserve the space instead of guessing at it.
//
// SMALL CLASSES ONLY. week.c's and advice.c's private constants are also wrong
// on emery and gabbro (off by 5 px and 2 px), but those two are locked — their
// shipped layout must not move — so they keep the wrong-but-shipped
// expressions verbatim and never call these. See tools/lock_guard.py.
#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
// Topmost y the status pill occupies, in absolute (bounds-relative) coords.
int ui_status_pill_top(GRect bounds);
// Lowest y a card may draw content at without colliding with the pill.
int ui_content_bottom(GRect bounds);
#endif

typedef enum {
  STATUS_BANNER_RAIN = 0,    // "RAIN IN <m>M" (orange)
  STATUS_BANNER_UPDATED = 1, // "UPDATED <x> AGO" (muted)
} StatusBannerMode;

// Draws either a rain alert or a "last updated" pill at the bottom of
// `bounds`, reusing the same pill geometry. Returns true if drawn.
// minutes_to_rain: -1 for "no rain alert"; ignored when mode != RAIN.
// last_updated_secs: unix seconds of last refresh; 0 means "never".
bool ui_draw_status_banner(GContext *ctx, GRect bounds,
                           StatusBannerMode mode,
                           int minutes_to_rain,
                           uint32_t last_updated_secs);

// Convenience: pick a banner mode automatically given the data and
// current animation frame. If a rain alert is present we alternate
// every ~4s between rain and updated; otherwise we always show updated.
// frame is anim_get_frame() (~10 Hz).
bool ui_draw_auto_banner(GContext *ctx, GRect bounds,
                         int minutes_to_rain,
                         uint32_t last_updated_secs,
                         uint32_t frame);

// Centered uppercase header label. GOTHIC_18_BOLD across all cards.
void ui_draw_header(GContext *ctx, GRect bounds, const char *text,
                    GColor color, int y);

// Header with a small leading icon (cloud-rain, sun, pulse, ...).
// `draw_icon` receives the icon center point and `icon_size`.
typedef void (*UIIconDrawFn)(GContext *ctx, GPoint center, int size,
                             GColor color);
void ui_draw_card_header_with_icon(GContext *ctx, GRect bounds,
                                   const char *label, GColor color,
                                   int y, int icon_size,
                                   UIIconDrawFn draw_icon);

// The shared "we have no reading yet" panel: a centered NO DATA YET / WAITING
// FOR PHONE pair, drawn in the band a card's content would occupy, between the
// header row and the status pill.
//
// Callers are nav.c's prv_draw_card() — which substitutes this for ANY card
// while weather_data_has_reading() is false — and detail_modal.c, for all five
// sheets. Cards do not call it themselves; see the note in prv_draw_card() for
// why the guard is central (RAM) rather than one copy per card. A card that
// instead printed its zeroed fields would be inventing a forecast: a 0° week,
// "GOOD" air quality, a new moon.
void ui_draw_awaiting_data(GContext *ctx, GRect bounds);

void ui_draw_dotted_hline(GContext *ctx, int x1, int x2, int y, GColor color);

