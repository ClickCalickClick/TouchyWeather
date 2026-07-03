#include "cards.h"
#include "../theme.h"
#include "../icons.h"
#include "../ui.h"
#include "../anim.h"
#include "../weather_data.h"
#include <stdio.h>

// Phase 10B: Week Ahead card. 4 days of explicit numerics:
//   day-label | cond-icon | low° (muted) "/" high° (color) | droplet+%
// No abstract bar — every value is a number you can read directly.
// Cluster-centered as a uniform-width row so all 4 rows align.
//
// Color rules:
//   - Day label: theme_fg
//   - Low temp:  theme_secondary (de-emphasized but legible)
//   - High temp: orange when sunny/partly-cloudy, blue when wet, fg otherwise
//   - Precip droplet + %: theme_accent_blue, only when prob >= 30
//   - "/" separator: theme_secondary

#define DAY_COUNT     5
#define POP_THRESHOLD 30

static GColor high_color(WeatherCondition cond) {
  switch (cond) {
    case COND_SUNNY:
    case COND_PARTLY_CLOUDY:
      return theme_accent_orange();
    case COND_RAIN:
    case COND_SNOW:
    case COND_STORM:
      return theme_accent_blue();
    default:
      return theme_fg();
  }
}

void card_week_draw(GContext *ctx, GRect bounds) {
  WeatherData *d = weather_data_get();
  int W = bounds.size.w;

  ui_draw_card_header_with_icon(ctx, bounds, "WEEK AHEAD",
                                theme_fg(),
                                UI_HEADER_Y, 18, icon_draw_calendar);

  GFont row_font = ui_font_header();
  GFont pop_font = ui_font_label();
  int icon_size = 16;
  int gap = 6;
  int row_h = PBL_IF_ROUND_ELSE(28, 26);

  // Vertically center the block of rows in the region between the bottom of
  // the "WEEK AHEAD" header ink and the top of the "UPDATED" pill. The header
  // text is GOTHIC_18 whose ink bottom sits ~18px below UI_HEADER_Y (the 24px
  // header box is taller than the glyphs). Pill geometry mirrors
  // ui_draw_status_banner(). The row's visible ink starts ~4px below its
  // row_y, and the block's visible ink height (first ink top -> last ink
  // bottom) is (DAY_COUNT-1)*row_h + 11, so we offset by those to center the
  // *ink* rather than the layout boxes.
  int header_ink_bottom = UI_HEADER_Y + 18;
  int banner_pad_bottom = PBL_IF_ROUND_ELSE(40, 22);
  int banner_h = 22;
  int pill_top = bounds.size.h - banner_pad_bottom - banner_h;
  int region_center = (header_ink_bottom + pill_top) / 2;
  int block_ink_h = (DAY_COUNT - 1) * row_h + 11;
  int row_ink_offset = PBL_IF_ROUND_ELSE(5, 4);
  int top_y = region_center - block_ink_h / 2 - row_ink_offset;
  if (top_y < UI_HEADER_Y + UI_HEADER_HEIGHT + PBL_IF_ROUND_ELSE(8, 4)) {
    top_y = UI_HEADER_Y + UI_HEADER_HEIGHT + PBL_IF_ROUND_ELSE(8, 4);
  }

  // Measure widest day label, low, high, and precip across 4 days for
  // uniform-column layout. Precip column collapses if no day qualifies.
  GSize day_max  = GSize(0, 0);
  GSize low_max  = GSize(0, 0);
  GSize high_max = GSize(0, 0);
  GSize pop_max  = GSize(0, 0);
  bool any_pop = false;
  char buf[12];
  for (int i = 0; i < DAY_COUNT; ++i) {
    GSize ds = graphics_text_layout_get_content_size(d->days_label[i],
        row_font, GRect(0,0,W,30), GTextOverflowModeTrailingEllipsis,
        GTextAlignmentLeft);
    if (ds.w > day_max.w) day_max = ds;

    snprintf(buf, sizeof(buf), "%d°", d->days_low[i]);
    GSize ls = graphics_text_layout_get_content_size(buf, row_font,
        GRect(0,0,W,30), GTextOverflowModeTrailingEllipsis,
        GTextAlignmentLeft);
    if (ls.w > low_max.w) low_max = ls;

    snprintf(buf, sizeof(buf), "%d°", d->days_high[i]);
    GSize hs = graphics_text_layout_get_content_size(buf, row_font,
        GRect(0,0,W,30), GTextOverflowModeTrailingEllipsis,
        GTextAlignmentLeft);
    if (hs.w > high_max.w) high_max = hs;

    if (d->days_pop[i] >= POP_THRESHOLD) {
      any_pop = true;
      snprintf(buf, sizeof(buf), "%d%%", (int)d->days_pop[i]);
      GSize qs = graphics_text_layout_get_content_size(buf, pop_font,
          GRect(0,0,W,30), GTextOverflowModeTrailingEllipsis,
          GTextAlignmentLeft);
      if (qs.w > pop_max.w) pop_max = qs;
    }
  }

  // "/" separator measurement.
  GSize sep = graphics_text_layout_get_content_size("/", row_font,
      GRect(0,0,W,30), GTextOverflowModeTrailingEllipsis,
      GTextAlignmentLeft);

  int pop_icon = 10;
  int pop_col_w = any_pop ? (pop_icon + 4 + pop_max.w) : 0;
  int cluster_w = day_max.w + gap + icon_size + gap +
                  low_max.w + 4 + sep.w + 4 + high_max.w +
                  (any_pop ? (gap + pop_col_w) : 0);
  int cluster_x = bounds.origin.x + (W - cluster_w) / 2;
  int floor_x = bounds.origin.x + UI_MARGIN_X;
  if (cluster_x < floor_x) cluster_x = floor_x;

  for (int i = 0; i < DAY_COUNT; ++i) {
    int row_y = top_y + i * row_h;
    int center_y = row_y + 11;

    // Day label.
    graphics_context_set_text_color(ctx, theme_fg());
    graphics_draw_text(ctx, d->days_label[i], row_font,
        GRect(cluster_x, row_y - 2, day_max.w + 4, 22),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

    // Condition icon.
    int icon_cx = cluster_x + day_max.w + gap + icon_size/2;
    icon_draw_condition(ctx, GPoint(icon_cx, center_y),
                        icon_size, d->days_cond[i]);

    // Low temp (muted secondary).
    int low_x = cluster_x + day_max.w + gap + icon_size + gap;
    snprintf(buf, sizeof(buf), "%d°", d->days_low[i]);
    graphics_context_set_text_color(ctx, theme_secondary());
    graphics_draw_text(ctx, buf, row_font,
        GRect(low_x, row_y - 2, low_max.w + 4, 22),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

    // "/" separator.
    int sep_x = low_x + low_max.w + 4;
    graphics_draw_text(ctx, "/", row_font,
        GRect(sep_x, row_y - 2, sep.w + 4, 22),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

    // High temp (cond color).
    int high_x = sep_x + sep.w + 4;
    snprintf(buf, sizeof(buf), "%d°", d->days_high[i]);
    graphics_context_set_text_color(ctx, high_color(d->days_cond[i]));
    graphics_draw_text(ctx, buf, row_font,
        GRect(high_x, row_y - 2, high_max.w + 4, 22),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

    // Precip column.
    if (any_pop && d->days_pop[i] >= POP_THRESHOLD) {
      int pop_x = high_x + high_max.w + gap;
      icon_draw_droplet(ctx,
          GPoint(pop_x + pop_icon/2, center_y),
          pop_icon, theme_accent_blue());
      snprintf(buf, sizeof(buf), "%d%%", (int)d->days_pop[i]);
      graphics_context_set_text_color(ctx, theme_accent_blue());
      graphics_draw_text(ctx, buf, pop_font,
          GRect(pop_x + pop_icon + 4, row_y, pop_max.w + 4, 18),
          GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    }
  }

  ui_draw_auto_banner(ctx, bounds, d->rain_alert_min, d->last_updated,
                      anim_get_frame());
}
