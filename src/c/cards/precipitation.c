#include "cards.h"
#include "../theme.h"
#include "../icons.h"
#include "../ui.h"
#include "../settings.h"
#include "../weather_data.h"
#include "../anim.h"
#include <stdio.h>

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
        bicon, theme_accent_blue(), theme_accent_blue());
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
      icon_w, theme_accent_blue(), theme_accent_blue());
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
  int chart_bot = PBL_IF_ROUND_ELSE(H - 86, H - 66);
  int chart_h = chart_bot - chart_top;
  int bar_w = PBL_IF_ROUND_ELSE(24, 22);
  int bar_gap = PBL_IF_ROUND_ELSE(8, 6);
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
            GRect(bx - 4, chart_bot - bh - 18, bar_w + 8, 16),
            GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
      }
    }

    // Hour label below.
    graphics_context_set_text_color(ctx, theme_fg());
    graphics_draw_text(ctx, labels[i], label_font,
        GRect(bx - 4, chart_bot + 2, bar_w + 8, 18),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }

  ui_draw_auto_banner(ctx, bounds, d->rain_alert_min, d->last_updated,
                      anim_get_frame());
}

