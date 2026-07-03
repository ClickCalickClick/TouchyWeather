#include "cards.h"
#include "../theme.h"
#include "../icons.h"
#include "../ui.h"
#include "../anim.h"
#include "../weather_data.h"
#include <stdio.h>

// Phase 10A: Next 6 Hours card. 6 stacked rows of
// (time, icon, temp, wind, precip). Wind (compass arrow + speed) is shown on
// EVERY row so speed/direction are always glanceable; the rain droplet + amount
// is an additional column that appears only on hours with measurable precip and
// collapses entirely when the whole window is dry. This mirrors the always-on +
// conditional two-column pattern in week.c (high/low always, pop conditional).
//
// Layout strategy: uniform-column cluster centering. We measure the widest
// time, temp, wind, and precip across all 6 rows, then center the
// `time | icon | temp | wind | precip` cluster horizontally. Rows align
// because each column has a uniform width. Gaps are tightened on rect (emery)
// so the two context columns still fit within the 176px usable width.

static GColor cond_accent(WeatherCondition cond) {
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

// Wind speed text for row i (always shown). Direction is the compass arrow.
static void fmt_wind(const WeatherData *d, int i, char *buf, size_t n) {
  snprintf(buf, n, "%d", d->hours_wind[i]);
}

// Precip amount for row i; returns false (empty buf) when the hour is dry.
static bool fmt_precip(const WeatherData *d, int i, char *buf, size_t n) {
  if (d->hours_precip_x10[i] <= 0) { buf[0] = '\0'; return false; }
  if (d->units == UNITS_METRIC) {
    int mm = (d->hours_precip_x10[i] + 9) / 10; // ceil, always >= 1
    snprintf(buf, n, "%dmm", mm);
  } else {
    int x = d->hours_precip_x10[i];
    snprintf(buf, n, "%d.%d\"", x / 10, x % 10);
  }
  return true;
}

void card_hours_draw(GContext *ctx, GRect bounds) {
  WeatherData *d = weather_data_get();
  int W = bounds.size.w;

  ui_draw_card_header_with_icon(ctx, bounds, "6 HOURS",
                                theme_fg(),
                                UI_HEADER_Y, 18, icon_draw_clock);

  // Tighter row font so 6 rows fit without crowding the page indicator.
  // Small classes (144x168 / 180x180): the 18px row font + header offset that
  // fit emery/gabbro push the 5th/6th row under the bottom banner on the
  // shorter screens. Drop the rows to the 14px bold label font and tighten row
  // height + top offset so all 6 hours clear the banner (Phase 5.2). Each
  // variable branches individually (order preserved) so the large classes keep
  // verbatim values and emery/gabbro stay byte-identical.
#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
  GFont row_font = ui_font_label();
#else
  GFont row_font = ui_font_header();
#endif
  GFont pop_font = ui_font_label();
#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
  int icon_size = 14;
#else
  int icon_size = 16;
#endif
  // Gaps are tighter on rect so wind + precip both fit in 176px usable.
  int gap = PBL_IF_ROUND_ELSE(5, 4);
#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
  int row_h = 15;
  int top_y = UI_HEADER_Y + UI_HEADER_HEIGHT + PBL_IF_ROUND_ELSE(0, 2);
#else
  int row_h = PBL_IF_ROUND_ELSE(20, 19);
  int top_y = UI_HEADER_Y + UI_HEADER_HEIGHT + PBL_IF_ROUND_ELSE(10, 6);
#endif

  // Context column glyph widths.
  int drop_icon  = 10;
  int arrow_icon = 12;
  int ctx_text_gap = PBL_IF_ROUND_ELSE(4, 3);

  // Measure the widest time, temp, wind (always), and precip (conditional)
  // across all rows for uniform column widths. The precip column collapses
  // when every hour is dry.
  GSize time_max = GSize(0, 0);
  GSize temp_max = GSize(0, 0);
  int   wind_text_w = 0;    // widest speed text
  int   precip_text_w = 0;  // widest amount text
  bool  any_precip = false;
  char buf[12];
  char wbuf[12];
  char pbuf[12];
  for (int i = 0; i < 6; ++i) {
    GSize ts = graphics_text_layout_get_content_size(d->hours_label[i],
        row_font, GRect(0,0,W,30), GTextOverflowModeTrailingEllipsis,
        GTextAlignmentLeft);
    if (ts.w > time_max.w) time_max = ts;
    snprintf(buf, sizeof(buf), "%d°", d->hours_temp[i]);
    GSize ps = graphics_text_layout_get_content_size(buf, row_font,
        GRect(0,0,W,30), GTextOverflowModeTrailingEllipsis,
        GTextAlignmentLeft);
    if (ps.w > temp_max.w) temp_max = ps;

    fmt_wind(d, i, wbuf, sizeof(wbuf));
    GSize ws = graphics_text_layout_get_content_size(wbuf, pop_font,
        GRect(0,0,W,30), GTextOverflowModeTrailingEllipsis,
        GTextAlignmentLeft);
    if (ws.w > wind_text_w) wind_text_w = ws.w;

    if (fmt_precip(d, i, pbuf, sizeof(pbuf))) {
      any_precip = true;
      GSize prs = graphics_text_layout_get_content_size(pbuf, pop_font,
          GRect(0,0,W,30), GTextOverflowModeTrailingEllipsis,
          GTextAlignmentLeft);
      if (prs.w > precip_text_w) precip_text_w = prs.w;
    }
  }

  int wind_col_w   = arrow_icon + ctx_text_gap + wind_text_w;
  int precip_col_w = drop_icon + ctx_text_gap + precip_text_w;

  int cluster_w = time_max.w + gap + icon_size + gap + temp_max.w +
                  gap + wind_col_w +
                  (any_precip ? (gap + precip_col_w) : 0);
  int cluster_x = bounds.origin.x + (W - cluster_w) / 2;
  int floor_x = bounds.origin.x + UI_MARGIN_X;
  if (cluster_x < floor_x) cluster_x = floor_x;

  for (int i = 0; i < 6; ++i) {
    int row_y = top_y + i * row_h;
    int icon_cx = cluster_x + time_max.w + gap + icon_size / 2;
#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
    int icon_cy = row_y + 8;   // center on the shorter 14px small-class row
#else
    int icon_cy = row_y + 10;
#endif

    graphics_context_set_text_color(ctx, theme_fg());
    graphics_draw_text(ctx, d->hours_label[i], row_font,
        GRect(cluster_x, row_y - 2, time_max.w + 4, 22),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

    icon_draw_condition(ctx, GPoint(icon_cx, icon_cy),
                        icon_size, d->hours_cond[i]);

    snprintf(buf, sizeof(buf), "%d°", d->hours_temp[i]);
    int temp_x = cluster_x + time_max.w + gap + icon_size + gap;
    graphics_context_set_text_color(ctx, cond_accent(d->hours_cond[i]));
    graphics_draw_text(ctx, buf, row_font,
        GRect(temp_x, row_y - 2, temp_max.w + 4, 22),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

    // Wind (always): compass arrow (direction) + speed.
    int wind_x = temp_x + temp_max.w + gap;
    fmt_wind(d, i, wbuf, sizeof(wbuf));
    icon_draw_compass_arrow(ctx,
        GPoint(wind_x + arrow_icon/2, icon_cy),
        arrow_icon, theme_fg(), d->hours_wind_dir[i]);
    graphics_context_set_text_color(ctx, theme_fg());
    graphics_draw_text(ctx, wbuf, pop_font,
        GRect(wind_x + arrow_icon + ctx_text_gap, row_y, wind_text_w + 4, 18),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

    // Precip (only on wet hours): droplet + amount.
    if (any_precip && fmt_precip(d, i, pbuf, sizeof(pbuf))) {
      int precip_x = wind_x + wind_col_w + gap;
      icon_draw_droplet(ctx,
          GPoint(precip_x + drop_icon/2, icon_cy),
          drop_icon, theme_accent_blue());
      graphics_context_set_text_color(ctx, theme_accent_blue());
      graphics_draw_text(ctx, pbuf, pop_font,
          GRect(precip_x + drop_icon + ctx_text_gap, row_y,
                precip_text_w + 4, 18),
          GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    }
  }

  ui_draw_auto_banner(ctx, bounds, d->rain_alert_min, d->last_updated,
                      anim_get_frame());
}
