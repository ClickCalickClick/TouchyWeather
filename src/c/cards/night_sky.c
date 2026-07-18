#include "cards.h"
#include "../theme.h"
#include "../icons.h"
#include "../ui.h"
#include "../settings.h"
#include "../anim.h"
#include "../weather_data.h"
#include <stdio.h>

// Phase 10C: Night Sky card with locked palette.
//
// Why locked colors: prior to this fix, the moon was drawn with
// theme_fg() as the body and theme_bg() as the shadow. In light mode
// that produced a black moon with a white shadow — visually inverted
// and confusing (a "waxing" crescent appeared waning). The phase
// shadow direction is mathematical and shouldn't flip with theme.
//
// Solution: always draw a small dark "sky" disc behind the moon, then
// the moon body in cream and shadow in the same dark sky color. The
// moon now reads correctly in either theme.

void card_night_sky_draw(GContext *ctx, GRect bounds) {
  WeatherData *d = weather_data_get();
  int W = bounds.size.w;

  ui_draw_card_header_with_icon(ctx, bounds, "NIGHT SKY",
                                theme_fg(),
                                UI_HEADER_Y, 18, icon_draw_moon_small);

  // Locked moon palette — never derived from theme.
  const GColor moon_body   = GColorIcterine;     // cream-yellow
  const GColor moon_shadow = GColorOxfordBlue;   // deep navy

  // --- Big Mode (Stage B): bigger moon + phase name, "% LIT" dropped. ---
  // The moon graphic already shows illumination, so the "% LIT" line goes; the
  // moon and both phase-name words grow. The locked cream/navy moon palette is
  // EXEMPT from the Big-Mode contrast collapse — its shadow direction is the
  // data. Mandatory status banner is KEPT (an earlier plan dropped it — not
  // allowed). Returns early -> Normal layout below untouched, Big-OFF identical.
  if (settings_get_big_mode()) {
    int msize, my, n1y, n2y;
#if defined(UI_SCREEN_SMALL_RECT)
    msize = 40; my = 50; n1y = 72; n2y = 100;
#elif defined(UI_SCREEN_SMALL_ROUND)
    msize = 40; my = 66; n1y = 88; n2y = 114;
#elif defined(UI_SCREEN_LARGE_RECT)
    msize = 64; my = 74; n1y = 116; n2y = 150;
#else  // UI_SCREEN_LARGE_ROUND
    msize = 64; my = 92; n1y = 134; n2y = 168;
#endif
    GPoint mc = GPoint(bounds.origin.x + W / 2, my);
    graphics_context_set_fill_color(ctx, moon_shadow);
    graphics_fill_circle(ctx, mc, msize / 2 + 7);
    icon_draw_moon_phase(ctx, mc, msize, d->moon_phase, d->moon_illum,
                         moon_body, moon_shadow);
    graphics_context_set_text_color(ctx, theme_fg());
    graphics_draw_text(ctx, d->moon_name1, ui_font_body(),
        GRect(bounds.origin.x, n1y, W, 34), GTextOverflowModeTrailingEllipsis,
        GTextAlignmentCenter, NULL);
    graphics_draw_text(ctx, d->moon_name2, ui_font_header(),
        GRect(bounds.origin.x, n2y, W, 28), GTextOverflowModeTrailingEllipsis,
        GTextAlignmentCenter, NULL);
    ui_draw_auto_banner(ctx, bounds, d->rain_alert_min, d->last_updated,
                        anim_get_frame());
    return;
  }

  // Small classes (144x168 / 180x180): the 56/58px moon + title-font phase
  // name stacked the two name words and the illumination line under the bottom
  // banner. Shrink the moon and the name fonts and tighten the offsets so the
  // whole stack clears the banner (Phase 5.2). Large classes keep verbatim
  // values -> emery/gabbro byte-identical.
#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
  int moon_size = 40;
#else
  int moon_size = PBL_IF_ROUND_ELSE(58, 56);
#endif
  // Tightened top gap (was 20/16) to give the auto-banner room below
  // the illumination line without overlapping it.
#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
  int moon_y = UI_HEADER_Y + UI_HEADER_HEIGHT + 2 + moon_size/2;
#else
  int moon_y = UI_HEADER_Y + UI_HEADER_HEIGHT + PBL_IF_ROUND_ELSE(10, 12) + moon_size/2;
#endif
  GPoint moon_c = GPoint(bounds.origin.x + W/2, moon_y);

  // Sky disc: ~7px larger than the moon so a thin navy ring frames
  // the moon regardless of theme. This is also what the moon-phase
  // shadow color matches, so the lit fraction reads cleanly.
  int sky_r = moon_size/2 + 7;
  graphics_context_set_fill_color(ctx, moon_shadow);
  graphics_fill_circle(ctx, moon_c, sky_r);

  // Moon glyph with the sky color as the shadow color. The shadow is
  // computed scanline-by-scanline from `moon_illum` so it stays
  // clipped inside the moon disc — no flanking masks needed.
  icon_draw_moon_phase(ctx, moon_c, moon_size,
                       d->moon_phase, d->moon_illum,
                       moon_body, moon_shadow);

  // Phase name word 1 (large, fg). Small classes drop to the header font.
#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
  int name1_y = moon_y + moon_size/2 + 2;
#else
  int name1_y = moon_y + moon_size/2 + PBL_IF_ROUND_ELSE(10, 8);
#endif
  graphics_context_set_text_color(ctx, theme_fg());
  graphics_draw_text(ctx, d->moon_name1,
#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
      ui_font_header(),
#else
      ui_font_title(),
#endif
      GRect(bounds.origin.x, name1_y, W, 32),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // Phase name word 2 (smaller, secondary — readable on white).
#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
  int name2_y = name1_y + 18;
#else
  int name2_y = name1_y + 28;
#endif
  graphics_context_set_text_color(ctx, theme_secondary());
  graphics_draw_text(ctx, d->moon_name2,
#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
      ui_font_label(),
#else
      ui_font_header(),
#endif
      GRect(bounds.origin.x, name2_y, W, 22),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // Illumination percentage.
  char illum_buf[12];
  snprintf(illum_buf, sizeof(illum_buf), "%d%% LIT", (int)d->moon_illum);
#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
  int illum_y = name2_y + 16;
#else
  int illum_y = name2_y + 22;
#endif
  graphics_context_set_text_color(ctx, theme_secondary());
  graphics_draw_text(ctx, illum_buf,
      ui_font_label(),
      GRect(bounds.origin.x, illum_y, W, 18),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  ui_draw_auto_banner(ctx, bounds, d->rain_alert_min, d->last_updated,
                      anim_get_frame());
}
