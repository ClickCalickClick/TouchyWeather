#include "cards.h"
#include "../theme.h"
#include "../icons.h"
#include "../ui.h"
#include "../ui_layout.h"
#include "../settings.h"
#include "../weather_data.h"
#include "../anim.h"
#include <stdio.h>

// #43 — the header's rain glyph on 1-bit.
//
// Both arguments are theme_accent_blue(), which collapses to theme_fg() on a
// 1-bit panel: a solid foreground cloud with the drops stroked in the SAME ink
// directly beneath it, i.e. one featureless blob. Phase 5's remap never reached
// this because its ICON_*_COLOR macros are file-local to icons.c and only wire
// up the two condition dispatchers — this card draws its own header icon.
//
// Same vocabulary that worked there: the cloud is a FILL, so theme_muted()
// dithers and keeps a readable silhouette, while the drops stay solid fg and
// read against it.
//
// Defined once and used at BOTH call sites. The Big Mode header duplicates this
// glyph, and a fix that reached only one of them is precisely how #99 survived
// Phase 5 — when a defect is in duplicated code, grep for the twin.
// Label boxes overhang the bar pitch by this much on each side.
//
// The pitch itself is right — avail_w/n = 24px on SMALL_RECT — and the labels'
// INK fits inside it: measured on chalk, "Now" is 23px and "+1h".."+4h" are 19.
// But a box of exactly the pitch still ellipsized "Now" to "N...", because a
// font's LAYOUT width is wider than its ink (side bearings), and the draw call
// measures the former. Widen the box past the pitch and let neighbouring boxes
// overlap: the boxes are centred, so what actually has to clear the margin and
// the neighbouring label is the ink, and at 23px against a 24px pitch it does.
// The overhang is why #39 is stated in terms of ink rather than boxes.
#define PRECIP_LBL_PAD 4

#if defined(PBL_BW)
#define PRECIP_ICON_CLOUD theme_muted()
#define PRECIP_ICON_DROP  theme_fg()
#else
#define PRECIP_ICON_CLOUD theme_accent_blue()
#define PRECIP_ICON_DROP  theme_accent_blue()
#endif

