#include "detail_modal.h"
#include "theme.h"
#include "ui.h"
#include "ui_layout.h"
#include "icons.h"
#include "weather_data.h"
#include "refresh_sheet.h"
#include "nav.h"
#include <stdio.h>

// Bottom sheet modeled on refresh_sheet.c: a single overlay layer whose frame
// slides up from the bottom. All content is vector (lines + filled circles) so
// it costs almost nothing and works on every platform.

#define SLIDE_DURATION_MS  250
#define SLIDE_FRAME_MS     30
// Sheet covers this fraction of the screen height; the top of the underlying
// card stays visible so it reads as an overlay, not a navigation.
//
// The small classes take the whole screen instead. At 80% the leftover strip is
// 34 px — enough to draw the card's OWN header above the sheet's, so the screen
// read "6 HOURS" then "TEMP TREND" and spent 35% of a 168 px display on two
// titles (#54); on chalk the strip cut that header through its x-height and the
// chord clipped both ends, leaving a row of sliced half-glyphs (#55). Neither
// survives the sheet covering the card entirely, and the charts get the 34 px.
// The slide-up, the grab notch and the top rule all stay, so it still reads as
// a dismissible overlay rather than a navigation.
#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
#  define SHEET_HEIGHT_PCT 100
#else
#  define SHEET_HEIGHT_PCT 80
#endif

// Lowest y (sheet-local) content may occupy. Covering the whole screen means
// the sheet's lower edge IS the frame, so on the round class the bottom corners
// are outside the glass and content has to stop short of the arc. Expands to a
// bare `s_sheet_h` on the large classes, so their expressions are unchanged.
#if defined(UI_SCREEN_SMALL_ROUND)
#  define DM_CONTENT_BOT (s_sheet_h - 20)
#elif defined(UI_SCREEN_SMALL_RECT)
#  define DM_CONTENT_BOT (s_sheet_h - 4)
#else
#  define DM_CONTENT_BOT s_sheet_h
#endif

// Sheet top chrome. Full-screen lifts the title out of the wide middle of the
// round glass and into the narrow cap, where the chord at y=12 is only ~90 px
// — too tight for "TEMP TREND" plus its icon — so chalk carries it lower.
#if defined(UI_SCREEN_SMALL_ROUND)
#  define DM_NOTCH_Y 14
#  define DM_TITLE_Y 24
#elif defined(UI_SCREEN_SMALL_RECT)
#  define DM_NOTCH_Y 4
#  define DM_TITLE_Y 10
#else
#  define DM_NOTCH_Y 5
#  define DM_TITLE_Y 12
#endif

// --- Font ramp ---
//
// This file used to call fonts_get_system_font() at 14 sites, so the small
// classes inherited the large classes' type ramp with none of their room, and
// there was no single place to retune it. The small classes now route through
// the ui_font_* accessors — which also gives the sheet the Big Mode ramp it
// never had, at no size cost: on a small class ui_font_header() is 18_BOLD in
// both modes and ui_font_caption() only gains weight. The large classes keep
// the verbatim expression they shipped, so their emitted code is unchanged.
//
// The two LECO numeral fonts have no accessor, so both branches name the same
// key; the macro exists so this file has one tuning point per role.
#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
#  define DM_FONT_CAPTION ui_font_caption()
#  define DM_FONT_HEADER  ui_font_header()
#else
#  define DM_FONT_CAPTION fonts_get_system_font(FONT_KEY_GOTHIC_14)
#  define DM_FONT_HEADER  fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD)
#endif
#define DM_FONT_NUM_SM    fonts_get_system_font(FONT_KEY_LECO_20_BOLD_NUMBERS)
#define DM_FONT_NUM_LG    fonts_get_system_font(FONT_KEY_LECO_28_LIGHT_NUMBERS)

// Muted ink for TEXT and STROKES. theme_muted() is safe for a fill on 1-bit —
// it dithers and stays visible, which is why the grab notch and the pollutant
// track survive — but a stroke or a glyph does not dither: LightGray quantizes
// to white on the light theme and DarkGray to black on the dark one, i.e. the
// background in both, and the element draws itself invisible. That is D6's
// rule 1 and the same root cause as the fog icon in Phase 5. Every muted
// baseline, caption and outlined dot in this file was hitting it.
// PBL_BW is false on emery and gabbro, so this cannot reach the locked pair.
#if defined(PBL_BW)
#  define DM_MUTED_INK theme_secondary()
#else
#  define DM_MUTED_INK theme_muted()
#endif

// Week sheet metrics. The large classes' 42 px icon and 32 px numeral row need
// ~155 px below the title; a full-screen small sheet has ~110. The numerals ran
// into the page dots and the POP row — the day's rain chance, the one reading
// the Week CARD does not already give you — was silently dropped by its own
// fits-on-screen guard. Shrinking the icon buys back exactly what the POP row
// costs; the large classes keep their shipped values.
#if defined(UI_SCREEN_SMALL_ROUND)
#  define DM_WK_ICON      26
#  define DM_WK_ICON_GAP   2
#  define DM_WK_CAPS_GAP   2
#  define DM_WK_NUMS_GAP  13
#  define DM_WK_NUMS_H    28
#elif defined(UI_SCREEN_SMALL_RECT)
#  define DM_WK_ICON      34
#  define DM_WK_ICON_GAP   2
#  define DM_WK_CAPS_GAP   2
#  define DM_WK_NUMS_GAP  13
#  define DM_WK_NUMS_H    30
#else
#  define DM_WK_ICON      42
#  define DM_WK_ICON_GAP   6
#  define DM_WK_CAPS_GAP   4
#  define DM_WK_NUMS_GAP  15
#  define DM_WK_NUMS_H    32
#endif

