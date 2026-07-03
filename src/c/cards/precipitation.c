#include "cards.h"
#include "../theme.h"
#include "../icons.h"
#include "../ui.h"
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
      int bh = chart_h * pct / 100;
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

