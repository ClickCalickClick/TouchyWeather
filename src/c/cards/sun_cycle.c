#include "cards.h"
#include "../theme.h"
#include "../icons.h"
#include "../ui.h"
#include "../anim.h"
#include "../weather_data.h"

// Returns the x at which the (icon + gap + text) cluster's left edge
// should sit so the cluster is horizontally centered within `bounds`.
// `UI_MARGIN_X` is enforced as a minimum safe inset.
static int prv_cluster_start_x(GRect bounds, int icon_size, int gap,
                               int text_w) {
  int W = bounds.size.w;
  int cluster_w = icon_size + gap + text_w;
  int start = bounds.origin.x + (W - cluster_w) / 2;
  int floor_x = bounds.origin.x + UI_MARGIN_X;
  return start < floor_x ? floor_x : start;
}

void card_sun_cycle_draw(GContext *ctx, GRect bounds) {
  WeatherData *d = weather_data_get();
  int W = bounds.size.w;
  int H = bounds.size.h;

  // Header — horizon-sun icon for grid consistency with other cards.
  int header_y = UI_HEADER_Y;
  ui_draw_card_header_with_icon(ctx, bounds, "SUN CYCLE",
                                theme_fg(),
                                header_y, 18, icon_draw_horizon_sun);

  // Stacked rows: sunrise on top, sunset below. Icon on the left,
  // time on the right of each row. To equalize edge whitespace, we
  // measure each time text and treat (icon + gap + time) as a single
  // cluster, centered horizontally in the card.
  // Small classes (144x168 / 180x180): the 40px icon + 56px row pitch pushed
  // the sunset row under the bottom banner. Shrink the icon and row pitch and
  // tighten the top offset so both rows clear the banner (Phase 5.2). Large
  // classes keep verbatim values -> emery/gabbro byte-identical.
#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
  int icon_size = 36;
#else
  int icon_size = 40;
#endif
  int gap = 10;
#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
  int row_h = 42;
  int top_row_y = header_y + UI_HEADER_HEIGHT + 6;
#else
  int row_h = 56;
  int top_row_y = header_y + UI_HEADER_HEIGHT + PBL_IF_ROUND_ELSE(20, 14);
#endif
  int bot_row_y = top_row_y + row_h;
  GFont time_font = ui_font_title();

  // --- Sunrise row ---
  GSize sr_size = graphics_text_layout_get_content_size(d->sunrise, time_font,
      GRect(0, 0, W, 36), GTextOverflowModeTrailingEllipsis,
      GTextAlignmentLeft);
  int sr_start = prv_cluster_start_x(bounds, icon_size, gap, sr_size.w);
  icon_draw_sunrise(ctx, GPoint(sr_start + icon_size/2, top_row_y + icon_size/2),
                    icon_size, theme_accent_orange());
  graphics_context_set_text_color(ctx, theme_fg());
  graphics_draw_text(ctx, d->sunrise, time_font,
      GRect(sr_start + icon_size + gap, top_row_y + 4, sr_size.w + 4, 36),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  // Dotted separator between rows (symmetric to UI_MARGIN_X).
  ui_draw_dotted_hline(ctx, bounds.origin.x + UI_MARGIN_X + 6,
                       bounds.origin.x + W - UI_MARGIN_X - 6,
                       top_row_y + row_h - 4, theme_muted());

  // --- Sunset row ---
  GSize ss_size = graphics_text_layout_get_content_size(d->sunset, time_font,
      GRect(0, 0, W, 36), GTextOverflowModeTrailingEllipsis,
      GTextAlignmentLeft);
  int ss_start = prv_cluster_start_x(bounds, icon_size, gap, ss_size.w);
  icon_draw_sunset(ctx, GPoint(ss_start + icon_size/2, bot_row_y + icon_size/2),
                   icon_size, theme_accent_blue());
  graphics_draw_text(ctx, d->sunset, time_font,
      GRect(ss_start + icon_size + gap, bot_row_y + 4, ss_size.w + 4, 36),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  (void)H;

  ui_draw_auto_banner(ctx, bounds, d->rain_alert_min, d->last_updated,
                      anim_get_frame());
}

