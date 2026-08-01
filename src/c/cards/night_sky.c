#include "cards.h"
#include "../theme.h"
#include "../icons.h"
#include "../ui.h"
#include "../ui_layout.h"
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

#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
// Ink heights and top-side leading for the two font tiers this card stacks,
// same measurements main_card.c reserves (MC_FEELS_INK_H / MC_LABEL_INK_H).
#define NS_NAME1_INK   11   // GOTHIC_18_BOLD ink...
#define NS_NAME1_RISE   7   // ...and its dead top leading
#define NS_LABEL_INK    9   // GOTHIC_14_BOLD ink...
#define NS_LABEL_RISE   6   // ...and its dead top leading

// Moon size and the width of the "sky" ring drawn behind it. The colour classes
// thin the ring from 7 to 5 purely to buy the stack real gaps; 1-bit keeps 7
// because there the ring is also what separates a white lit limb from a white
// page. See NS_MOON_SHADOW below for the part of #52 that actually mattered.
#if defined(PBL_BW)
#define NS_MOON_SIZE   40
#define NS_SKY_RING     7
#else
#define NS_MOON_SIZE   40
#define NS_SKY_RING     5
#endif

// #52: the moon's SHADOW, not its sky, is what fails on 1-bit.
//
// GColorOxfordBlue quantizes to solid black and GColorIcterine to white, so the
// DARK theme reads correctly and only the LIGHT theme breaks: page white, sky
// black, lit limb white, shadow black. Two of those four are the page colour, so
// the eye assembles the BLACK shapes into the object and reads the ring plus the
// shadow as "a thin crescent" while the caption says 73% LIT.
//
// The first attempt here was Phase 5's suggestion — a thicker sky ring, on the
// theory that the moon needed a field to sit ON. Built and captured, it made the
// card *worse*: a fatter black annulus reads even more like a ring, because the
// problem was never the ring's width. **Three regions need to be distinct and a
// 1-bit panel has two colours.** No arrangement of black and white separates
// page / lit / shadow.
//
// A dithered fill is the third tone. This is the same property that made
// theme_fg() mandatory for the page dots in Phase 7 — a fill dithers rather than
// quantizing — used in the opposite direction: there dithering was the defect,
// here it is the fix. The shadow becomes a 50% checkerboard sitting between the
// solid sky and the white limb, and the moon reads as a disc with a shaded part
// in BOTH themes rather than as an outline with a bite taken out of it.
#if defined(PBL_BW)
#define NS_MOON_SHADOW  theme_muted()
#else
#define NS_MOON_SHADOW  moon_shadow
#endif

// Header glyph: an actual crescent (#53).
//
// icons.c's icon_draw_moon_small() fills a plain disc and then sets a stroke
// colour and width it never draws with — its own comment walks through three
// abandoned approaches to carving the crescent and settles for "an unfilled
// ring", which it also does not draw. The result is a featureless dot that
// reads as a bullet, not a moon.
//
// Carving is straightforward once the background is known, and here it is:
// the header is painted over the card's own background fill. A second disc of
// the same radius, offset left by 2r/3, leaves a ~6px crescent lit on the right
// at the header's 18px — waxing, which is the conventional way to draw a moon
// as an icon rather than as data (the card's real moon, below, carries the
// actual phase).
//
// This lives here rather than in icons.c deliberately: the helper has exactly
// ONE caller, the defect is scoped to the small classes, and icons.c is shared
// code that emery and gabbro compile. Fixing it locally keeps the locked pair
// entirely out of the blast radius.
static void prv_draw_moon_glyph(GContext *ctx, GPoint c, int size, GColor color) {
  int r = size / 2;
  graphics_context_set_fill_color(ctx, color);
  graphics_fill_circle(ctx, c, r);
  graphics_context_set_fill_color(ctx, theme_bg());
  graphics_fill_circle(ctx, GPoint(c.x - (r * 2) / 3, c.y), r);
}
#endif

void card_night_sky_draw(GContext *ctx, GRect bounds) {
  WeatherData *d = weather_data_get();
  int W = bounds.size.w;

  ui_draw_card_header_with_icon(ctx, bounds, "NIGHT SKY",
                                theme_fg(),
#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
                                UI_HEADER_Y, 18, prv_draw_moon_glyph);
#else
                                UI_HEADER_Y, 18, icon_draw_moon_small);
#endif

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

