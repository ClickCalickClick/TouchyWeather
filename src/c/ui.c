#include "ui.h"
#include "theme.h"
#include "icons.h"
#include <stdio.h>
#include <time.h>

// --- Font role accessors (Phase 3.1 Stage A) ---
// One accessor per distinct font in use. Each returns the exact system
// font the cards used before this refactor; Big Mode / Phase 5 will later
// branch inside these on scale + screen class, leaving call sites untouched.
GFont ui_font_header(void)  { return fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD); }
GFont ui_font_body(void)    { return fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD); }
GFont ui_font_title(void)   { return fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD); }
GFont ui_font_label(void)   { return fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD); }
GFont ui_font_caption(void) { return fonts_get_system_font(FONT_KEY_GOTHIC_14); }
GFont ui_font_number(void)  { return fonts_get_system_font(FONT_KEY_LECO_42_NUMBERS); }

// --- Layout metric accessors (Phase 3.1 Stage A + Phase 5 screen-class axis) ---
// Single scaling point for the shared layout constants. Each accessor is now
// a 4-way table over the compile-time screen class (see ui.h). The two LARGE
// branches return the verbatim pre-Phase-5 PBL_IF_ROUND_ELSE values, so emery
// (large-rect) and gabbro (large-round) are pixel-identical by construction.
// The two SMALL classes start equal to their large sibling and are tuned per
// platform with screenshot evidence (Phase 5.2) — never up front (prove first).
int ui_margin_x(void) {
#if defined(UI_SCREEN_SMALL_ROUND)
  return 20;   // chalk 180  — TODO(5.2): tune; currently == large-round
#elif defined(UI_SCREEN_LARGE_ROUND)
  return 20;   // gabbro 260 — verbatim pre-Phase-5 round value
#elif defined(UI_SCREEN_SMALL_RECT)
  return 12;   // 144x168    — TODO(5.2): tune; currently == large-rect
#else
  return 12;   // emery 200  — verbatim pre-Phase-5 rect value
#endif
}
int ui_header_y(void) {
#if defined(UI_SCREEN_SMALL_ROUND)
  return 24;   // chalk 180  — TODO(5.2): tune; currently == large-round
#elif defined(UI_SCREEN_LARGE_ROUND)
  return 24;   // gabbro 260 — verbatim pre-Phase-5 round value
#elif defined(UI_SCREEN_SMALL_RECT)
  return 8;    // 144x168    — TODO(5.2): tune; currently == large-rect
#else
  return 8;    // emery 200  — verbatim pre-Phase-5 rect value
#endif
}
int ui_header_height(void) { return 24; }  // uniform across all screen classes

static void prv_format_ago(uint32_t when, char *out, size_t n) {
  if (!when) { snprintf(out, n, "UPDATED --"); return; }
  uint32_t now = (uint32_t)time(NULL);
  if (now < when) { snprintf(out, n, "UPDATED NOW"); return; }
  uint32_t delta = now - when;
  if (delta < 60)               snprintf(out, n, "UPDATED NOW");
  else if (delta < 60 * 60)     snprintf(out, n, "UPDATED %luM AGO", (unsigned long)(delta / 60));
  else if (delta < 24 * 60 * 60) snprintf(out, n, "UPDATED %luH AGO", (unsigned long)(delta / 3600));
  else                          snprintf(out, n, "UPDATED %luD AGO", (unsigned long)(delta / 86400));
}

