#include "cards.h"
#include "../theme.h"
#include "../icons.h"
#include "../ui.h"
#include "../weather_data.h"
#include "../anim.h"
#include <stdio.h>

void card_main_draw(GContext *ctx, GRect bounds) {
  WeatherData *d = weather_data_get();
  int W = bounds.size.w;
  int H = bounds.size.h;
  int margin = UI_MARGIN_X;
  // Phase 10F slide transition: card draws into a bounds rect that may
  // be horizontally offset. Translate every X coordinate by ox so the
  // entire card moves as one rigid unit during the slide.
  int ox = bounds.origin.x;
  // Nudge the main-card layout down. `yshift` is the base shift applied to
  // the whole card; round (Gabbro) adds further tiers on top.
  //   yshift     — base for the bottom group (wind/humidity row + banner)
  //                and, via the tiers below, the entire card. Round 5px,
  //                rect (Emery) 2px so the whole main page sits 2px lower.
  //                The shared page indicator overlay matches in nav.c.
  //   hero_shift — temp / hi-lo / FEELS: yshift + 3px on round.
  //   top_shift  — weather icon: hero_shift + 9px on round.
  //   loc_shift  — location label: top_shift + 2px on round (a touch lower
  //                than the icon).
  int yshift = PBL_IF_ROUND_ELSE(5, 2);
  int hero_shift = yshift + PBL_IF_ROUND_ELSE(3, 0);
  int top_shift = hero_shift + PBL_IF_ROUND_ELSE(9, 0);
  int loc_shift = top_shift + PBL_IF_ROUND_ELSE(2, 0);

  // Layout anchors for the hero block (icon → temp/hi-lo → FEELS) and the
  // wind/humidity row. When the location label is hidden (Clay "Show location"
  // OFF, the default) the hero block re-centers vertically between the top of
  // the screen and the wind row, so the card isn't top-heavy with the location
  // gone. The wind row, pill and page indicator stay put — nothing below moves
  // and no other card is affected. When location is ON, block_shift is 0 and
  // the layout is identical to the location-enabled design.
  int icon_size = 48;
  int temp_h = 50;
  int feels_lift = PBL_IF_ROUND_ELSE(0, 3);
  int icon_y = PBL_IF_ROUND_ELSE(32, 26) + top_shift;
  int temp_y = PBL_IF_ROUND_ELSE(60, 60) + hero_shift;
  int row_y  = PBL_IF_ROUND_ELSE(H - 104, H - 84) + yshift;
  int block_top = icon_y;
  int block_bottom = temp_y + temp_h - feels_lift + 30; // FEELS box bottom
  int block_h = block_bottom - block_top;
  int block_shift = d->show_location ? 0
                    : (((row_y - block_h) / 2) - block_top);
  icon_y += block_shift;
  temp_y += block_shift;
  // Gabbro, no-location mode only: drop the temp / hi-lo / FEELS cluster 8px
  // below the centered icon for a more balanced split. Icon stays put; the
  // location-on look and Emery are unchanged. hi-lo and FEELS derive from
  // temp_y, so they follow.
  int temp_drop = PBL_IF_ROUND_ELSE(d->show_location ? 0 : 8, 0);
  temp_y += temp_drop;

  // Location name header, centered at the very top. Reads as a quiet label
  // (muted) above the hero icon. On round the usable width near y=0-20 is
  // only ~110-120px (the circle narrows), so cap the box and let long names
  // ellipsize; rect gets the full margin-to-margin width. Skipped entirely
  // when no name has arrived yet so we don't reserve dead space.
  if (d->show_location && d->location_name[0]) {
    int loc_w = PBL_IF_ROUND_ELSE(120, W - 2 * margin);
    int loc_x = ox + (W - loc_w) / 2;
    // Foreground color: black on the light theme, white on dark.
    graphics_context_set_text_color(ctx, theme_fg());
    graphics_draw_text(ctx, d->location_name,
                       fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                       GRect(loc_x, 2 + loc_shift, loc_w, 18),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);
  }

  // Hero condition icon, centered, top (icon_y computed above).
  icon_draw_condition_animated(ctx, GPoint(ox + W / 2, icon_y + icon_size/2),
                               icon_size, d->condition, anim_get_frame());

  // Big temperature "72°" — left-aligned at the margin so it doesn't
  // collide with the Hi/Lo column on the right. Symmetric margins on
  // both sides of the card; the *visible* text just takes whatever
  // width LECO_42 needs.
  char temp_buf[8];
  snprintf(temp_buf, sizeof(temp_buf), "%d°", d->temp);
  GFont temp_font = fonts_get_system_font(FONT_KEY_LECO_42_NUMBERS);
  int hilo_w = 52;
  GRect temp_r = GRect(bounds.origin.x + margin, temp_y,
                       W - 2 * margin - hilo_w, temp_h);
  graphics_context_set_text_color(ctx, theme_fg());
  graphics_draw_text(ctx, temp_buf, temp_font, temp_r,
                     GTextOverflowModeFill, GTextAlignmentLeft, NULL);

  // Hi/Lo column on the right (symmetric margin from right edge).
  // Right-align the values so visible "64°/33°" hug W-margin, matching
  // the temp text on the left which hugs the left margin. This makes the
  // edge whitespace symmetric without changing the orientation/structure.
  int hilo_x = bounds.origin.x + W - margin - hilo_w;
  // Up arrow + "5°"
  icon_draw_arrow_up(ctx, GPoint(hilo_x, temp_y + 10), 10,
                     theme_accent_orange());
  char hi_buf[8]; snprintf(hi_buf, sizeof(hi_buf), "%d°", d->high);
  graphics_context_set_text_color(ctx, theme_accent_orange());
  graphics_draw_text(ctx, hi_buf, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(hilo_x + 10, temp_y + 2, hilo_w - 10, 22),
                     GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentRight, NULL);
  // Down arrow + "2°"
  icon_draw_arrow_down(ctx, GPoint(hilo_x, temp_y + 32), 10,
                       theme_accent_blue());
  char lo_buf[8]; snprintf(lo_buf, sizeof(lo_buf), "%d°", d->low);
  graphics_context_set_text_color(ctx, theme_accent_blue());
  graphics_draw_text(ctx, lo_buf, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(hilo_x + 10, temp_y + 24, hilo_w - 10, 22),
                     GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentRight, NULL);

  // "FEELS 75°" centered below big temp — bumped to 24_BOLD.
  // feels_lift (rect-only 3px) is computed with the layout anchors above.
  char feels_buf[16];
  snprintf(feels_buf, sizeof(feels_buf), "FEELS %d°", d->feels_like);
  graphics_context_set_text_color(ctx, theme_fg());
  graphics_draw_text(ctx, feels_buf,
                     fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                     GRect(ox, temp_y + temp_h - feels_lift, W, 30),
                     GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);

  // Bottom split row: wind | humidity (row_y computed with the anchors above;
  // it stays fixed regardless of the location toggle). Sits above the banner.
  // Vertical divider.
  graphics_context_set_stroke_color(ctx, theme_muted());
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, GPoint(ox + W/2, row_y), GPoint(ox + W/2, row_y + 32));

  // Wind (left column). Speed unit follows the user's selected system.
  icon_draw_wind(ctx, GPoint(ox + W/4, row_y + 8), 22, theme_fg());
  char wind_buf[16];
  const char *wind_unit = (d->units == UNITS_METRIC) ? "KMH" : "MPH";
  snprintf(wind_buf, sizeof(wind_buf), "%d%s %s",
           d->wind_speed, wind_unit, d->wind_dir);
  graphics_context_set_text_color(ctx, theme_fg());
  graphics_draw_text(ctx, wind_buf,
                     fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(ox, row_y + 16, W/2, 22),
                     GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);

  // Humidity / dew point (right column). User can toggle which one
  // appears via the Clay "Show dew point" switch.
  icon_draw_droplet(ctx, GPoint(ox + W*3/4, row_y + 8), 18, theme_accent_blue());
  char hum_buf[8];
  if (d->use_dew_point) {
    snprintf(hum_buf, sizeof(hum_buf), "%d°", d->dew_point);
  } else {
    snprintf(hum_buf, sizeof(hum_buf), "%d%%", d->humidity);
  }
  graphics_draw_text(ctx, hum_buf,
                     fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(ox + W/2, row_y + 16, W/2, 22),
                     GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);

  // Rotating status banner (rain ⇄ updated). The lowered pill position now
  // lives in ui_draw_status_banner's pad_bottom so every card matches, so we
  // hand it the plain bounds here (no per-card shift).
  ui_draw_auto_banner(ctx, bounds, d->rain_alert_min, d->last_updated,
                      anim_get_frame());
}