#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
  // The small classes cascade this stack downward from the header with fixed
  // offsets and never look at where it ends, which is why the sky disc landed
  // on the "WANING" cap-line (#51) and the "% LIT" row ran into the pill (#50).
  //
  // Honest note on #50: it does not currently clip. Phase 1 nudged the pill down
  // (rect -2, round -5) and that bought back just enough — measured, the illum
  // ink ends 2px above the pill on basalt and *1px* on chalk. It reads clean and
  // is one font metric away from not being. Solving the stack against the real
  // ui_content_bottom() converts that luck into structure: rows cannot overlap
  // each other or the pill, whatever the data or the tier does.
  char illum_buf[12];
  snprintf(illum_buf, sizeof(illum_buf), "%d%% LIT", (int)d->moon_illum);

  const int sky_r = NS_MOON_SIZE / 2 + NS_SKY_RING;
  // Below the header's INK, not below its 24px box — the box's bottom leading is
  // dead space and reserving it is what makes these stacks top-heavy.
  const int band_top = bounds.origin.y + UI_HEADER_Y + 18;

  enum { NS_ROW_MOON, NS_ROW_NAME1, NS_ROW_NAME2, NS_ROW_ILLUM, NS_ROW_COUNT };
  UILayoutRow rows[NS_ROW_COUNT] = {
    [NS_ROW_MOON]  = { .present = true, .h = 2 * sky_r },
    [NS_ROW_NAME1] = { .present = d->moon_name1[0] != '\0', .h = NS_NAME1_INK },
    [NS_ROW_NAME2] = { .present = d->moon_name2[0] != '\0', .h = NS_LABEL_INK },
    [NS_ROW_ILLUM] = { .present = true, .h = NS_LABEL_INK },
  };
  GRect avail = GRect(bounds.origin.x, band_top, W,
                      ui_content_bottom(bounds) - band_top);
  ui_layout_solve(rows, NS_ROW_COUNT, avail);

  GPoint moon_c = GPoint(bounds.origin.x + W / 2, rows[NS_ROW_MOON].cy);
  graphics_context_set_fill_color(ctx, moon_shadow);
  graphics_fill_circle(ctx, moon_c, sky_r);
  icon_draw_moon_phase(ctx, moon_c, NS_MOON_SIZE,
                       d->moon_phase, d->moon_illum,
                       moon_body, NS_MOON_SHADOW);

  // rows[].x/.w are chord-clamped, so a long phase name ellipsizes at the glass
  // edge on chalk instead of running under the bezel.
  if (rows[NS_ROW_NAME1].present) {
    graphics_context_set_text_color(ctx, theme_fg());
    graphics_draw_text(ctx, d->moon_name1, ui_font_header(),
        GRect(rows[NS_ROW_NAME1].x, rows[NS_ROW_NAME1].y - NS_NAME1_RISE,
              rows[NS_ROW_NAME1].w, NS_NAME1_RISE + rows[NS_ROW_NAME1].h + 4),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }
  if (rows[NS_ROW_NAME2].present) {
    graphics_context_set_text_color(ctx, theme_secondary());
    graphics_draw_text(ctx, d->moon_name2, ui_font_label(),
        GRect(rows[NS_ROW_NAME2].x, rows[NS_ROW_NAME2].y - NS_LABEL_RISE,
              rows[NS_ROW_NAME2].w, NS_LABEL_RISE + rows[NS_ROW_NAME2].h + 4),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }
  graphics_context_set_text_color(ctx, theme_secondary());
  graphics_draw_text(ctx, illum_buf, ui_font_label(),
      GRect(rows[NS_ROW_ILLUM].x, rows[NS_ROW_ILLUM].y - NS_LABEL_RISE,
            rows[NS_ROW_ILLUM].w, NS_LABEL_RISE + rows[NS_ROW_ILLUM].h + 4),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
#else
  int moon_size = PBL_IF_ROUND_ELSE(58, 56);
  // Tightened top gap (was 20/16) to give the auto-banner room below
  // the illumination line without overlapping it.
  int moon_y = UI_HEADER_Y + UI_HEADER_HEIGHT + PBL_IF_ROUND_ELSE(10, 12) + moon_size/2;
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

  // Phase name word 1 (large, fg).
  int name1_y = moon_y + moon_size/2 + PBL_IF_ROUND_ELSE(10, 8);
  graphics_context_set_text_color(ctx, theme_fg());
  graphics_draw_text(ctx, d->moon_name1,
      ui_font_title(),
      GRect(bounds.origin.x, name1_y, W, 32),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // Phase name word 2 (smaller, secondary — readable on white).
  int name2_y = name1_y + 28;
  graphics_context_set_text_color(ctx, theme_secondary());
  graphics_draw_text(ctx, d->moon_name2,
      ui_font_header(),
      GRect(bounds.origin.x, name2_y, W, 22),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // Illumination percentage.
  char illum_buf[12];
  snprintf(illum_buf, sizeof(illum_buf), "%d%% LIT", (int)d->moon_illum);
  int illum_y = name2_y + 22;
  graphics_context_set_text_color(ctx, theme_secondary());
  graphics_draw_text(ctx, illum_buf,
      ui_font_label(),
      GRect(bounds.origin.x, illum_y, W, 18),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
#endif

  ui_draw_auto_banner(ctx, bounds, d->rain_alert_min, d->last_updated,
                      anim_get_frame());
}