bool ui_draw_status_banner(GContext *ctx, GRect bounds,
                           StatusBannerMode mode,
                           int minutes_to_rain,
                           uint32_t last_updated_secs) {
  if (mode == STATUS_BANNER_RAIN && minutes_to_rain < 0) return false;

  // Sit above page indicator. On round we keep clear of the bottom arc.
  // Lowered (smaller pad) so the pill sits where the main card's down-nudged
  // layout placed it — round 40→35 (-5), rect 22→20 (-2) — keeping the pill
  // position uniform across every card.
  int pad_bottom = PBL_IF_ROUND_ELSE(35, 20);
  int banner_h = 22;
  int banner_w = PBL_IF_ROUND_ELSE(140, 130);
  GRect r = GRect(bounds.origin.x + (bounds.size.w - banner_w) / 2,
                  bounds.origin.y + bounds.size.h - pad_bottom - banner_h,
                  banner_w, banner_h);

  GColor pill_bg = (mode == STATUS_BANNER_RAIN)
                   ? theme_accent_orange()
                   : theme_muted();
  GColor txt_color = (mode == STATUS_BANNER_RAIN)
                     ? GColorBlack
                     : theme_fg();

  graphics_context_set_fill_color(ctx, pill_bg);
  graphics_fill_rect(ctx, r, banner_h / 2, GCornersAll);

  char buf[32];
  if (mode == STATUS_BANNER_RAIN) {
    // Lead times of an hour or more read as hours ("RAIN IN 4H") so the pill
    // doesn't look like an imminent minute-countdown; sub-hour stays minutes.
    if (minutes_to_rain >= 60) {
      snprintf(buf, sizeof(buf), "RAIN IN %dH", minutes_to_rain / 60);
    } else {
      snprintf(buf, sizeof(buf), "RAIN IN %dM", minutes_to_rain);
    }
  } else {
    prv_format_ago(last_updated_secs, buf, sizeof(buf));
  }

  graphics_context_set_text_color(ctx, txt_color);
  GRect tr = GRect(r.origin.x + 6, r.origin.y + 2, r.size.w - 12, banner_h - 2);
  graphics_draw_text(ctx, buf, ui_font_label(),
                     tr, GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);
  return true;
}

bool ui_draw_auto_banner(GContext *ctx, GRect bounds,
                         int minutes_to_rain,
                         uint32_t last_updated_secs,
                         uint32_t frame) {
  bool has_rain = minutes_to_rain >= 0;
  StatusBannerMode mode;
  if (has_rain) {
    // 100ms frames; 40 frames = 4s. Toggle every 4s.
    mode = ((frame / 40) & 1) ? STATUS_BANNER_UPDATED : STATUS_BANNER_RAIN;
  } else {
    mode = STATUS_BANNER_UPDATED;
  }
  return ui_draw_status_banner(ctx, bounds, mode,
                               minutes_to_rain, last_updated_secs);
}

void ui_draw_header(GContext *ctx, GRect bounds, const char *text,
                    GColor color, int y) {
  graphics_context_set_text_color(ctx, color);
  GRect tr = GRect(bounds.origin.x, bounds.origin.y + y,
                   bounds.size.w, UI_HEADER_HEIGHT);
  graphics_draw_text(ctx, text, ui_font_header(),
                     tr, GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);
}

void ui_draw_card_header_with_icon(GContext *ctx, GRect bounds,
                                   const char *label, GColor color,
                                   int y, int icon_size,
                                   UIIconDrawFn draw_icon) {
  GFont hf = ui_font_header();
  GSize tsize = graphics_text_layout_get_content_size(label, hf,
      GRect(0,0,bounds.size.w, UI_HEADER_HEIGHT),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
  int gap = 5;
  int total_w = icon_size + gap + tsize.w;
  int start_x = bounds.origin.x + (bounds.size.w - total_w) / 2;
  int icon_cy = bounds.origin.y + y + UI_HEADER_HEIGHT/2 - 2;
  if (draw_icon) {
    draw_icon(ctx, GPoint(start_x + icon_size/2, icon_cy), icon_size, color);
  }
  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, label, hf,
      GRect(start_x + icon_size + gap, bounds.origin.y + y,
            tsize.w + 4, UI_HEADER_HEIGHT),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

void ui_draw_dotted_hline(GContext *ctx, int x1, int x2, int y, GColor color) {
  graphics_context_set_stroke_color(ctx, color);
  graphics_context_set_stroke_width(ctx, 2);
  for (int x = x1; x <= x2; x += 5) {
    graphics_draw_pixel(ctx, GPoint(x, y));
    graphics_draw_pixel(ctx, GPoint(x, y + 1));
    graphics_draw_pixel(ctx, GPoint(x + 1, y));
    graphics_draw_pixel(ctx, GPoint(x + 1, y + 1));
  }
}