void card_precipitation_draw(GContext *ctx, GRect bounds) {
  WeatherData *d = weather_data_get();
  int W = bounds.size.w;
  int H = bounds.size.h;
  // Slide-transition origin offset (Phase 10F): translate every X coord
  // so the header cluster + bar chart move with the card during the
  // 200ms push.
  int ox = bounds.origin.x;

  // --- Big Mode (Stage B): 3 fat bars instead of 5. ---
  // Sample Now / +2h / +4h so the chart still spans the window; wide bars are
  // exactly the "larger object boundaries" the policy asks for. The +1h/+3h
  // bars drop (all 6 hours live in the precip detail modal). % above + hour
  // below both bump to the 18px label font. Mandatory banner kept. Returns
  // early -> Normal 5-bar layout below untouched, Big-OFF pixel-identical.
  if (settings_get_big_mode()) {
    // Header (same centered icon+label idiom, ui_font_header auto-scales).
    GFont bhf = ui_font_header();
    GSize bts = graphics_text_layout_get_content_size("PRECIPITATION", bhf,
        GRect(0, 0, W, UI_HEADER_HEIGHT), GTextOverflowModeTrailingEllipsis,
        GTextAlignmentLeft);
    int bicon = 18, bgap = 5;
    int btot = bicon + bgap + bts.w;
    int bsx = ox + (W - btot) / 2;
    icon_draw_cloud_rain(ctx,
        GPoint(bsx + bicon / 2, UI_HEADER_Y + UI_HEADER_HEIGHT / 2),
        bicon, PRECIP_ICON_CLOUD, PRECIP_ICON_DROP);
    graphics_context_set_text_color(ctx, theme_fg());
    graphics_draw_text(ctx, "PRECIPITATION", bhf,
        GRect(bsx + bicon + bgap, UI_HEADER_Y, bts.w + 4, UI_HEADER_HEIGHT),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

    static const int idx[3] = {0, 2, 4};
    static const char *const blabels[3] = {"Now", "+2h", "+4h"};
    int bar_w, bar_gap, chart_top, chart_bot;
#if defined(UI_SCREEN_SMALL_RECT)
    bar_w = 34; bar_gap = 16; chart_top = UI_HEADER_Y + UI_HEADER_HEIGHT + 12; chart_bot = H - 66;
#elif defined(UI_SCREEN_SMALL_ROUND)
    bar_w = 34; bar_gap = 16; chart_top = UI_HEADER_Y + UI_HEADER_HEIGHT + 12; chart_bot = H - 74;
#elif defined(UI_SCREEN_LARGE_RECT)
    bar_w = 46; bar_gap = 26; chart_top = UI_HEADER_Y + UI_HEADER_HEIGHT + 16; chart_bot = H - 86;
#else  // UI_SCREEN_LARGE_ROUND
    bar_w = 48; bar_gap = 28; chart_top = UI_HEADER_Y + UI_HEADER_HEIGHT + 20; chart_bot = H - 96;
#endif
    int chart_h = chart_bot - chart_top;
    int label_top = 20;  // reserve for the % label above a full-height bar
    int total_bars_w = 3 * bar_w + 2 * bar_gap;
    int bar_x0 = ox + (W - total_bars_w) / 2;
    GFont bf = ui_font_label();  // 18B in Big Mode (% + hour labels)
    for (int k = 0; k < 3; ++k) {
      int pct = d->precip[idx[k]];
      if (pct < 0) pct = 0;
      if (pct > 100) pct = 100;
      int bx = bar_x0 + k * (bar_w + bar_gap);
      if (pct == 0) {
        graphics_context_set_fill_color(ctx, theme_muted());
        graphics_fill_rect(ctx, GRect(bx, chart_bot - 3, bar_w, 3), 1, GCornersTop);
      } else {
        int bh = (chart_h - label_top) * pct / 100;
        if (bh < 4) bh = 4;
        graphics_context_set_fill_color(ctx, theme_accent_blue());
        graphics_fill_rect(ctx, GRect(bx, chart_bot - bh, bar_w, bh), 3, GCornersTop);
        if (pct >= 10) {
          char pb[6]; snprintf(pb, sizeof(pb), "%d%%", pct);
          graphics_context_set_text_color(ctx, theme_fg());
          graphics_draw_text(ctx, pb, bf,
              GRect(bx - 6, chart_bot - bh - 20, bar_w + 12, 18),
              GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
        }
      }
      graphics_context_set_text_color(ctx, theme_fg());
      graphics_draw_text(ctx, blabels[k], bf,
          GRect(bx - 6, chart_bot + 2, bar_w + 12, 20),
          GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    }
    ui_draw_auto_banner(ctx, bounds, d->rain_alert_min, d->last_updated,
                        anim_get_frame());
    return;
  }

  // Header icon + label.
  int header_y = UI_HEADER_Y;
  const char *label = "PRECIPITATION";
  GFont hf = ui_font_header();
  GSize tsize = graphics_text_layout_get_content_size(label, hf,
      GRect(0,0,W,UI_HEADER_HEIGHT), GTextOverflowModeTrailingEllipsis,
      GTextAlignmentLeft);
  int icon_w = 18;
  int gap = 5;
  int total_w = icon_w + gap + tsize.w;
  int start_x = ox + (W - total_w) / 2;
  icon_draw_cloud_rain(ctx,
      GPoint(start_x + icon_w/2, header_y + UI_HEADER_HEIGHT/2),
      icon_w, PRECIP_ICON_CLOUD, PRECIP_ICON_DROP);
  graphics_context_set_text_color(ctx, theme_fg());
  graphics_draw_text(ctx, label, hf,
      GRect(start_x + icon_w + gap, header_y, tsize.w + 4, UI_HEADER_HEIGHT),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  // 5 vertical bars. Use more vertical room than before; reserve room
  // for a per-bar % label above the top of the tallest possible bar.
  // Phase 4.7: tighten top label gap (+20 -> +10) for taller bars
  // without moving any other UI element (header, hour labels, banner,
  // and indicator are unchanged).
  const int n = 5;
  int chart_top = header_y + UI_HEADER_HEIGHT + 10;
#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
  // Both the chart's floor and the bars' width were the large classes' numbers.
  //
  // Vertically, `H - 86` is gabbro's constant. On chalk it put the chart floor
  // at 94 with the pill at 140, so a 100%-probability bar was 18px tall while
  // 26px of the card sat empty underneath it (#41). Derive the floor from the
  // pill instead, leaving exactly the hour-label row between them: chalk's
  // chart goes 36px -> 60px, and rect's 60 -> 62.
  //
  // Horizontally, 5x22+4x6 = 134 on a 144px screen starts the block at x=5 and
  // runs to 139, against the card's 12px margin (#39) — and the hour labels
  // overhang the bars, so the real ink went to x=1. Solve for the width that
  // exists at the LABEL row (the lowest, and on chalk the narrowest, thing the
  // chart draws) and let the bars follow: budgeting one full gap of slack makes
  // the label strip exactly `avail_w` wide, so labels tile without overlapping
  // and the outermost ones land on the margin rather than past it.
  const int label_h = 18;
  int chart_bot = ui_content_bottom(bounds) - label_h;
  int chart_h = chart_bot - chart_top;
  int avail_w = ui_band_w(bounds, chart_bot + 2, label_h);
  int bar_gap = 8;
  int bar_w = (avail_w - n * bar_gap) / n;
  if (bar_w < 10) bar_w = 10;
#else
  int chart_bot = PBL_IF_ROUND_ELSE(H - 86, H - 66);
  int chart_h = chart_bot - chart_top;
  int bar_w = PBL_IF_ROUND_ELSE(24, 22);
  int bar_gap = PBL_IF_ROUND_ELSE(8, 6);
#endif
  int total_bars_w = n * bar_w + (n - 1) * bar_gap;
  int bar_x0 = ox + (W - total_bars_w) / 2;

  const char *labels[5] = { "Now", "+1h", "+2h", "+3h", "+4h" };
  GFont label_font = ui_font_label();
  GFont pct_font = ui_font_label();

  for (int i = 0; i < n; ++i) {
    int pct = d->precip[i];
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    int bx = bar_x0 + i * (bar_w + bar_gap);

    if (pct == 0) {
      // Flat 2px muted baseline so empty bars are still visible.
      graphics_context_set_fill_color(ctx, theme_muted());
      graphics_fill_rect(ctx, GRect(bx, chart_bot - 2, bar_w, 2),
                         1, GCornersTop);
    } else {
      // Reserve 18px of headroom above a full-height bar for the % label so
      // a 100% bar's label clears the header box (matches the Big Mode path).
      int bh = (chart_h - 18) * pct / 100;
      if (bh < 3) bh = 3; // legibility floor only for nonzero values
      graphics_context_set_fill_color(ctx, theme_accent_blue());
      graphics_fill_rect(ctx, GRect(bx, chart_bot - bh, bar_w, bh),
                         2, GCornersTop);
      // % label above bar (only when probability is meaningfully large).
      if (pct >= 10) {
        char pct_buf[6]; snprintf(pct_buf, sizeof(pct_buf), "%d%%", pct);
        graphics_context_set_text_color(ctx, theme_fg());
        graphics_draw_text(ctx, pct_buf, pct_font,
#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
            GRect(bx - PRECIP_LBL_PAD, chart_bot - bh - 18, bar_w + bar_gap + 2 * PRECIP_LBL_PAD, 16),
#else
            GRect(bx - 4, chart_bot - bh - 18, bar_w + 8, 16),
#endif
            GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
      }
    }

    // Hour label below.
    graphics_context_set_text_color(ctx, theme_fg());
    graphics_draw_text(ctx, labels[i], label_font,
#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
        GRect(bx - PRECIP_LBL_PAD, chart_bot + 2, bar_w + bar_gap + 2 * PRECIP_LBL_PAD, 18),
#else
        GRect(bx - 4, chart_bot + 2, bar_w + 8, 18),
#endif
        GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }

  ui_draw_auto_banner(ctx, bounds, d->rain_alert_min, d->last_updated,
                      anim_get_frame());
}