typedef enum { DM_IDLE = 0, DM_OPENING, DM_OPEN, DM_CLOSING } DMState;

static Layer *s_layer = NULL;
static DMState s_state = DM_IDLE;
static DetailType s_type = DETAIL_NONE;
static int s_screen_h = 0;
static int s_screen_w = 0;
static int s_sheet_h = 0;

// Sheet top edge Y in screen coords: s_screen_h (fully closed, below screen)
// … s_screen_h - s_sheet_h (fully open).
static int s_top_y = 0;

static AppTimer *s_slide_timer = NULL;
static uint64_t s_slide_start_ms = 0;
static int s_slide_from = 0;
static int s_slide_to = 0;

// Per-modal transient state.
static bool s_pop_overlay = false;  // 6 Hours: show POP series over temp trend
static int  s_week_page = 0;        // Week: which day (0..4) is shown

static uint64_t prv_now_ms(void) {
  time_t s; uint16_t ms;
  time_ms(&s, &ms);
  return (uint64_t)s * 1000ULL + (uint64_t)ms;
}

static void prv_apply_frame(void) {
  if (!s_layer) return;
  GRect f = GRect(0, s_top_y, s_screen_w, s_sheet_h);
  layer_set_frame(s_layer, f);
  layer_set_bounds(s_layer, GRect(0, 0, s_screen_w, s_sheet_h));
}

static void prv_mark_dirty(void) {
  if (!s_layer) return;
  prv_apply_frame();
  layer_mark_dirty(s_layer);
}

static void prv_slide_tick(void *ctx) {
  (void)ctx;
  s_slide_timer = NULL;
  uint64_t elapsed = prv_now_ms() - s_slide_start_ms;
  if (elapsed >= SLIDE_DURATION_MS) {
    s_top_y = s_slide_to;
    prv_mark_dirty();
    if (s_state == DM_OPENING) {
      s_state = DM_OPEN;
    } else if (s_state == DM_CLOSING) {
      s_state = DM_IDLE;
      s_type = DETAIL_NONE;
      prv_mark_dirty();
    }
    return;
  }
  int32_t p1k = (int32_t)((elapsed * 1000) / SLIDE_DURATION_MS);
  int32_t inv = 1000 - p1k;
  int32_t eased = 1000 - (inv * inv) / 1000;  // ease-out quad
  s_top_y = s_slide_from + ((s_slide_to - s_slide_from) * eased) / 1000;
  prv_mark_dirty();
  s_slide_timer = app_timer_register(SLIDE_FRAME_MS, prv_slide_tick, NULL);
}

static void prv_start_slide(int to_y) {
  s_slide_from = s_top_y;
  s_slide_to = to_y;
  s_slide_start_ms = prv_now_ms();
  if (s_slide_timer) app_timer_cancel(s_slide_timer);
  s_slide_timer = app_timer_register(SLIDE_FRAME_MS, prv_slide_tick, NULL);
}

// ---- Content renderers (local sheet coords: 0..s_sheet_w × 0..s_sheet_h) ----

// Shared: sheet top chrome (rounded top, grab notch, title) drawn at y=0.
// Returns the y below the header where content may begin.
static int prv_draw_chrome(GContext *ctx, const char *title,
                           UIIconDrawFn icon) {
  int w = s_screen_w;
  // Grab notch.
  graphics_context_set_fill_color(ctx, theme_muted());
  graphics_fill_rect(ctx, GRect(w / 2 - 14, DM_NOTCH_Y, 28, 4), 2, GCornersAll);
  // Title row (icon + label), reusing the card-header vocabulary.
  GRect hb = GRect(0, DM_TITLE_Y, w, UI_HEADER_HEIGHT);
  ui_draw_card_header_with_icon(ctx, hb, title, theme_fg(), 0, 16, icon);
  return DM_TITLE_Y + UI_HEADER_HEIGHT + 4;
}

#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
// Usable plot width: the band the axis labels can occupy, measured at the label
// row's LOWEST ink row rather than its center. Center-measuring is right for the
// centered text ui_band_w() was written for, but an axis label is a box sitting
// on the bottom of the screen, and its bottom corners are the part the arc eats.
// Deriving the PLOT from this — instead of from a fixed margin — is what keeps a
// label and the point it describes sharing an x-range on chalk.
static int prv_axis_band_w(int label_y, int label_h) {
  return ui_band_w(GRect(0, 0, s_screen_w, s_sheet_h),
                   label_y + label_h - 1, 1);
}

