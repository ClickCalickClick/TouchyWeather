#include "cards.h"
#include "../theme.h"
#include "../icons.h"
#include "../ui.h"
#include "../settings.h"
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

#if defined(UI_SCREEN_SMALL_RECT)
// Drops the leading "0" from an imperial precip amount ("0.2\"" -> ".2\"") —
// step 4 of the D2 cascade below, and the last few pixels it can find before
// it has to drop the column outright. Metric ("3mm") is already minimal and is
// left alone. Hand-rolled rather than using <string.h> so the include itself
// stays out of the locked platforms' translation unit.
static void prv_shorten_precip(char *buf) {
  if (buf[0] != '0' || buf[1] != '.') return;
  int i = 0;
  while (buf[i + 1]) { buf[i] = buf[i + 1]; ++i; }
  buf[i] = '\0';
}
#endif

void card_hours_draw(GContext *ctx, GRect bounds) {
  WeatherData *d = weather_data_get();
  int W = bounds.size.w;

  // --- Big Mode (Stage B): 3 bigger rows instead of 6 dense ones. ---
  // Sample every other hour (0,2,4) so the card still spans the full 6h window
  // (each row prints its own time, so the 2h pitch is self-evident); each row
  // is just time + condition icon + temperature, large. The wind and precip
  // columns and the odd hours all drop — every one of them is still in the
  // "6 Hours" detail modal (SELECT-hold / swipe up). Header renames to "HOURLY"
  // so showing 3 of 6 under "6 HOURS" doesn't lie. Returns early -> the Normal
  // 6-row layout below is untouched and Big-OFF stays pixel-identical.
  if (settings_get_big_mode()) {
    static const int idx[3] = {0, 2, 4};
    GFont row_font = ui_font_body();  // 24B small / 28B large in Big Mode
    int row_icon, row_h, top_y, hdr_icon;
#if defined(UI_SCREEN_SMALL_RECT)
    row_icon = 22; row_h = 28; top_y = UI_HEADER_Y + UI_HEADER_HEIGHT + 4;  hdr_icon = 18;
#elif defined(UI_SCREEN_SMALL_ROUND)
    row_icon = 22; row_h = 26; top_y = UI_HEADER_Y + UI_HEADER_HEIGHT + 4;  hdr_icon = 18;
#elif defined(UI_SCREEN_LARGE_RECT)
    row_icon = 28; row_h = 42; top_y = UI_HEADER_Y + UI_HEADER_HEIGHT + 18; hdr_icon = 22;
#else  // UI_SCREEN_LARGE_ROUND
    row_icon = 28; row_h = 44; top_y = UI_HEADER_Y + UI_HEADER_HEIGHT + 18; hdr_icon = 22;
#endif
    ui_draw_card_header_with_icon(ctx, bounds, "HOURLY", theme_fg(),
                                  UI_HEADER_Y, hdr_icon, icon_draw_clock);

    // Uniform time + temp column widths across the 3 sampled rows, then center
    // the time | icon | temp cluster (same idiom as the Normal layout).
    int time_w = 0, temp_w = 0;
    char tb[12];
    for (int k = 0; k < 3; ++k) {
      int i = idx[k];
      GSize ts = graphics_text_layout_get_content_size(d->hours_label[i], row_font,
          GRect(0, 0, W, 40), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
      if (ts.w > time_w) time_w = ts.w;
      snprintf(tb, sizeof(tb), "%d°", d->hours_temp[i]);
      GSize ps = graphics_text_layout_get_content_size(tb, row_font,
          GRect(0, 0, W, 40), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
      if (ps.w > temp_w) temp_w = ps.w;
    }
    int gap = 10;
    int cluster_w = time_w + gap + row_icon + gap + temp_w;
    int cluster_x = bounds.origin.x + (W - cluster_w) / 2;
    int floor_x = bounds.origin.x + UI_MARGIN_X;
    if (cluster_x < floor_x) cluster_x = floor_x;
    int th = row_h - 2;
    for (int k = 0; k < 3; ++k) {
      int i = idx[k];
      int row_y = top_y + k * row_h;
      int icon_cx = cluster_x + time_w + gap + row_icon / 2;
#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
      // #78 — `th/2 - 2` centres the icon on the text BOX, but GOTHIC_24_BOLD's
      // ink sits low in its box, so the icon floated ~5px above the digits it
      // belongs to and read as part of the row above. Measured on basalt: text
      // ink 44..59 (centre 51.5) against an icon centre of 47. Centre it on the
      // ink instead. Large classes keep the box-centred expression verbatim.
      int icon_cy = row_y + th / 2 + 2;
#else
      int icon_cy = row_y + th / 2 - 2;
#endif
      graphics_context_set_text_color(ctx, theme_fg());
      graphics_draw_text(ctx, d->hours_label[i], row_font,
          GRect(cluster_x, row_y, time_w + 4, th),
          GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
      icon_draw_condition(ctx, GPoint(icon_cx, icon_cy), row_icon, d->hours_cond[i]);
      snprintf(tb, sizeof(tb), "%d°", d->hours_temp[i]);
      int temp_x = cluster_x + time_w + gap + row_icon + gap;
      graphics_draw_text(ctx, tb, row_font,
          GRect(temp_x, row_y, temp_w + 4, th),
          GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    }
    ui_draw_auto_banner(ctx, bounds, d->rain_alert_min, d->last_updated,
                        anim_get_frame());
    return;
  }

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

#if defined(UI_SCREEN_SMALL_RECT)
  // D2 — the measured column cascade.
  //
  // The five-column cluster measures ~156px at the widest data against 120px
  // usable here, and the old code centred it and clamped LEFT only, so the
  // overflow simply ran off the right edge: every precip amount rendered as
  // "0." with the digits gone (#24) and the widest rows lost the last digit of
  // the wind speed too (#25). That is R3, and this replaces it.
  //
  // The decision (D2) is to keep precip and drop WIND. Dropping wind alone
  // only recovers 33px and still leaves ~3px of overflow, so steps 3 and 4 are
  // load-bearing rather than optional — together they find ~9px more, which is
  // what lets precip actually survive at every data width measured. Step 5
  // exists so the card can never clip, not because it is expected to fire.
  //
  // It re-measures after every step instead of hardcoding a column set, so it
  // stays right for long labels, three-digit temps and metric/imperial
  // switches. Chalk has 140px usable, fits all five columns, and is deliberately
  // NOT part of this branch (#30).
  bool show_wind = true;
  bool precip_short = false;
  const int usable = W - 2 * UI_MARGIN_X;
#define HOURS_REMEASURE() do {                                       \
    wind_col_w   = arrow_icon + ctx_text_gap + wind_text_w;          \
    precip_col_w = drop_icon + ctx_text_gap + precip_text_w;         \
    cluster_w = time_max.w + gap + icon_size + gap + temp_max.w +    \
                (show_wind  ? (gap + wind_col_w)   : 0) +            \
                (any_precip ? (gap + precip_col_w) : 0);             \
  } while (0)

  // 1. drop the wind column (compass arrow + speed)
  if (cluster_w > usable) { show_wind = false; HOURS_REMEASURE(); }
  // 2. tighten the inter-column gap and the icon
  if (cluster_w > usable) { gap = 3; icon_size = 13; HOURS_REMEASURE(); }
  // 3. shorten the imperial precip format ("0.2\"" -> ".2\"")
  if (cluster_w > usable && d->units != UNITS_METRIC) {
    precip_short = true;
    precip_text_w = 0;
    for (int i = 0; i < 6; ++i) {
      if (fmt_precip(d, i, pbuf, sizeof(pbuf))) {
        prv_shorten_precip(pbuf);
        GSize prs = graphics_text_layout_get_content_size(pbuf, pop_font,
            GRect(0,0,W,30), GTextOverflowModeTrailingEllipsis,
            GTextAlignmentLeft);
        if (prs.w > precip_text_w) precip_text_w = prs.w;
      }
    }
    HOURS_REMEASURE();
  }
  // 4. last resort — drop precip as well, so the row can never clip
  if (cluster_w > usable) { any_precip = false; HOURS_REMEASURE(); }
  // Whatever the cascade dropped, take wind back if it now fits: a dry window
  // never needed to lose it in the first place.
  if (!show_wind) {
    show_wind = true;
    HOURS_REMEASURE();
    if (cluster_w > usable) { show_wind = false; HOURS_REMEASURE(); }
  }
#undef HOURS_REMEASURE
#endif

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

    // Wind: compass arrow (direction) + speed. Always drawn except where the
    // D2 cascade above dropped the column. The brace is split across the guard
    // so the draw statements themselves stay verbatim for the locked classes.
    int wind_x = temp_x + temp_max.w + gap;
#if defined(UI_SCREEN_SMALL_RECT)
    if (show_wind) {
#endif
    fmt_wind(d, i, wbuf, sizeof(wbuf));
    icon_draw_compass_arrow(ctx,
        GPoint(wind_x + arrow_icon/2, icon_cy),
        arrow_icon, theme_fg(), d->hours_wind_dir[i]);
    graphics_context_set_text_color(ctx, theme_fg());
    graphics_draw_text(ctx, wbuf, pop_font,
        GRect(wind_x + arrow_icon + ctx_text_gap, row_y, wind_text_w + 4, 18),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
#if defined(UI_SCREEN_SMALL_RECT)
    }
#endif

    // Precip (only on wet hours): droplet + amount.
    if (any_precip && fmt_precip(d, i, pbuf, sizeof(pbuf))) {
      int precip_x = wind_x + wind_col_w + gap;
#if defined(UI_SCREEN_SMALL_RECT)
      if (!show_wind) precip_x = wind_x;   // precip takes the dropped slot
      if (precip_short) prv_shorten_precip(pbuf);
#endif
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
