#include "cards.h"
#include "../theme.h"
#include "../icons.h"
#include "../ui.h"
#include "../settings.h"
#include "../weather_data.h"
#include "../anim.h"
#include <stdio.h>

#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
static int prv_clamp_peak_y(GRect bounds, int y) {
  // Reserve the whole 18px box, not just the ink: at the shipped geometry the
  // box bottom lands exactly on the pill (c.y + 24 + 18 == 126), which leaves
  // the ink 4px of air and the layout none at all. Clamping to the ink height
  // computes to a no-op here — it would only catch a collision after one had
  // already happened — so reserve the box and PEAK gains real clearance.
  // -16 balances the row between its two neighbours: reserving the full 18px box
  // bought 8px above the pill but squeezed the gap to "LOW" from 6px to 2, which
  // just moves the crowding. At 16 the row sits ~6px off the pill and ~3px off
  // the label above it.
  int max_y = ui_content_bottom(bounds) - 16;
  return (y > max_y) ? max_y : y;
}
#endif

void card_uv_draw(GContext *ctx, GRect bounds) {
  WeatherData *d = weather_data_get();
  int W = bounds.size.w;
  int H = bounds.size.h;
  // Slide-transition origin offset (Phase 10F): the card may be drawn
  // into a horizontally shifted bounds during the 200ms push, so every
  // X coordinate must be translated by ox to move with the card.
  int ox = bounds.origin.x;

  // --- Big Mode (Stage B): drop the arc gauge; giant number + big label. ---
  // The 49-ish px number is the readable focus once the decorative gauge box is
  // freed. UV is clamped >=0 (BITHAM has a minus glyph, but UV is physically 0+
  // and the empty state should read "0 / LOW"). PEAK shows on the large classes
  // only (room). Mandatory banner kept. Returns early -> Normal gauge layout
  // below untouched, Big-OFF pixel-identical.
  if (settings_get_big_mode()) {
    ui_draw_card_header_with_icon(ctx, bounds, "UV INDEX", theme_fg(),
                                  UI_HEADER_Y, 18, icon_draw_sun);
    int uv = d->uv < 0 ? 0 : d->uv;
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
    char nb[8]; snprintf(nb, sizeof(nb), "%d", uv);
    graphics_context_set_text_color(ctx, theme_fg());
    graphics_draw_text(ctx, nb, ui_font_number(),
        GRect(ox, num_y, W, 50), GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    graphics_draw_text(ctx, uv_label(uv), ui_font_body(),
        GRect(ox, label_y, W, 32), GTextOverflowModeTrailingEllipsis,
        GTextAlignmentCenter, NULL);
#if !defined(UI_SCREEN_SMALL_RECT) && !defined(UI_SCREEN_SMALL_ROUND)
    if (d->uv_max > 0) {
      char pb[16]; snprintf(pb, sizeof(pb), "PEAK %d", d->uv_max);
      graphics_context_set_text_color(ctx, theme_fg());
      graphics_draw_text(ctx, pb, ui_font_header(),
          GRect(ox, label_y + 34, W, 24), GTextOverflowModeTrailingEllipsis,
          GTextAlignmentCenter, NULL);
    }
#endif
    ui_draw_auto_banner(ctx, bounds, d->rain_alert_min, d->last_updated,
                        anim_get_frame());
    return;
  }

  // Header: small sun + "UV INDEX".
  int header_y = UI_HEADER_Y;
  // Header in fg for legibility on either theme. The orange accent
  // lives on the gauge body where it carries the actual UV signal.
  ui_draw_card_header_with_icon(ctx, bounds, "UV INDEX",
                                theme_fg(),
                                header_y, 18, icon_draw_sun);

  // Half-arc gauge (180° from -90 to +90). Small classes (144x168 / 180x180)
  // shrink the radius: the gabbro-260 radius (72) overflows the short screens
  // and drove the value/label/PEAK under the bottom banner (Phase 5.2). Large
  // classes keep verbatim values -> emery/gabbro byte-identical.
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
  // Background track.
  graphics_context_set_fill_color(ctx, theme_muted());
  graphics_fill_radial(ctx, arc_box, GOvalScaleModeFitCircle, thickness,
                       DEG_TO_TRIGANGLE(-90), DEG_TO_TRIGANGLE(90));
  // UV index is physically 0+; guard a negative/unknown live value. The LECO
  // hero font has no minus glyph (a negative UV drew a tofu box), and a
  // negative value would also sweep the gauge backwards. Clamp to >=0 so the
  // hero, gauge, and label all read "0 / LOW" on bad data instead of breaking.
  int uv = d->uv < 0 ? 0 : d->uv;

  // Foreground proportion: uv 0..11 over 180°.
  int uv_capped = uv > 11 ? 11 : uv;
  int sweep_deg = (uv_capped * 180) / 11;
  graphics_context_set_fill_color(ctx, theme_accent_orange());
  graphics_fill_radial(ctx, arc_box, GOvalScaleModeFitCircle, thickness,
                       DEG_TO_TRIGANGLE(-90),
                       DEG_TO_TRIGANGLE(-90 + sweep_deg));

  // Big number inside. Small classes lift the number box and tighten the
  // label/PEAK offsets so all three clear the shrunken gauge and the banner.
#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
  // #47: the numeral box starts inside the 10px arc band. Measured, the ink
  // clears the arc's inner edge by 1px rather than overlapping it as the
  // register states — but 1px is not clearance, it is a coincidence. 2px down
  // balances it against the label below (which had 5px), giving ~3px each way.
  int num_top = c.y - 36, num_h = 44, label_dy = 6, peak_dy = 24;
#else
  int num_top = c.y - 32, num_h = 50, label_dy = 18, peak_dy = 38;
#endif
  char buf[8]; snprintf(buf, sizeof(buf), "%d", uv);
  graphics_context_set_text_color(ctx, theme_fg());
  graphics_draw_text(ctx, buf,
      ui_font_number(),
      GRect(c.x - radius, num_top, radius*2, num_h),
      GTextOverflowModeFill, GTextAlignmentCenter, NULL);

  // Label below.
  graphics_draw_text(ctx, uv_label(uv),
      ui_font_header(),
      GRect(ox, c.y + label_dy, W, 24),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // "PEAK n" subtitle in the secondary color, just below the qualitative
  // label. Suppressed when uv_max is 0/unknown so we don't show "PEAK 0"
  // before any daytime refresh has populated the daily peak.
  if (d->uv_max > 0) {
    char peak_buf[16];
    snprintf(peak_buf, sizeof(peak_buf), "PEAK %d", d->uv_max);
    graphics_context_set_text_color(ctx, theme_secondary());
    graphics_draw_text(ctx, peak_buf,
        ui_font_caption(),
#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
        // #44 — PEAK is the card's last line and its box bottom landed exactly
        // on the pill's top edge (c.y + 24 + 18 == 126 on SMALL_RECT). It reads
        // as touching, and any font or pill change makes it a collision. Clamp
        // it to the reserved content area rather than trusting the arithmetic.
        GRect(ox, prv_clamp_peak_y(bounds, c.y + peak_dy), W, 18),
#else
        GRect(ox, c.y + peak_dy, W, 18),
#endif
        GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }

  (void)H;
  ui_draw_auto_banner(ctx, bounds, d->rain_alert_min, d->last_updated,
                      anim_get_frame());
}

