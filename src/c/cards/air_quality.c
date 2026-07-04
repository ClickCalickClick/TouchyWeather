#include "cards.h"
#include "../theme.h"
#include "../icons.h"
#include "../ui.h"
#include "../settings.h"
#include "../weather_data.h"
#include "../anim.h"
#include <stdio.h>

// Phase 10E: AQI category color helper.
//
// Maps the EPA AQI bands to readable Pebble palette colors. Used both
// for the gauge sweep and the hero number so the value's severity is
// communicated by color even at a glance.
static GColor aqi_category_color(int aqi) {
  if (aqi <= 50)   return GColorIslamicGreen;       // GOOD
  if (aqi <= 100)  return theme_accent_orange();    // MODERATE (yellow)
  if (aqi <= 150)  return GColorOrange;             // UNHEALTHY FOR SENSITIVE
  if (aqi <= 200)  return GColorRed;                // UNHEALTHY
  if (aqi <= 300)  return GColorPurple;             // VERY UNHEALTHY
  return GColorDarkCandyAppleRed;                   // HAZARDOUS
}

// Phase 10E.x: AQI → arc sweep mapping.
//
// "Healthy/unhealthy halves" expression:
//   AQI 0–100   → 0–90°   (left half of the arc)
//   AQI 100–500 → 90–180° (right half, piecewise by EPA band)
//
// Anchoring AQI 100 (the Good/Moderate → USG inflection) at the top of
// the arc makes the gauge legible at a glance: left half = everyday-
// acceptable, right half = take action. Also extends honest support to
// AQI 301–500 instead of clipping at 300 like the previous linear
// formula did.
static int aqi_to_sweep_deg(int aqi) {
  if (aqi <= 0) return 0;
  if (aqi <= 100) return (aqi * 90) / 100;
  // Upper-half breakpoints: AQI 100→90°, 150→113°, 200→135°, 300→158°, 500→180°
  static const int bp[]  = {100, 150, 200, 300, 500};
  static const int deg[] = { 90, 113, 135, 158, 180};
  for (int i = 1; i < 5; i++) {
    if (aqi <= bp[i]) {
      return deg[i-1] + ((aqi - bp[i-1]) * (deg[i] - deg[i-1])) / (bp[i] - bp[i-1]);
    }
  }
  return 180;
}

