#include "cards.h"
#include "../theme.h"
#include "../icons.h"
#include "../ui.h"
#include "../settings.h"
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

  // --- Big Mode (Stage B): simplified "fewer, bigger elements" main card. ---
  // Condition icon + a huge centered temperature + one hi/lo line (↑high ↓low).
  // Location, FEELS and the wind/humidity split row are dropped for legibility
  // (FEELS is on the Advice card; wind lives in the 6 Hours detail). The arrows
  // carry the hi-vs-lo meaning since the accent colors collapse to fg in Big
  // Mode. Returns early, so the entire Normal layout below is untouched and
  // Big-OFF stays pixel-identical.
  if (settings_get_big_mode()) {
    int icon_size, icon_y, temp_y, hilo_y;
#if defined(UI_SCREEN_SMALL_RECT)
    icon_size = 40; icon_y = 4;  temp_y = 46;  hilo_y = 96;
#elif defined(UI_SCREEN_SMALL_ROUND)
    icon_size = 46; icon_y = 10; temp_y = 54;  hilo_y = 104;
#elif defined(UI_SCREEN_LARGE_RECT)
    icon_size = 60; icon_y = 18; temp_y = 74;  hilo_y = 138;
#else  // UI_SCREEN_LARGE_ROUND
    icon_size = 68; icon_y = 34; temp_y = 100; hilo_y = 168;
#endif
    // Condition icon, centered near the top.
    icon_draw_condition_animated(ctx, GPoint(ox + W / 2, icon_y + icon_size / 2),
                                 icon_size, d->condition, anim_get_frame());
    // Huge temperature, centered. ui_font_number() -> BITHAM_42_BOLD in Big Mode
    // (a full font, so the degree glyph and a sub-zero minus both render).
    char temp_buf[8];
    snprintf(temp_buf, sizeof(temp_buf), "%d°", d->temp);
    graphics_context_set_text_color(ctx, theme_fg());
    graphics_draw_text(ctx, temp_buf, ui_font_number(),
                       GRect(ox, temp_y, W, 50),
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    // One hi/lo line: ↑high  ↓low, centered as a cluster.
    GFont hilo_font = ui_font_body();
    char hi_buf[8], lo_buf[8];
    snprintf(hi_buf, sizeof(hi_buf), "%d°", d->high);
    snprintf(lo_buf, sizeof(lo_buf), "%d°", d->low);
    GSize hi_sz = graphics_text_layout_get_content_size(hi_buf, hilo_font,
        GRect(0, 0, W, 40), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
    GSize lo_sz = graphics_text_layout_get_content_size(lo_buf, hilo_font,
        GRect(0, 0, W, 40), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
    int arrow = (icon_size >= 60) ? 16 : 12;
    int ag = 4;          // arrow -> its number
    int group_gap = 16;  // high group -> low group
    int th = (icon_size >= 60) ? 34 : 28;  // hi/lo text box height
    int hi_group_w = arrow + ag + hi_sz.w;
    int lo_group_w = arrow + ag + lo_sz.w;
    int cluster_w = hi_group_w + group_gap + lo_group_w;
    int cx = ox + (W - cluster_w) / 2;
    int arrow_cy = hilo_y + th / 2 - 2;
    graphics_context_set_text_color(ctx, theme_fg());
    icon_draw_arrow_up(ctx, GPoint(cx + arrow / 2, arrow_cy), arrow, theme_fg());
    graphics_draw_text(ctx, hi_buf, hilo_font,
        GRect(cx + arrow + ag, hilo_y, hi_sz.w + 4, th),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    int lx = cx + hi_group_w + group_gap;
    icon_draw_arrow_down(ctx, GPoint(lx + arrow / 2, arrow_cy), arrow, theme_fg());
    graphics_draw_text(ctx, lo_buf, hilo_font,
        GRect(lx + arrow + ag, hilo_y, lo_sz.w + 4, th),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    // Status banner (enlarged for Big Mode inside ui_draw_status_banner).
    ui_draw_auto_banner(ctx, bounds, d->rain_alert_min, d->last_updated,
                        anim_get_frame());
    return;
  }

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
  // The shift tiers below (yshift/hero_shift/top_shift/loc_shift) and the
  // large icon/temp box are keyed off the taller emery/gabbro screens. On the
  // two small classes those absolutes leave the fixed-from-top hero block
  // (icon → temp/hi-lo → FEELS) and the fixed-from-bottom wind row colliding in
  // the middle — the wind glyph rode up into "FEELS" (Phase 5.2). Give the
  // small classes their own compressed anchors: a smaller icon/temp box and a
  // higher wind row. The no-location re-centering below (block_shift) then
  // vertically centers the hero block above the wind row.
  //
  // Layout anchors for the hero block and the wind/humidity row. When the
  // location label is hidden (Clay "Show location" OFF, the default) the hero
  // block re-centers vertically between the top of the screen and the wind row,
  // so the card isn't top-heavy with the location gone. The wind row, pill and
  // page indicator stay put — nothing below moves and no other card is affected.
  // When location is ON, block_shift is 0 and the layout matches the
  // location-enabled design. Large-rect (emery) / large-round (gabbro) keep the
  // verbatim pre-Phase-5 expressions → both stay pixel-identical.
#if defined(UI_SCREEN_SMALL_RECT)
  // 144x168. temp_h must stay ~44 so FEELS clears the hi/lo column (its low
  // value sits at temp_y+46); a smaller box rides FEELS up into "50°". row_y
  // 86 puts the wind row just below FEELS and just above the banner (top ~126).
  int icon_size = 28;
  int temp_h = 44;
  int feels_lift = 0;
  int icon_y = 2;
  int temp_y = 8;
  int row_y = 86;
  int loc_shift = 0;
#elif defined(UI_SCREEN_SMALL_ROUND)
  // 180x180. Same temp_h clearance rule; a touch more vertical room than
  // small-rect, so the block sits lower and the wind row at 100 clears the
  // small-round banner (top ~140).
  int icon_size = 38;
  int temp_h = 46;
  int feels_lift = 0;
  int icon_y = 8;
  int temp_y = 16;
  int row_y = 100;
  int loc_shift = 0;
#else
  int yshift = PBL_IF_ROUND_ELSE(5, 2);
  int hero_shift = yshift + PBL_IF_ROUND_ELSE(3, 0);
  int top_shift = hero_shift + PBL_IF_ROUND_ELSE(9, 0);
  int loc_shift = top_shift + PBL_IF_ROUND_ELSE(2, 0);
  int icon_size = 48;
  int temp_h = 50;
  int feels_lift = PBL_IF_ROUND_ELSE(0, 3);
  int icon_y = PBL_IF_ROUND_ELSE(32, 26) + top_shift;
  int temp_y = PBL_IF_ROUND_ELSE(60, 60) + hero_shift;
  int row_y  = PBL_IF_ROUND_ELSE(H - 104, H - 84) + yshift;
#endif
  int block_top = icon_y;
  int block_bottom = temp_y + temp_h - feels_lift + 30; // FEELS box bottom
  int block_h = block_bottom - block_top;
  int block_shift = d->show_location ? 0
                    : (((row_y - block_h) / 2) - block_top);
  icon_y += block_shift;
  temp_y += block_shift;
  // Gabbro (large-round), no-location mode only: drop the temp / hi-lo / FEELS
  // cluster 8px below the centered icon for a more balanced split. Icon stays
  // put; the location-on look and Emery are unchanged. hi-lo and FEELS derive
  // from temp_y, so they follow. The small classes use their own compressed
  // anchors above and don't want this gabbro-tuned drop. Kept at this position
  // (after block_shift) so gabbro's codegen is byte-for-byte unchanged.
#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
  int temp_drop = 0;
#else
  int temp_drop = PBL_IF_ROUND_ELSE(d->show_location ? 0 : 8, 0);
#endif
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
                       ui_font_label(),
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
  GFont temp_font = ui_font_number();
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
  graphics_draw_text(ctx, hi_buf, ui_font_header(),
                     GRect(hilo_x + 10, temp_y + 2, hilo_w - 10, 22),
                     GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentRight, NULL);
  // Down arrow + "2°"
  icon_draw_arrow_down(ctx, GPoint(hilo_x, temp_y + 32), 10,
                       theme_accent_blue());
  char lo_buf[8]; snprintf(lo_buf, sizeof(lo_buf), "%d°", d->low);
  graphics_context_set_text_color(ctx, theme_accent_blue());
  graphics_draw_text(ctx, lo_buf, ui_font_header(),
                     GRect(hilo_x + 10, temp_y + 24, hilo_w - 10, 22),
                     GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentRight, NULL);

  // "FEELS 75°" centered below big temp — bumped to 24_BOLD.
  // feels_lift (rect-only 3px) is computed with the layout anchors above.
  char feels_buf[16];
  snprintf(feels_buf, sizeof(feels_buf), "FEELS %d°", d->feels_like);
  graphics_context_set_text_color(ctx, theme_fg());
  graphics_draw_text(ctx, feels_buf,
                     ui_font_body(),
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
                     ui_font_header(),
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
                     ui_font_header(),
                     GRect(ox + W/2, row_y + 16, W/2, 22),
                     GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);

  // Rotating status banner (rain ⇄ updated). The lowered pill position now
  // lives in ui_draw_status_banner's pad_bottom so every card matches, so we
  // hand it the plain bounds here (no per-card shift).
  ui_draw_auto_banner(ctx, bounds, d->rain_alert_min, d->last_updated,
                      anim_get_frame());
}
