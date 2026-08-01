#include "cards.h"
#include "../theme.h"
#include "../icons.h"
#include "../ui.h"
#include "../settings.h"
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

  // --- Big Mode (Stage B): fewer, bigger day rows. ---
  // 3 days (small classes) / 5 days (large) of day + ↑high ↓low. The arrows
  // carry the hi-vs-lo meaning (accents are fg in Big Mode), replacing the
  // Normal card's hue-coded high / muted-low + "/" separator. Two temperatures
  // per row is wide, so the row uses ui_font_header (18B small / 24B large,
  // a tier below 6 Hours' single-temp rows) and drops the condition icon on
  // the tight 144/180px small classes. POP% and days 4–5-on-small drop — all
  // still in the Week detail modal. Mandatory status banner kept. Returns early
  // -> Normal 5-row layout below untouched, Big-OFF pixel-identical.
  if (settings_get_big_mode()) {
    GFont rf = ui_font_header();  // 18B small / 24B large in Big Mode
    int n_days, row_icon, row_h, top_y, arrow, hdr_icon;
#if defined(UI_SCREEN_SMALL_RECT)
    n_days = 3; row_icon = 0;  row_h = 26; top_y = UI_HEADER_Y + UI_HEADER_HEIGHT + 6;  arrow = 10; hdr_icon = 18;
#elif defined(UI_SCREEN_SMALL_ROUND)
    n_days = 3; row_icon = 0;  row_h = 26; top_y = UI_HEADER_Y + UI_HEADER_HEIGHT + 6;  arrow = 10; hdr_icon = 18;
#elif defined(UI_SCREEN_LARGE_RECT)
    n_days = 5; row_icon = 20; row_h = 26; top_y = UI_HEADER_Y + UI_HEADER_HEIGHT + 14; arrow = 12; hdr_icon = 18;
#else  // UI_SCREEN_LARGE_ROUND
    n_days = 5; row_icon = 22; row_h = 27; top_y = UI_HEADER_Y + UI_HEADER_HEIGHT + 14; arrow = 12; hdr_icon = 18;
#endif
    ui_draw_card_header_with_icon(ctx, bounds, "WEEK AHEAD", theme_fg(),
                                  UI_HEADER_Y, hdr_icon, icon_draw_calendar);

    // Uniform day / high / low column widths across the sampled days.
    int day_w = 0, hi_w = 0, lo_w = 0;
    char b[12];
    for (int i = 0; i < n_days; ++i) {
      GSize ds = graphics_text_layout_get_content_size(d->days_label[i], rf,
          GRect(0, 0, W, 30), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
      if (ds.w > day_w) day_w = ds.w;
      snprintf(b, sizeof(b), "%d°", d->days_high[i]);
      GSize hs = graphics_text_layout_get_content_size(b, rf,
          GRect(0, 0, W, 30), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
      if (hs.w > hi_w) hi_w = hs.w;
      snprintf(b, sizeof(b), "%d°", d->days_low[i]);
      GSize ls = graphics_text_layout_get_content_size(b, rf,
          GRect(0, 0, W, 30), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
      if (ls.w > lo_w) lo_w = ls.w;
    }
    int gap = 8, ag = 3, group_gap = 12;
    int icon_col = row_icon ? (row_icon + gap) : 0;
    int cluster_w = day_w + gap + icon_col + arrow + ag + hi_w + group_gap + arrow + ag + lo_w;
    int cluster_x = bounds.origin.x + (W - cluster_w) / 2;
    int floor_x = bounds.origin.x + UI_MARGIN_X;
    if (cluster_x < floor_x) cluster_x = floor_x;
    int th = row_h - 2;
    for (int i = 0; i < n_days; ++i) {
      int row_y = top_y + i * row_h;
      int cy = row_y + th / 2 - 2;
      int x = cluster_x;
      graphics_context_set_text_color(ctx, theme_fg());
      graphics_draw_text(ctx, d->days_label[i], rf,
          GRect(x, row_y, day_w + 4, th),
          GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
      x += day_w + gap;
      if (row_icon) {
        icon_draw_condition(ctx, GPoint(x + row_icon / 2, cy), row_icon, d->days_cond[i]);
        x += row_icon + gap;
      }
      icon_draw_arrow_up(ctx, GPoint(x + arrow / 2, cy), arrow, theme_fg());
      x += arrow + ag;
      snprintf(b, sizeof(b), "%d°", d->days_high[i]);
      graphics_draw_text(ctx, b, rf, GRect(x, row_y, hi_w + 4, th),
          GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
      x += hi_w + group_gap;
      icon_draw_arrow_down(ctx, GPoint(x + arrow / 2, cy), arrow, theme_fg());
      x += arrow + ag;
      snprintf(b, sizeof(b), "%d°", d->days_low[i]);
      graphics_draw_text(ctx, b, rf, GRect(x, row_y, lo_w + 4, th),
          GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    }
    ui_draw_auto_banner(ctx, bounds, d->rain_alert_min, d->last_updated,
                        anim_get_frame());
    return;
  }

  ui_draw_card_header_with_icon(ctx, bounds, "WEEK AHEAD",
                                theme_fg(),
                                UI_HEADER_Y, 18, icon_draw_calendar);

  // Small classes (144x168 / 180x180): 5 day-rows in the 18px header font
  // overflow the bottom banner (the last 1-2 days were buried). Drop to the
  // 14px bold label font and a 16px row height so all 5 fit; the top_y clamp
  // below then floors the block just under the header (Phase 5.2). Large
  // classes keep verbatim values -> emery/gabbro byte-identical.
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
  int gap = 6;
#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
  int row_h = 16;
#else
  int row_h = PBL_IF_ROUND_ELSE(28, 26);
#endif

  // Vertically center the block of rows in the region between the bottom of
  // the "WEEK AHEAD" header ink and the top of the "UPDATED" pill. The header
  // text is GOTHIC_18 whose ink bottom sits ~18px below UI_HEADER_Y (the 24px
  // header box is taller than the glyphs). Pill geometry mirrors
  // ui_draw_status_banner(). The row's visible ink starts ~4px below its
  // row_y, and the block's visible ink height (first ink top -> last ink
  // bottom) is (DAY_COUNT-1)*row_h + 11, so we offset by those to center the
  // *ink* rather than the layout boxes.
  int header_ink_bottom = UI_HEADER_Y + 18;
  // Phase 4 carry-over (#34): the small classes take the pill's real geometry
  // from the shared accessor instead of this file's private guess, which was
  // 22px wrong on chalk (it believed the pill top was 118 when it is 140, so
  // the "centre the block" maths below centred against the wrong region). The
  // large classes are locked, so they keep the wrong-but-shipped expression
  // verbatim and never call the accessor. See ui.h.
#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
  int pill_top = ui_status_pill_top(bounds);
#else
  int banner_pad_bottom = PBL_IF_ROUND_ELSE(40, 22);
  int banner_h = 22;
  int pill_top = bounds.size.h - banner_pad_bottom - banner_h;
#endif
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

#if defined(UI_SCREEN_SMALL_RECT)
  // D3 — drop the pop% column, KEEP the condition icon.
  //
  // The full row measures ~159px against 120px usable, and the old left-only
  // clamp let the overflow run off the right edge: "70%" rendered as "7(" and
  // "65%" as "6!" (#31). D2's "keep precip" does not map here, because on this
  // card the overflowing column IS the precip column — so the two goals are in
  // direct conflict and the tie goes to the icon: rain probability already has
  // a dedicated card two swipes away, while the condition icon is the only
  // at-a-glance signal in a Week row.
  //
  // Measured, not hardcoded, for the same reason as hours.c. Chalk fits
  // everything and is deliberately excluded.
  const int usable = W - 2 * UI_MARGIN_X;
  if (any_pop && cluster_w > usable) {
    any_pop = false;        // the draw loop's own `any_pop` gate collapses it
    pop_col_w = 0;
    cluster_w = day_max.w + gap + icon_size + gap +
                low_max.w + 4 + sep.w + 4 + high_max.w;
  }
  // Backstop for pathological data (sub-zero lows widen every temp column).
  if (cluster_w > usable) {
    gap = 4;
    cluster_w = day_max.w + gap + icon_size + gap +
                low_max.w + 4 + sep.w + 4 + high_max.w +
                (any_pop ? (gap + pop_col_w) : 0);
  }
#endif

  int cluster_x = bounds.origin.x + (W - cluster_w) / 2;
  int floor_x = bounds.origin.x + UI_MARGIN_X;
  if (cluster_x < floor_x) cluster_x = floor_x;

  for (int i = 0; i < DAY_COUNT; ++i) {
    int row_y = top_y + i * row_h;
#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
    int center_y = row_y + 8;   // center icons on the shorter 14px small-class row
#else
    int center_y = row_y + 11;
#endif

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
