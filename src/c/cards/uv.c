#include "cards.h"
#include "../theme.h"
#include "../icons.h"
#include "../ui.h"
#include "../weather_data.h"
#include "../anim.h"
#include <stdio.h>

void card_uv_draw(GContext *ctx, GRect bounds) {
  WeatherData *d = weather_data_get();
  int W = bounds.size.w;
  int H = bounds.size.h;
  // Slide-transition origin offset (Phase 10F): the card may be drawn
  // into a horizontally shifted bounds during the 200ms push, so every
  // X coordinate must be translated by ox to move with the card.
  int ox = bounds.origin.x;

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
  int num_top = c.y - 38, num_h = 44, label_dy = 6, peak_dy = 24;
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
        GRect(ox, c.y + peak_dy, W, 18),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }

  (void)H;
  ui_draw_auto_banner(ctx, bounds, d->rain_alert_min, d->last_updated,
                      anim_get_frame());
}

