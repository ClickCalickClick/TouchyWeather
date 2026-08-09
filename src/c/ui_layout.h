#pragma once
#include <pebble.h>
#include "ui.h"

// --- Small-screen flow layout (ported from the TouchyWeather watchface) ---
//
// Cards used to place every row at a hard-coded y per screen class. On the two
// large classes there is enough vertical slack that this works; on the small
// ones it does not, and the failure modes were all the same shape:
//
//   * a row computed from the top and a pill anchored to the bottom collide in
//     the middle (Golden Hour's 4th row, Night Sky's "72% LIT", Advice's quip)
//   * turning a row off leaves a hole, so the stack goes top-heavy — which
//     main_card.c hand-patched with a `block_shift` fudge
//   * a horizontal cluster is centered, clamped on the LEFT only, and then
//     draws off the right edge (6 Hours' precip column, Week's pop column,
//     Settings' checkbox column)
//   * a full-width rect on chalk runs under the bezel, because nothing in the
//     app is chord-aware (the status pill, the page dots, the hi/lo column)
//
// This module solves all four by measuring rows and stacking them, instead of
// asserting where they go. It is a direct port of the watchface's
// face_layout.c, generalized from that file's fixed FLOW_ROW_* enum to a
// caller-supplied array so different cards can declare different row sets.
//
// SCOPE: this entire translation unit is compiled ONLY on the two small screen
// classes. emery (LARGE_RECT) and gabbro (LARGE_ROUND) are locked — they must
// keep their shipped layout byte-for-byte — so they compile an empty object
// file and every call site here must sit inside the same guard. See
// tools/lock_guard.py, which fails the build if their binaries move.
#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)

typedef struct {
  bool present;  // IN:  is this row drawn at all?
  int  h;        // IN:  measured natural height (prefer INK height, see below)
  int  y;        // OUT: absolute top edge
  int  cy;       // OUT: absolute vertical center
  int  x;        // OUT: absolute left edge of the usable width
  int  w;        // OUT: usable width (chord-clamped on chalk)
} UILayoutRow;

// Usable width of a horizontal band spanning `y`..`y+h`.
//
// Rect classes: full width minus the class inset.
// Round (chalk): the inscribed chord, so text and pills ellipsize instead of
// running under the bezel.
//
// `y` and `h` are SCREEN-absolute, and the chord is measured against the screen
// circle rather than against `bounds` — so a caller may pass any sub-band it
// likes (a card's content area, the strip above the pill) without the answer
// silently changing. `bounds` is read only for the rect classes' width. Card
// bounds are offset horizontally during the nav slide and never vertically, so
// a row's y is the same in both coordinate systems.
//
// Two details carried over from the watchface, both learned the hard way:
//
//   1. The chord is measured at the band's vertical CENTER, not at its farther
//      edge. Every row here is centered text or a centered pill, so its ink is
//      thickest across the middle and tapers at the corners; measuring at the
//      far edge is correct for a solid rectangle but far too pessimistic for
//      this content, and it clipped the watchface's clock into an ellipsis.
//   2. The result is floored at UI_LAYOUT_MIN_ROW_W. A row pushed to the very
//      edge of the glass would otherwise get a zero or negative width, and the
//      pill drawers derive their inner text rect by SUBTRACTING padding from
//      it — which goes negative and takes the app down. Overshooting the arc
//      slightly is the better failure.
#define UI_LAYOUT_MIN_ROW_W 48
int ui_band_w(GRect bounds, int y, int h);

// Stack the present rows and center them vertically in `avail`, then compute
// each row's usable width. `count` is the length of `rows`.
//
// Returns false when even minimum gaps overflow `avail` — the rows are still
// placed (top-aligned, minimum gaps) so a caller that cannot shrink further
// has a defined, non-clipping-at-the-top result to draw.
//
// `avail` should be the card's real content band: below the header ink, above
// ui_content_bottom(). Pass the band, not the whole screen.
//
// Measure rows by their VISIBLE INK height, not the font's layout box. A
// Pebble system font's box carries dead top-side internal leading (15 px on
// LECO_42), and reserving it is what inflated every inter-row gap and made
// stacks look top-heavy. Lift the draw box by that leading so the glyph — not
// the box — lands on the reserved band.
bool ui_layout_solve(UILayoutRow *rows, int count, GRect avail);

// Height the stack needs at minimum gaps, including the class's top/bottom
// padding. Use this to test a candidate font tier before committing to it.
int ui_layout_required_h(const UILayoutRow *rows, int count);

#endif  // UI_SCREEN_SMALL_RECT || UI_SCREEN_SMALL_ROUND