void card_air_quality_draw(GContext *ctx, GRect bounds) {
  WeatherData *d = weather_data_get();
  int W = bounds.size.w;
  int H = bounds.size.h;
  // Slide-transition origin offset (Phase 10F): translate every X coord
  // so the gauge moves with the card during the 200ms push.
  int ox = bounds.origin.x;

  // --- Big Mode (Stage B): drop the arc gauge; giant number + big label. ---
  // Twin of the Big UV layout. AQI keeps its EPA category COLOR on the number
  // and label (the one hue that IS the data) — it opts back out of the Big-Mode
  // contrast collapse, since severity-by-color is the point. Pollen shows on the
  // large classes only. Mandatory banner kept. Returns early -> Normal gauge
  // layout below untouched, Big-OFF pixel-identical.
  if (settings_get_big_mode()) {
    ui_draw_card_header_with_icon(ctx, bounds, "AIR QUALITY", theme_fg(),
                                  UI_HEADER_Y, 18, icon_draw_pulse);
    GColor cat = aqi_category_color(d->aqi);
    int num_y, label_y;
#if defined(UI_SCREEN_SMALL_RECT)
    num_y = 42; label_y = 92;
#elif defined(UI_SCREEN_SMALL_ROUND)
    num_y = 56; label_y = 106;
#elif defined(UI_SCREEN_LARGE_RECT)
    num_y = 54; label_y = 112;
#else  // UI_SCREEN_LARGE_ROUND
    num_y = 74; label_y = 132;
#endif
    char nb[8]; snprintf(nb, sizeof(nb), "%d", d->aqi);
    graphics_context_set_text_color(ctx, cat);
    graphics_draw_text(ctx, nb, ui_font_number(),
        GRect(ox, num_y, W, 50), GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    graphics_context_set_text_color(ctx, cat);
    graphics_draw_text(ctx, aqi_label(d->aqi), ui_font_body(),
        GRect(ox, label_y, W, 32), GTextOverflowModeTrailingEllipsis,
        GTextAlignmentCenter, NULL);
#if !defined(UI_SCREEN_SMALL_RECT) && !defined(UI_SCREEN_SMALL_ROUND)
    if (d->pollen_level >= 0) {
      const char *plabel;
      int lvl = d->pollen_level;
      if (lvl == 0)      plabel = "POLLEN: NONE";
      else if (lvl == 1) plabel = "POLLEN: VERY LOW";
      else if (lvl == 2) plabel = "POLLEN: LOW";
      else if (lvl == 3) plabel = "POLLEN: MODERATE";
      else if (lvl == 4) plabel = "POLLEN: HIGH";
      else               plabel = "POLLEN: VERY HIGH";
      graphics_context_set_text_color(ctx, theme_fg());
      graphics_draw_text(ctx, plabel, ui_font_header(),
          GRect(ox, label_y + 34, W, 24), GTextOverflowModeTrailingEllipsis,
          GTextAlignmentCenter, NULL);
    }
#endif
    ui_draw_auto_banner(ctx, bounds, d->rain_alert_min, d->last_updated,
                        anim_get_frame());
    return;
  }

  // Header in fg for legibility; the category color carries the signal.
  int header_y = UI_HEADER_Y;
  ui_draw_card_header_with_icon(ctx, bounds, "AIR QUALITY",
                                theme_fg(),
                                header_y, 18, icon_draw_pulse);

  // Half-arc gauge — same style as UV (consistent visual language).
  // AQI 0..300 maps to 0..180°. Values >300 max out the arc but the
  // numeric label still reads truthfully.
  // Small classes shrink the radius so the value/label/pollen badge clear the
  // bottom banner on the short screens (Phase 5.2, same as UV). Large classes
  // keep verbatim values -> emery/gabbro byte-identical.
#if defined(UI_SCREEN_SMALL_ROUND)
  int radius = 42;
#elif defined(UI_SCREEN_SMALL_RECT)
  int radius = 44;
#else
  int radius = PBL_IF_ROUND_ELSE(72, 64);
#endif
  int thickness = 10;
  GPoint c = { ox + W/2, header_y + UI_HEADER_HEIGHT + 8 + radius };
  GRect arc_box = GRect(c.x - radius, c.y - radius, radius*2, radius*2);
  graphics_context_set_fill_color(ctx, theme_muted());
  graphics_fill_radial(ctx, arc_box, GOvalScaleModeFitCircle, thickness,
                       DEG_TO_TRIGANGLE(-90), DEG_TO_TRIGANGLE(90));
  int aqi_capped = d->aqi;
  if (aqi_capped < 0) aqi_capped = 0;
  if (aqi_capped > 500) aqi_capped = 500;
  int sweep_deg = aqi_to_sweep_deg(aqi_capped);
  GColor cat = aqi_category_color(d->aqi);
  graphics_context_set_fill_color(ctx, cat);
  graphics_fill_radial(ctx, arc_box, GOvalScaleModeFitCircle, thickness,
                       DEG_TO_TRIGANGLE(-90),
                       DEG_TO_TRIGANGLE(-90 + sweep_deg));

  // Small classes lift the value box and tighten the label/pollen offsets (and
  // drop the pollen badge to the caption font) so all elements clear the
  // shrunken gauge and the banner.
#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
  int num_top = c.y - 38, num_h = 44, label_dy = 4, pollen_dy = 22;
#else
  int num_top = c.y - 32, num_h = 50, label_dy = 18, pollen_dy = 40;
#endif

  // Big AQI number inside, colored by category.
  char buf[8]; snprintf(buf, sizeof(buf), "%d", d->aqi);
  graphics_context_set_text_color(ctx, cat);
  graphics_draw_text(ctx, buf,
      ui_font_number(),
      GRect(c.x - radius, num_top, radius*2, num_h),
      GTextOverflowModeFill, GTextAlignmentCenter, NULL);

  // Label below — also colored by category.
  graphics_context_set_text_color(ctx, cat);
  graphics_draw_text(ctx, aqi_label(d->aqi),
      ui_font_header(),
      GRect(ox, c.y + label_dy, W, 24),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // Pollen badge (Google UPI 0..5). Skipped when uncovered (-1) so the
  // card looks unchanged for users outside Google's coverage region.
  if (d->pollen_level >= 0) {
    const char *plabel;
    GColor pcolor;
    int lvl = d->pollen_level;
    if (lvl == 0)      { plabel = "POLLEN: NONE";      pcolor = theme_secondary(); }
    else if (lvl == 1) { plabel = "POLLEN: VERY LOW";     pcolor = GColorIslamicGreen; }
    else if (lvl == 2) { plabel = "POLLEN: LOW";       pcolor = GColorIslamicGreen; }
    else if (lvl == 3) { plabel = "POLLEN: MODERATE";  pcolor = theme_accent_orange(); }
    else if (lvl == 4) { plabel = "POLLEN: HIGH";      pcolor = GColorOrange; }
    else               { plabel = "POLLEN: VERY HIGH";    pcolor = GColorRed; }
    graphics_context_set_text_color(ctx, pcolor);
    graphics_draw_text(ctx, plabel,
        // Small classes drop the badge to the caption font to fit.
#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
        ui_font_caption(),
#else
        ui_font_header(),
#endif
        GRect(ox, c.y + pollen_dy, W, 24),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }

  (void)H;
  ui_draw_auto_banner(ctx, bounds, d->rain_alert_min, d->last_updated,
                      anim_get_frame());
}

