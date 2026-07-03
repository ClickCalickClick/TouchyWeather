#include "cards.h"
#include "../theme.h"
#include "../icons.h"
#include "../ui.h"
#include "../anim.h"
#include "../weather_data.h"

// Phase 11: Golden Hour card.
//
// Displays the four chronological "magic light" milestones for the
// current day:
//
//   ▮ BLUE  5:42 AM  (morning blue hour begins, sun -6°)
//   ▯ GOLD  6:14 AM  (morning golden hour begins, sun -0.833°)
//   ▯ GOLD  7:32 PM  (evening golden hour begins, sun +6°)
//   ▮ BLUE  8:18 PM  (evening blue hour begins, sun -0.833°)
//
// Layout pattern follows the Sun Cycle card: small color-swatch icon +
// label + time, with all rows vertically equally spaced and uniform
// column widths so times line up cleanly.

typedef struct {
  const char *kind;     // "BLUE" or "GOLD"
  const char *time_str; // h:MM AM/PM
  GColor swatch;        // pill color (yellow/orange for gold, blue for blue)
} GHRow;

static int prv_max_w(GFont f, const char *labels[], int n) {
  int max_w = 0;
  for (int i = 0; i < n; ++i) {
    GSize s = graphics_text_layout_get_content_size(labels[i], f,
        GRect(0, 0, 200, 30), GTextOverflowModeTrailingEllipsis,
        GTextAlignmentLeft);
    if (s.w > max_w) max_w = s.w;
  }
  return max_w;
}

void card_golden_hour_draw(GContext *ctx, GRect bounds) {
  WeatherData *d = weather_data_get();
  int W = bounds.size.w;
  int ox = bounds.origin.x;

  // Header.
  int header_y = UI_HEADER_Y;
  ui_draw_card_header_with_icon(ctx, bounds, "GOLDEN HOUR",
                                theme_fg(),
                                header_y, 18, icon_draw_horizon_sun);

  // Build the 4 rows in chronological order. The "swatch" is the small
  // colored pill drawn before the label to communicate band identity.
  GHRow rows[4] = {
    { "BLUE", d->blue_am, theme_accent_blue()   },
    { "GOLD", d->gold_am, theme_accent_orange() },
    { "GOLD", d->gold_pm, theme_accent_orange() },
    { "BLUE", d->blue_pm, theme_accent_blue()   },
  };

  // Uniform column widths: measure max kind-label and max time-string
  // so all four rows align cleanly regardless of "AM"/"PM" character
  // count.
  GFont label_font = ui_font_header();
  GFont time_font  = ui_font_header();

  const char *kinds[4] = { rows[0].kind, rows[1].kind, rows[2].kind, rows[3].kind };
  const char *times[4] = { rows[0].time_str, rows[1].time_str, rows[2].time_str, rows[3].time_str };
  int kind_w = prv_max_w(label_font, kinds, 4);
  int time_w = prv_max_w(time_font, times, 4);

  // Layout constants.
  int swatch_w = 8;
  int swatch_h = 14;
  int gap = 8;
  int cluster_w = swatch_w + gap + kind_w + gap + time_w;
  int cluster_x = ox + (W - cluster_w) / 2;
  int floor_x = ox + UI_MARGIN_X;
  if (cluster_x < floor_x) cluster_x = floor_x;

  // Vertical layout: distribute 4 rows in the area below the header.
  int top_y = header_y + UI_HEADER_HEIGHT + PBL_IF_ROUND_ELSE(14, 10);
  int row_h = PBL_IF_ROUND_ELSE(28, 26);

  for (int i = 0; i < 4; ++i) {
    int y = top_y + i * row_h;

    // Color swatch — small filled pill on the left.
    GRect sw = GRect(cluster_x, y + 4, swatch_w, swatch_h);
    graphics_context_set_fill_color(ctx, rows[i].swatch);
    graphics_fill_rect(ctx, sw, 2, GCornersAll);

    // Kind label ("BLUE" / "GOLD") — colored to match swatch for legibility.
    int label_x = cluster_x + swatch_w + gap;
    graphics_context_set_text_color(ctx, rows[i].swatch);
    graphics_draw_text(ctx, rows[i].kind, label_font,
        GRect(label_x, y, kind_w + 4, 22),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

    // Time — primary fg color for max contrast.
    int time_x = label_x + kind_w + gap;
    graphics_context_set_text_color(ctx, theme_fg());
    graphics_draw_text(ctx, rows[i].time_str, time_font,
        GRect(time_x, y, time_w + 4, 22),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }

  ui_draw_auto_banner(ctx, bounds, d->rain_alert_min, d->last_updated,
                      anim_get_frame());
}