// X-axis labels for a 6-point series.
//
// The old walk was `i += (s_screen_w < 160) ? 2 : 1`, and it failed at both ends
// of the range it was meant to cover. On the rect classes it labels 0/2/4, so it
// never labels the LAST point and the axis has no end. On chalk 180 is not < 160,
// so it drew all six 40 px boxes across a chord-narrowed line, which collapses
// into "PM2 AM1 AM2 A" the moment the hour strings are wide (#58) — and the live
// emulator strings are single digits, so the data on screen hides it.
//
// First / middle / last instead: three boxes always fit, the range reads
// end-to-end, and the two end labels are aligned INTO the plot (left at the
// first point, right at the last) rather than centered on it, so no label can
// extend past the chart it describes (#57, #59).
static void prv_draw_axis_labels(GContext *ctx, const int *px, int y, int h,
                                 int lo, int hi) {
  WeatherData *d = weather_data_get();
  const int kW = 44;
  graphics_context_set_text_color(ctx, theme_secondary());
  graphics_draw_text(ctx, d->hours_label[0], DM_FONT_CAPTION,
                     GRect(lo, y, kW, h),
                     GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  int mx = px[3] - kW / 2;
  if (mx + kW > hi) mx = hi - kW;
  if (mx < lo) mx = lo;
  graphics_draw_text(ctx, d->hours_label[3], DM_FONT_CAPTION,
                     GRect(mx, y, kW, h),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  graphics_draw_text(ctx, d->hours_label[5], DM_FONT_CAPTION,
                     GRect(hi - kW, y, kW, h),
                     GTextOverflowModeFill, GTextAlignmentRight, NULL);
}

// A chart point's value, drawn on an opaque knockout so the trend line cannot
// run through the digits (#56: "74" read as "7ч" where the line and its first
// marker crossed the glyphs). Sits above the point, flips below when there is no
// room above, and is clamped inside the plot horizontally.
static void prv_draw_point_value(GContext *ctx, int value, int cx, int cy,
                                 int top_limit, int lo, int hi) {
  char b[8];
  snprintf(b, sizeof(b), "%d", value);
  GFont f = DM_FONT_NUM_SM;
  GSize ts = graphics_text_layout_get_content_size(
      b, f, GRect(0, 0, 60, 30), GTextOverflowModeFill, GTextAlignmentLeft);
  int w = ts.w + 6;
  int h = ts.h;
  int x = cx - w / 2;
  int y = cy - h - 4;
  // Clamped up against the limit rather than flipped below the point: flipping
  // drops the numeral into the middle of the plot, which is the collision this
  // exists to prevent. Pinned here it can at worst touch its own marker.
  if (y < top_limit) y = top_limit;
  if (x + w > hi) x = hi - w;
  if (x < lo) x = lo;
  graphics_context_set_fill_color(ctx, theme_bg());
  graphics_fill_rect(ctx, GRect(x, y, w, h), 0, GCornerNone);
  graphics_context_set_text_color(ctx, theme_fg());
  graphics_draw_text(ctx, b, f, GRect(x, y, w, h),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}
#endif  // small classes

// 6 Hours → segmented temperature trend line with point markers + hour labels.
static void prv_draw_hours(GContext *ctx) {
  WeatherData *d = weather_data_get();
  int top = prv_draw_chrome(ctx, "TEMP TREND", icon_draw_clock);

  // Hint line just under the header (SELECT toggles the POP overlay).
  GFont hf = DM_FONT_CAPTION;
  graphics_context_set_text_color(ctx, DM_MUTED_INK);
  graphics_draw_text(ctx, s_pop_overlay ? "SEL: hide rain %" : "SEL: show rain %",
                     hf, GRect(0, top, s_screen_w, 15),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);

#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
  int label_h = 15;
  // 26, not 16: the max annotation is drawn ABOVE its point, so the point at the
  // top of the plot needs a whole label's height of clear air above it or the
  // numeral lands back on the hint row.
  int chart_top = top + 15 + 26;
  int chart_bottom = DM_CONTENT_BOT - label_h - 6;
  int chart_w = prv_axis_band_w(chart_bottom + 4, label_h);
  int chart_x = (s_screen_w - chart_w) / 2;
#else
  int margin = UI_MARGIN_X + 6;
  int label_h = 15;
  int chart_x = margin;
  int chart_w = s_screen_w - 2 * margin;
  int chart_top = top + 15 + 16;               // hint + room for max annotation
  int chart_bottom = DM_CONTENT_BOT - label_h - 6;  // room for hour labels
#endif
  int chart_h = chart_bottom - chart_top;
  if (chart_h < 20) return;

  // Temp range → y mapping (pad so extremes aren't on the edge).
  int tmin = d->hours_temp[0], tmax = d->hours_temp[0];
  for (int i = 1; i < 6; i++) {
    if (d->hours_temp[i] < tmin) tmin = d->hours_temp[i];
    if (d->hours_temp[i] > tmax) tmax = d->hours_temp[i];
  }
  int span = tmax - tmin; if (span < 1) span = 1;

  int px[6], py[6];
  for (int i = 0; i < 6; i++) {
    px[i] = chart_x + (chart_w * i) / 5;
    int norm = ((d->hours_temp[i] - tmin) * 1000) / span;  // 0..1000
    py[i] = chart_bottom - (chart_h * norm) / 1000;
  }

  // Dotted baseline gridline.
  ui_draw_dotted_hline(ctx, chart_x, chart_x + chart_w, chart_bottom + 2,
                       DM_MUTED_INK);

  // Optional POP overlay (SELECT toggles): light blue dots along the bottom
  // scaled 0..100% into the chart height.
  if (s_pop_overlay) {
    graphics_context_set_stroke_color(ctx, theme_accent_blue());
    graphics_context_set_fill_color(ctx, theme_accent_blue());
    graphics_context_set_stroke_width(ctx, 2);
    int prevx = 0, prevy = 0;
    for (int i = 0; i < 6; i++) {
      int y = chart_bottom - (chart_h * d->hours_pop[i]) / 100;
      if (i > 0) graphics_draw_line(ctx, GPoint(prevx, prevy), GPoint(px[i], y));
      graphics_fill_circle(ctx, GPoint(px[i], y), 2);
      prevx = px[i]; prevy = y;
    }
  }

  // Temperature trend: segmented line + filled markers (orange).
  graphics_context_set_stroke_color(ctx, theme_accent_orange());
  graphics_context_set_stroke_width(ctx, 3);
  for (int i = 1; i < 6; i++) {
    graphics_draw_line(ctx, GPoint(px[i - 1], py[i - 1]), GPoint(px[i], py[i]));
  }
  graphics_context_set_fill_color(ctx, theme_accent_orange());
  for (int i = 0; i < 6; i++) {
    graphics_fill_circle(ctx, GPoint(px[i], py[i]), 3);
  }

  // Annotate the min and max temps in LECO, always ABOVE their point so they
  // never collide with the hour-label row at the bottom.
#if !defined(UI_SCREEN_SMALL_RECT) && !defined(UI_SCREEN_SMALL_ROUND)
  GFont numf = DM_FONT_NUM_SM;
  char b[8];
#endif
  bool did_max = false, did_min = false;
  graphics_context_set_text_color(ctx, theme_fg());
  for (int i = 0; i < 6; i++) {
    bool is_max = (!did_max && d->hours_temp[i] == tmax);
    bool is_min = (!did_min && d->hours_temp[i] == tmin);
    if (!is_max && !is_min) continue;
#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
    prv_draw_point_value(ctx, d->hours_temp[i], px[i], py[i], top + 15,
                         chart_x, chart_x + chart_w);
#else
    snprintf(b, sizeof(b), "%d", d->hours_temp[i]);
    graphics_draw_text(ctx, b, numf, GRect(px[i] - 18, py[i] - 22, 36, 22),
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
#endif
    if (is_max) did_max = true; else did_min = true;
  }

  // Hour labels along the bottom (every other one on narrow screens).
#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
  prv_draw_axis_labels(ctx, px, chart_bottom + 4, label_h,
                       chart_x, chart_x + chart_w);
#else
  GFont lf = DM_FONT_CAPTION;
  graphics_context_set_text_color(ctx, theme_secondary());
  int step = (s_screen_w < 160) ? 2 : 1;
  for (int i = 0; i < 6; i += step) {
    graphics_draw_text(ctx, d->hours_label[i], lf,
                       GRect(px[i] - 20, chart_bottom + 4, 40, label_h),
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  }
#endif
}

// Precipitation → hourly rainfall amount bars + headline + POP row.
static void prv_draw_precip(GContext *ctx) {
  WeatherData *d = weather_data_get();
  int top = prv_draw_chrome(ctx, "RAINFALL", icon_draw_droplet);

#if !defined(UI_SCREEN_SMALL_RECT) && !defined(UI_SCREEN_SMALL_ROUND)
  int margin = UI_MARGIN_X + 6;
#endif
  // Headline: "RAIN IN Nh", else a POP-based chance if any upcoming hour
  // looks likely, else "NO RAIN SOON".
  const int POP_HEADLINE_MIN = 40;
  char head[24];
  if (d->rain_alert_min >= 0) {
    int m = d->rain_alert_min;
    if (m < 60) snprintf(head, sizeof(head), "RAIN IN %dM", m);
    else        snprintf(head, sizeof(head), "RAIN IN %dH", (m + 30) / 60);
  } else {
    snprintf(head, sizeof(head), "NO RAIN SOON");
    for (int i = 0; i < 6; i++) {  // hours_pop[i] is +1h..+6h out
      if (d->hours_pop[i] >= POP_HEADLINE_MIN) {
        snprintf(head, sizeof(head), "%d%% CHANCE IN %dH", d->hours_pop[i], i + 1);
        break;
      }
    }
  }
  graphics_context_set_text_color(ctx, theme_fg());
  graphics_draw_text(ctx, head, DM_FONT_HEADER,
                     GRect(0, top, s_screen_w, 22),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);

#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
  int chart_top = top + 26;
  int label_h = 16;
  int pop_h = 16;
  int chart_bottom = DM_CONTENT_BOT - label_h - pop_h - 10;
  int chart_w = prv_axis_band_w(chart_bottom + 2 + pop_h, label_h);
  int chart_x = (s_screen_w - chart_w) / 2;
#else
  int chart_x = margin;
  int chart_w = s_screen_w - 2 * margin;
  int chart_top = top + 26;
  int label_h = 16;
  int pop_h = 16;
  int chart_bottom = DM_CONTENT_BOT - label_h - pop_h - 10;
#endif
  int chart_h = chart_bottom - chart_top;
  if (chart_h < 16) return;

  // Scale bars by the max amount (min floor so a light drizzle still shows).
  int amax = 1;
  for (int i = 0; i < 6; i++) if (d->hours_precip_x10[i] > amax) amax = d->hours_precip_x10[i];

  int slot = chart_w / 6;
  int bw = slot - 6; if (bw < 4) bw = 4;
  ui_draw_dotted_hline(ctx, chart_x, chart_x + chart_w, chart_bottom, DM_MUTED_INK);
  graphics_context_set_fill_color(ctx, theme_accent_blue());
  for (int i = 0; i < 6; i++) {
    int bx = chart_x + i * slot + (slot - bw) / 2;
    int bh = (chart_h * d->hours_precip_x10[i]) / amax;
    if (d->hours_precip_x10[i] > 0 && bh < 2) bh = 2;  // trace floor
    graphics_fill_rect(ctx, GRect(bx, chart_bottom - bh, bw, bh), 1, GCornersTop);
  }

  // POP row beneath each bar (percent).
  GFont sf = DM_FONT_CAPTION;
  graphics_context_set_text_color(ctx, theme_secondary());
  char b[8];
  for (int i = 0; i < 6; i++) {
    int bx = chart_x + i * slot;
    snprintf(b, sizeof(b), "%d", d->hours_pop[i]);
    graphics_draw_text(ctx, b, sf, GRect(bx, chart_bottom + 2, slot, pop_h),
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  }
  // Hour labels along the very bottom.
#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
  // Anchored on the bar centers rather than drawn into slot-width boxes: a slot
  // is ~19 px here, so a real hour string was being cut to "1…" "1…" "2 …" (#59).
  int cpx[6];
  for (int i = 0; i < 6; i++) cpx[i] = chart_x + i * slot + slot / 2;
  prv_draw_axis_labels(ctx, cpx, chart_bottom + 2 + pop_h, label_h,
                       chart_x, chart_x + chart_w);
#else
  graphics_context_set_text_color(ctx, DM_MUTED_INK);
  int step = (s_screen_w < 160) ? 2 : 1;
  for (int i = 0; i < 6; i += step) {
    int bx = chart_x + i * slot;
    graphics_draw_text(ctx, d->hours_label[i], sf,
                       GRect(bx, chart_bottom + 2 + pop_h, slot, label_h),
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  }
#endif
}

// High-temp accent by condition, mirroring the Week card's palette rules.
static GColor prv_high_color(WeatherCondition cond) {
  switch (cond) {
    case COND_SUNNY:
    case COND_PARTLY_CLOUDY: return theme_accent_orange();
    case COND_RAIN:
    case COND_SNOW:
    case COND_STORM:         return theme_accent_blue();
    default:                 return theme_fg();
  }
}

// Week → one page per day (5). Big condition icon, HIGH/LOW columns, POP row,
// and a page-dot indicator. UP/DOWN page between days.
#define WEEK_DAYS 5
static void prv_draw_week(GContext *ctx) {
  WeatherData *d = weather_data_get();
  int p = s_week_page;
  if (p < 0) p = 0;
  if (p >= WEEK_DAYS) p = WEEK_DAYS - 1;

  // Header title = the day label; calendar icon in the chrome.
  int top = prv_draw_chrome(ctx, d->days_label[p], icon_draw_calendar);
  int cx = s_screen_w / 2;

  // Reserve the bottom strip for the page dots.
  int dots_h = 16;
#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
  // Measured off the dot row itself rather than off a fixed strip height: the
  // strip reserved 20 px for a row of 3 px dots and the POP row needed 9 of them.
  int content_bottom = (DM_CONTENT_BOT - dots_h / 2 - 2) - 6;
#else
  int content_bottom = DM_CONTENT_BOT - dots_h - 4;
#endif

  // Big condition icon, centered horizontally near the top of the content.
  int icon_size = DM_WK_ICON;
  int icon_cy = top + icon_size / 2 + DM_WK_ICON_GAP;
  icon_draw_condition(ctx, GPoint(cx, icon_cy), icon_size, d->days_cond[p]);

  // HIGH / LOW columns beneath the icon.
  GFont numf = DM_FONT_NUM_LG;
  GFont capf = DM_FONT_CAPTION;
  int col_w = s_screen_w / 2;
  int caps_y = icon_cy + icon_size / 2 + DM_WK_CAPS_GAP;
  int nums_y = caps_y + DM_WK_NUMS_GAP;
  char hb[8], lb[8];
  snprintf(hb, sizeof(hb), "%d°", d->days_high[p]);
  snprintf(lb, sizeof(lb), "%d°", d->days_low[p]);

  graphics_context_set_text_color(ctx, DM_MUTED_INK);
  graphics_draw_text(ctx, "HIGH", capf, GRect(0, caps_y, col_w, 15),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  graphics_draw_text(ctx, "LOW", capf, GRect(col_w, caps_y, col_w, 15),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);

  graphics_context_set_text_color(ctx, prv_high_color(d->days_cond[p]));
  graphics_draw_text(ctx, hb, numf, GRect(0, nums_y, col_w, DM_WK_NUMS_H),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  graphics_context_set_text_color(ctx, theme_secondary());
  graphics_draw_text(ctx, lb, numf, GRect(col_w, nums_y, col_w, DM_WK_NUMS_H),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);

  // POP row (droplet + %), centered under the columns.
  int pop_y = nums_y + DM_WK_NUMS_H;
  if (pop_y + 18 <= content_bottom) {
    char pb[8];
    snprintf(pb, sizeof(pb), "%d%%", (int)d->days_pop[p]);
    GFont pf = DM_FONT_HEADER;
    GSize ps = graphics_text_layout_get_content_size(pb, pf,
        GRect(0, 0, s_screen_w, 22), GTextOverflowModeFill, GTextAlignmentLeft);
    int drop = 12;
    int grp_w = drop + 4 + ps.w;
    int gx = cx - grp_w / 2;
    icon_draw_droplet(ctx, GPoint(gx + drop / 2, pop_y + 9), drop,
                      theme_accent_blue());
    graphics_context_set_text_color(ctx, theme_accent_blue());
    graphics_draw_text(ctx, pb, pf, GRect(gx + drop + 4, pop_y, ps.w + 4, 22),
                       GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  }

  // Page dots: filled for the current day, outlined for the rest.
  int dot_r = 3;
  int spacing = 12;
  int total_w = (WEEK_DAYS - 1) * spacing;
  int dx = cx - total_w / 2;
  int dy = DM_CONTENT_BOT - dots_h / 2 - 2;
  for (int i = 0; i < WEEK_DAYS; i++) {
    GPoint c = GPoint(dx + i * spacing, dy);
    if (i == p) {
      graphics_context_set_fill_color(ctx, theme_fg());
      graphics_fill_circle(ctx, c, dot_r);
    } else {
      graphics_context_set_stroke_color(ctx, DM_MUTED_INK);
      graphics_draw_circle(ctx, c, dot_r);
    }
  }
}

// UV → current value + qualitative label + PEAK, then an hourly UV curve
// (+1h..+6h) as a segmented trend line, mirroring the temp-trend renderer.
static void prv_draw_uv(GContext *ctx) {
  WeatherData *d = weather_data_get();
  int top = prv_draw_chrome(ctx, "UV TODAY", icon_draw_sun);

  char sb[24];
  snprintf(sb, sizeof(sb), "UV %d  %s", d->uv, uv_label(d->uv));
  graphics_context_set_text_color(ctx, theme_accent_orange());
  graphics_draw_text(ctx, sb, DM_FONT_HEADER,
                     GRect(0, top, s_screen_w, 22),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  int y = top + 22;
  if (d->uv_max > 0) {
    char pb[16];
    snprintf(pb, sizeof(pb), "PEAK %d", d->uv_max);
    graphics_context_set_text_color(ctx, theme_secondary());
    graphics_draw_text(ctx, pb, DM_FONT_CAPTION,
                       GRect(0, y, s_screen_w, 16),
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    y += 16;
  }

#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
  int label_h = 15;
  int chart_top = y + 24;  // clear air above the plot for the peak annotation
  int chart_bottom = DM_CONTENT_BOT - label_h - 6;
  int chart_w = prv_axis_band_w(chart_bottom + 4, label_h);
  int chart_x = (s_screen_w - chart_w) / 2;
#else
  int margin = UI_MARGIN_X + 6;
  int label_h = 15;
  int chart_x = margin;
  int chart_w = s_screen_w - 2 * margin;
  int chart_top = y + 10;
  int chart_bottom = DM_CONTENT_BOT - label_h - 6;
#endif
  int chart_h = chart_bottom - chart_top;
  if (chart_h < 20) return;

  int umax = 1;
  for (int i = 0; i < 6; i++) if (d->hours_uv[i] > umax) umax = d->hours_uv[i];

  int px[6], py[6];
  for (int i = 0; i < 6; i++) {
    px[i] = chart_x + (chart_w * i) / 5;
    py[i] = chart_bottom - (chart_h * d->hours_uv[i]) / umax;
  }
  ui_draw_dotted_hline(ctx, chart_x, chart_x + chart_w, chart_bottom + 2,
                       DM_MUTED_INK);
  graphics_context_set_stroke_color(ctx, theme_accent_orange());
  graphics_context_set_stroke_width(ctx, 3);
  for (int i = 1; i < 6; i++) {
    graphics_draw_line(ctx, GPoint(px[i - 1], py[i - 1]), GPoint(px[i], py[i]));
  }
  graphics_context_set_fill_color(ctx, theme_accent_orange());
  for (int i = 0; i < 6; i++) graphics_fill_circle(ctx, GPoint(px[i], py[i]), 3);

#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
  // Mark WHEN the curve peaks (#64). The temp trend annotates its extremes and
  // this one annotated nothing, so the curve gave a shape with no scale. Only
  // the max is worth a label — a UV minimum is almost always 0.
  // NOTE: the hours this is plotted against are the temp buffer's and are wrong
  // (#63, platform-independent and deferred); this labels the value, not a claim
  // about the time, and gains its full meaning once #63 lands.
  for (int i = 0; i < 6; i++) {
    if (d->hours_uv[i] != umax) continue;
    prv_draw_point_value(ctx, umax, px[i], py[i], y,
                         chart_x, chart_x + chart_w);
    break;
  }
  prv_draw_axis_labels(ctx, px, chart_bottom + 4, label_h,
                       chart_x, chart_x + chart_w);
#else
  // (No per-point annotation — the summary line already gives current UV +
  // peak; the curve just communicates the shape/trend.)

  GFont lf = DM_FONT_CAPTION;
  graphics_context_set_text_color(ctx, theme_secondary());
  int step = (s_screen_w < 160) ? 2 : 1;
  for (int i = 0; i < 6; i += step) {
    graphics_draw_text(ctx, d->hours_label[i], lf,
                       GRect(px[i] - 20, chart_bottom + 4, 40, label_h),
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  }
#endif
}

// AQI category color (mirrors card_air_quality's static helper — kept local
// so the modules stay decoupled).
static GColor prv_aqi_color(int aqi) {
#if defined(PBL_BW)
  // Category hue carries nothing on 1-bit, and these raw constants are actively
  // harmful here: GColorIslamicGreen dithers to roughly the same 50 % check as
  // the theme_muted() track drawn behind it, so the filled part of the bar and
  // the empty part become the same texture and the breakdown reads as nothing.
  // Phase 5 fixed exactly this on the Air Quality CARD (#45/#46) — this file
  // keeps its own decoupled copy of the helper and was missed, so the defect
  // survived in the sheet at every AQI outside the 51..100 band (which escapes
  // only because theme_accent_orange() already has a 1-bit fallback).
  // Solid fg on a dithered track: the vocabulary the UV card proves out.
  (void)aqi;
  return theme_fg();
#else
  if (aqi <= 50)  return GColorIslamicGreen;
  if (aqi <= 100) return theme_accent_orange();
  if (aqi <= 150) return GColorOrange;
  if (aqi <= 200) return GColorRed;
  if (aqi <= 300) return GColorPurple;
  return GColorDarkCandyAppleRed;
#endif
}

// Air Quality → AQI headline + a 4-pollutant breakdown (PM2.5/PM10/O3/NO2)
// as labelled bars scaled to the largest value, plus a pollen line when the
// region is covered (else a units caption).
static void prv_draw_aq(GContext *ctx) {
  WeatherData *d = weather_data_get();
  int top = prv_draw_chrome(ctx, "AIR DETAIL", icon_draw_pulse);
  int margin = UI_MARGIN_X + 6;
  GColor cat = prv_aqi_color(d->aqi);

  char hb[24];
  snprintf(hb, sizeof(hb), "AQI %d  %s", d->aqi, aqi_label(d->aqi));
  graphics_context_set_text_color(ctx, cat);
  graphics_draw_text(ctx, hb, DM_FONT_HEADER,
                     GRect(0, top, s_screen_w, 22),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  int y = top + 26;

  const char *names[4] = { "PM2.5", "PM10", "O3", "NO2" };
  int vals[4] = { d->pm2_5, d->pm10, d->o3, d->no2 };
  int vmax = 1;
  for (int i = 0; i < 4; i++) if (vals[i] > vmax) vmax = vals[i];

  GFont nf = DM_FONT_CAPTION;
  GFont vf = DM_FONT_HEADER;
#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
  // Four rows plus the caption have to fit the band, and at the large classes'
  // 24 px pitch they do not: the sheet ran NO2 and the pollen line straight off
  // the bottom edge, silently, so a quarter of its data had never rendered on
  // any small platform. Derive the pitch from the room that actually exists
  // rather than asserting one, so it also survives a taller caption.
  int cap_h = 15;
  int avail = DM_CONTENT_BOT - y - cap_h - 4;
  int row_h = avail / 4;
  if (row_h > 24) row_h = 24;
  if (row_h < 15) row_h = 15;
#else
  int row_h = 24;
#endif
  int label_w = 46;
  int val_w = 38;
  int bar_x = margin + label_w + 4;
  int bar_w = s_screen_w - margin - val_w - 4 - bar_x;
  if (bar_w < 10) bar_w = 10;
  for (int i = 0; i < 4; i++) {
    int ry = y + i * row_h;
    graphics_context_set_text_color(ctx, theme_secondary());
    graphics_draw_text(ctx, names[i], nf, GRect(margin, ry + 2, label_w, 18),
                       GTextOverflowModeFill, GTextAlignmentLeft, NULL);
    int by = ry + 6, bh = 8;
    graphics_context_set_fill_color(ctx, theme_muted());
    graphics_fill_rect(ctx, GRect(bar_x, by, bar_w, bh), 2, GCornersAll);
    int fw = (bar_w * vals[i]) / vmax;
    if (vals[i] > 0 && fw < 2) fw = 2;
    graphics_context_set_fill_color(ctx, cat);
    graphics_fill_rect(ctx, GRect(bar_x, by, fw, bh), 2, GCornersAll);
    char vb[8]; snprintf(vb, sizeof(vb), "%d", vals[i]);
    graphics_context_set_text_color(ctx, theme_fg());
    graphics_draw_text(ctx, vb, vf, GRect(s_screen_w - margin - val_w, ry,
                                          val_w, 22),
                       GTextOverflowModeFill, GTextAlignmentRight, NULL);
  }
  int fy = y + 4 * row_h + 2;

  // Caption line: pollen severity when covered, otherwise the units.
  if (d->pollen_level >= 0) {
    static const char *pw[6] = { "NONE", "VERY LOW", "LOW", "MODERATE",
                                 "HIGH", "VERY HIGH" };
    int lvl = d->pollen_level; if (lvl > 5) lvl = 5;
    char pcap[28];
    snprintf(pcap, sizeof(pcap), "POLLEN: %s", pw[lvl]);
    graphics_context_set_text_color(ctx, theme_secondary());
    graphics_draw_text(ctx, pcap, nf, GRect(margin, fy, s_screen_w - 2 * margin,
                                            15),
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  } else {
    graphics_context_set_text_color(ctx, DM_MUTED_INK);
    graphics_draw_text(ctx, "ug/m3", nf, GRect(margin, fy,
                                               s_screen_w - 2 * margin, 15),
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  }
}

static void prv_update(Layer *layer, GContext *ctx) {
  (void)layer;
  GRect b = layer_get_bounds(s_layer);
  // Sheet surface. The rounded top corners are what sell the slide-up on the
  // large classes, where the card stays visible behind. At full screen there is
  // nothing behind to round against, so the radius would just punch two notches
  // of the old card through the top corners at rest.
  graphics_context_set_fill_color(ctx, theme_bg());
#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
  graphics_fill_rect(ctx, b, 0, GCornerNone);
#else
  graphics_fill_rect(ctx, b, 6, GCornerTopLeft | GCornerTopRight);
#endif
  // Top edge rule. It marks the seam against the card behind — and once the
  // sheet covers the whole screen there is no card behind and no seam, just the
  // top of the glass. Drawing it anyway costs a hairline that a 1-bit panel can
  // only render at full contrast (a solid black bar across row 0, measured on
  // diorite), so on the small classes it is drawn only mid-slide, while there
  // genuinely is something behind it.
#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
  if (s_top_y > 0) {
    graphics_context_set_stroke_color(ctx, DM_MUTED_INK);
    graphics_context_set_stroke_width(ctx, 1);
    graphics_draw_line(ctx, GPoint(0, 0), GPoint(b.size.w, 0));
  }
#else
  graphics_context_set_stroke_color(ctx, DM_MUTED_INK);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, GPoint(0, 0), GPoint(b.size.w, 0));
#endif

  switch (s_type) {
    case DETAIL_HOURS:  prv_draw_hours(ctx);  break;
    case DETAIL_PRECIP: prv_draw_precip(ctx); break;
    case DETAIL_WEEK:   prv_draw_week(ctx);   break;
    case DETAIL_UV:     prv_draw_uv(ctx);     break;
    case DETAIL_AQ:     prv_draw_aq(ctx);     break;
    default: break;
  }
}

// ---- Public API ----

void detail_modal_init(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect rb = layer_get_bounds(root);
  s_screen_w = rb.size.w;
  s_screen_h = rb.size.h;
  s_sheet_h = (s_screen_h * SHEET_HEIGHT_PCT) / 100;
  s_top_y = s_screen_h;  // closed
  s_state = DM_IDLE;
  s_type = DETAIL_NONE;
  s_layer = layer_create(GRect(0, s_top_y, s_screen_w, s_sheet_h));
  layer_set_clips(s_layer, true);
  layer_set_update_proc(s_layer, prv_update);
  layer_add_child(root, s_layer);
}

void detail_modal_deinit(void) {
  if (s_slide_timer) { app_timer_cancel(s_slide_timer); s_slide_timer = NULL; }
  s_state = DM_IDLE;
  if (s_layer) { layer_destroy(s_layer); s_layer = NULL; }
}

bool detail_modal_is_active(void) {
  return s_state != DM_IDLE;
}

bool detail_modal_open(DetailType type) {
  if (type == DETAIL_NONE) return false;
  if (s_state != DM_IDLE) return false;
  if (refresh_sheet_is_active()) return false;  // one overlay at a time
  s_type = type;
  s_pop_overlay = false;
  s_week_page = 0;
  s_state = DM_OPENING;
  s_top_y = s_screen_h;
  prv_start_slide(s_screen_h - s_sheet_h);
  return true;
}

void detail_modal_close(void) {
  if (s_state == DM_IDLE || s_state == DM_CLOSING) return;
  s_state = DM_CLOSING;
  prv_start_slide(s_screen_h);
}

bool detail_modal_handle_back(void) {
  if (s_state == DM_IDLE) return false;
  detail_modal_close();
  return true;
}

// Week is paged by day: UP/DOWN move to the previous/next day (clamped, no
// wrap). Other modals just consume UP/DOWN to keep card nav locked.
static bool prv_week_page(int delta) {
  if (s_type != DETAIL_WEEK || s_state != DM_OPEN) return false;
  int np = s_week_page + delta;
  if (np < 0) np = 0;
  if (np >= WEEK_DAYS) np = WEEK_DAYS - 1;
  if (np != s_week_page) { s_week_page = np; prv_mark_dirty(); }
  return true;
}

bool detail_modal_handle_up(void)   { prv_week_page(-1); return detail_modal_is_active(); }
bool detail_modal_handle_down(void) { prv_week_page(+1); return detail_modal_is_active(); }

bool detail_modal_handle_select(void) {
  if (s_state != DM_OPEN) return detail_modal_is_active();
  if (s_type == DETAIL_HOURS) {
    s_pop_overlay = !s_pop_overlay;  // toggle POP overlay
    prv_mark_dirty();
  }
  return true;
}
