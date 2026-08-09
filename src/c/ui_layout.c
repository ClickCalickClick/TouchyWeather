#include "ui_layout.h"

// Everything below is small-classes-only. On emery/gabbro this file compiles
// to an empty object so their binaries stay byte-identical (tools/lock_guard.py).
#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)

// Per-class frame.
//
// PAD_TOP/PAD_BOTTOM hold the stack clear of the band's edges. GAP_MIN/GAP_MAX
// bound the elastic spacing between rows: the solver starts at GAP_MIN, spreads
// whatever slack exists into the gaps up to GAP_MAX, and lets anything left
// over become margin — which is what centers the stack instead of letting it
// drift to the top.
//
// INSET is horizontal breathing room held back from every row's usable width.
// On the rect class it is ui_margin_x()'s 12, so flowed rows line up with every
// hand-placed element in the app. On chalk it is deliberately SMALLER than
// ui_margin_x()'s 20: the chord already does the real narrowing, and stacking a
// 20 px inset on top of it wastes the widest part of the glass at the vertical
// middle — which is exactly where the content sits.
#if defined(UI_SCREEN_SMALL_ROUND)
  #define UL_PAD_TOP     2
  #define UL_PAD_BOTTOM  2
  #define UL_GAP_MIN     2
  #define UL_GAP_MAX    10
  #define UL_INSET       8
#else  // UI_SCREEN_SMALL_RECT
  #define UL_PAD_TOP     2
  #define UL_PAD_BOTTOM  2
  #define UL_GAP_MIN     2
  #define UL_GAP_MAX    10
  #define UL_INSET      12
#endif

static int prv_present_count(const UILayoutRow *rows, int count) {
  int n = 0;
  for (int i = 0; i < count; i++) {
    if (rows[i].present) n++;
  }
  return n;
}

static int prv_content_h(const UILayoutRow *rows, int count) {
  int h = 0;
  for (int i = 0; i < count; i++) {
    if (rows[i].present) h += rows[i].h;
  }
  return h;
}

int ui_layout_required_h(const UILayoutRow *rows, int count) {
  int n = prv_present_count(rows, count);
  if (n == 0) return UL_PAD_TOP + UL_PAD_BOTTOM;
  return UL_PAD_TOP + UL_PAD_BOTTOM + prv_content_h(rows, count) +
         (n - 1) * UL_GAP_MIN;
}

#if defined(PBL_ROUND)
// Integer square root (Newton). Used for the round-screen chord; a float sqrt
// would pull in the soft-FP library for several calls a redraw.
static int prv_isqrt(int v) {
  if (v <= 0) return 0;
  int x = v, y = (x + 1) / 2;
  while (y < x) {
    x = y;
    y = (x + v / x) / 2;
  }
  return x;
}
#endif

int ui_band_w(GRect bounds, int y, int h) {
#if defined(PBL_ROUND)
  (void)bounds;
  // The circle is the SCREEN's, not the caller's rect. ui_layout_solve() hands
  // this function its `avail` band, and a band is almost never centered on the
  // glass: the main card's runs from the top of the screen down to the status
  // pill, so its own center sits ~22 px above the true one. Deriving the circle
  // from it made the top row measure ~35 px too wide and put the location label
  // straight back under the bezel — the exact defect class this module exists
  // to close. The settings card hid this because its band happens to sit within
  // 3 px of center.
  const int r = PBL_DISPLAY_WIDTH / 2;
  const int screen_cy = PBL_DISPLAY_HEIGHT / 2;
  // Chord at the band's vertical CENTER — see the header for why not the edge.
  int dy = (y + h / 2) - screen_cy;
  if (dy < 0) dy = -dy;
  if (dy >= r) return UI_LAYOUT_MIN_ROW_W;
  int half = prv_isqrt(r * r - dy * dy);
  int w = 2 * half - 2 * UL_INSET;
  // Never hand back a width a drawer can't use — the pill drawers subtract
  // padding from this and would go negative.
  return (w > UI_LAYOUT_MIN_ROW_W) ? w : UI_LAYOUT_MIN_ROW_W;
#else
  (void)y;
  (void)h;
  int w = bounds.size.w - 2 * UL_INSET;
  return (w > UI_LAYOUT_MIN_ROW_W) ? w : UI_LAYOUT_MIN_ROW_W;
#endif
}

bool ui_layout_solve(UILayoutRow *rows, int count, GRect avail) {
  const int n = prv_present_count(rows, count);
  const int content = prv_content_h(rows, count);
  // PAD is symmetric, so the stack centers exactly in `avail`.
  const int band_h = avail.size.h - UL_PAD_TOP - UL_PAD_BOTTOM;
  const int gaps = (n > 1) ? (n - 1) : 0;

  int gap = UL_GAP_MIN;
  bool fits = (content + gaps * UL_GAP_MIN) <= band_h;
  if (fits && gaps > 0) {
    int slack = band_h - content - gaps * UL_GAP_MIN;
    gap += slack / gaps;
    if (gap > UL_GAP_MAX) gap = UL_GAP_MAX;
  }

  const int stack_h = content + gaps * gap;
  int y = avail.origin.y + UL_PAD_TOP;
  if (fits) y += (band_h - stack_h) / 2;

  for (int i = 0; i < count; i++) {
    if (!rows[i].present) {
      // Leave a defined position behind so a caller that reads a hidden row's
      // geometry gets something sane instead of stale values.
      rows[i].y = y;
      rows[i].cy = y;
      rows[i].x = avail.origin.x;
      rows[i].w = 0;
      continue;
    }
    rows[i].y = y;
    rows[i].cy = y + rows[i].h / 2;
    rows[i].w = ui_band_w(avail, y, rows[i].h);
    rows[i].x = avail.origin.x + (avail.size.w - rows[i].w) / 2;
    y += rows[i].h + gap;
  }
  return fits;
}

#endif  // UI_SCREEN_SMALL_RECT || UI_SCREEN_SMALL_ROUND
