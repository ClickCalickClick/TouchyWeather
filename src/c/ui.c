#include "ui.h"
#include "theme.h"
#include "icons.h"
#include "settings.h"
// ui_band_w(), for the status pill's chord clamp on chalk. Every declaration in
// this header sits inside the same small-class guard the call sites do, so on
// emery and gabbro it contributes no code and the translation unit is unmoved.
#include "ui_layout.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

// --- Font role accessors (Phase 3.1 Stage A + Phase 5 screen class + Stage B scale) ---
// One accessor per distinct font role. Each carries three axes, resolved here
// so call sites never change:
//   1. Stage A  — the base font role (verbatim current look, the "Normal" path).
//   2. Phase 5  — compile-time screen class (#if UI_SCREEN_*), for platform fit.
//   3. Stage B  — the RUNTIME Big Mode scale axis (this file's new branch).
// Big Mode is a runtime toggle (settings_get_big_mode()), NOT compile-time like
// the screen class, so this is a real runtime branch. The invariant: when Big
// Mode is OFF the else-path returns the EXACT font it did before, so every
// platform renders pixel-identical to pre-Stage-B (verified by screenshot, since
// the added branch means the binary is no longer byte-identical).
//
// Big-Mode font map (per the Fable UI plan): bump each role a tier for
// readability, holding the small screen classes back where a bigger font would
// not fit the shorter 144/180px height.
GFont ui_font_header(void) {
  if (settings_get_big_mode()) {
    // Card titles are chrome, not data. Small classes keep 18B to save the
    // vertical room the enlarged body/number need; large classes go to 24B.
#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
    return fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
#else
    return fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
#endif
  }
  return fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
}
GFont ui_font_body(void) {
  if (settings_get_big_mode()) {
#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
    return fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
#else
    return fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
#endif
  }
  return fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
}
GFont ui_font_title(void) {
  // GOTHIC_28_BOLD -> BITHAM_30_BLACK (heavier + slightly larger) in Big Mode.
  if (settings_get_big_mode())
    return fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK);
  return fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
}
GFont ui_font_label(void) {
  // GOTHIC_14_BOLD -> GOTHIC_18_BOLD in Big Mode (also enlarges the status pill).
  if (settings_get_big_mode())
    return fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  return fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
}
GFont ui_font_caption(void) {
  // GOTHIC_14 -> GOTHIC_14_BOLD in Big Mode (same size, more weight for legibility).
  if (settings_get_big_mode())
    return fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
  return fonts_get_system_font(FONT_KEY_GOTHIC_14);
}
GFont ui_font_number(void) {
  // Hero numerals (temp, UV, AQI).
  if (settings_get_big_mode()) {
    // Big Mode hero numeral. BITHAM_42_BOLD is a FULL font — unlike the LECO /
    // ROBOTO subset numeral fonts it carries the degree and minus glyphs, so a
    // temperature ("72°") or a sub-zero value ("-5°") renders with no tofu and
    // needs no per-sign fallback. Heavier than LECO_42 at the same nominal size;
    // the readability gain in Big Mode also comes from the simplified layouts.
    return fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD);
  }
  // Normal path (verbatim). LECO_42 is tuned for the large screens; on the 144px
  // small-rect class it overflows its box (main-card temp clips to "6..."), so
  // drop to a smaller LECO there. Large classes keep the verbatim LECO_42.
#if defined(UI_SCREEN_SMALL_RECT)
  return fonts_get_system_font(FONT_KEY_LECO_36_BOLD_NUMBERS);
#else
  return fonts_get_system_font(FONT_KEY_LECO_42_NUMBERS);
#endif
}

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

// --- Status-pill geometry (small classes only; see ui.h) ---
//
// Kept in lockstep with ui_draw_status_banner() below by construction: both
// read the same pad_bottom table and the same Big-Mode banner height. If that
// function's geometry changes, change it here too — a screenshot will show the
// drift immediately as a gap or an overlap at the bottom of every card.
// Pill top for EVERY screen class. ui_status_pill_top() below is the small-class
// public face of this; ui_draw_awaiting_data() needs the same number on the
// large classes too, and a second copy of the pad table is exactly the kind of
// duplicated helper that has already produced three separate defects here.
static int prv_pill_top(GRect bounds) {
#if defined(UI_SCREEN_SMALL_ROUND)
  const int pad_bottom = 18;
#else
  const int pad_bottom = PBL_IF_ROUND_ELSE(35, 20);
#endif
  const int banner_h = settings_get_big_mode() ? 28 : 22;
  return bounds.origin.y + bounds.size.h - pad_bottom - banner_h;
}

#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
int ui_status_pill_top(GRect bounds) {
  return prv_pill_top(bounds);
}

int ui_content_bottom(GRect bounds) {
  // 4 px of air so descenders and degree rings don't kiss the pill's edge —
  // "61°" merging into the pill outline was a real Big-Mode defect.
  return ui_status_pill_top(bounds) - 4;
}
#endif

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

#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
// Shrink the pill's label until it fits the pill.
//
// Clamping banner_w (below) also shrinks its inner text box — to ~108px on
// SMALL_RECT and ~104px on chalk, where the strings were written against
// 118/128 — and Big Mode independently promotes ui_font_label() to
// GOTHIC_18_BOLD, at which "UPDATED 59M AGO" measures far past either. The draw
// call's GTextOverflowModeTrailingEllipsis would resolve that by eating the
// tail, i.e. by deleting the number: "UPDATED 5…" is a pill that has lost the
// one thing it exists to say.
//
// So drop words instead of characters, least informative first: " AGO" (the
// tense is already carried by "UPDATED"), then the "UPDATED " prefix itself.
// Measured at each step rather than switched on Big Mode, because the same
// cascade then also covers the 14px path's marginal cases, three-digit deltas
// and the RAIN wording — one policy instead of a second set of constants to
// keep in sync with the first. This is the Phase-4 Hours cascade in miniature.
static bool prv_pill_text_fits(const char *s, int inner_w) {
  GSize sz = graphics_text_layout_get_content_size(
      s, ui_font_label(), GRect(0, 0, 200, 40),
      GTextOverflowModeFill, GTextAlignmentLeft);
  return sz.w <= inner_w;
}

static void prv_fit_pill_text(char *buf, int inner_w) {
  if (inner_w <= 0 || prv_pill_text_fits(buf, inner_w)) return;
  char *ago = strstr(buf, " AGO");
  if (ago) {
    *ago = '\0';
    if (prv_pill_text_fits(buf, inner_w)) return;
  }
  const char *prefix = "UPDATED ";
  size_t plen = strlen(prefix);
  if (strncmp(buf, prefix, plen) == 0) {
    memmove(buf, buf + plen, strlen(buf + plen) + 1);
  }
}
#endif

bool ui_draw_status_banner(GContext *ctx, GRect bounds,
                           StatusBannerMode mode,
                           int minutes_to_rain,
                           uint32_t last_updated_secs) {
  if (mode == STATUS_BANNER_RAIN && minutes_to_rain < 0) return false;

  // Sit above page indicator. On round we keep clear of the bottom arc.
  // Lowered (smaller pad) so the pill sits where the main card's down-nudged
  // layout placed it — round 40→35 (-5), rect 22→20 (-2) — keeping the pill
  // position uniform across every card. On the small-round (chalk 180) class
  // the gabbro-tuned 35px pad pushes the pill into the middle of the content
  // (it overlapped the 17:00 row on 6 Hours), so hug the bottom arc tighter
  // there to reclaim a row of vertical space. Large classes unchanged.
#if defined(UI_SCREEN_SMALL_ROUND)
  int pad_bottom = 18;
#else
  int pad_bottom = PBL_IF_ROUND_ELSE(35, 20);
#endif
  // Big Mode: a taller, wider pill so the 18px label (ui_font_label bumps to
  // GOTHIC_18_BOLD in Big Mode) has room. The text rect derives from banner_h,
  // so it follows automatically. Normal path unchanged -> pixel-identical.
  bool big = settings_get_big_mode();
  int banner_h = big ? 28 : 22;
  int banner_w = PBL_IF_ROUND_ELSE(140, 130) + (big ? 20 : 0);
#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
  // The widths above are the large classes'. On the small ones they are wider
  // than the screen can carry, in two different ways:
  //
  //   * SMALL_RECT: 130 on a 144px screen leaves 7px gutters against every
  //     card's own 12px margin, so the pill reads as wider than the content it
  //     belongs to (#70) — and in Big Mode 130+20 = 150 on 144 puts the origin
  //     at x=-3 and cuts BOTH rounded ends flat against the screen edge (#71).
  //   * SMALL_ROUND: 140 is a straight rect on a 180px circle. At the pill's
  //     lower edge the chord is only ~108px, so ~16px spilled past the glass on
  //     each side and the ends rendered flat (#18/#72).
  //
  // ui_band_w() answers both: the class inset on rect (144-24 = 120, which is
  // exactly the card margin the pill was violating) and the inscribed chord on
  // round. It is measured at the band's vertical CENTER, which is right here
  // rather than merely convenient — the pill is a stadium, so its corners are
  // pulled in by banner_h/2 and its widest ink is the middle row this measures.
  {
    int pill_y = bounds.origin.y + bounds.size.h - pad_bottom - banner_h;
    int band = ui_band_w(bounds, pill_y, banner_h);
    if (banner_w > band) banner_w = band;
  }
#endif
  GRect r = GRect(bounds.origin.x + (bounds.size.w - banner_w) / 2,
                  bounds.origin.y + bounds.size.h - pad_bottom - banner_h,
                  banner_w, banner_h);

#if defined(PBL_BW)
  // 1-bit: an accent pill collapses to fg (black text on it would vanish)
  // and a muted pill dithers illegibly. Use a solid inverted pill — fg
  // background with bg text — so both banner modes stay crisp and readable.
  GColor pill_bg = theme_fg();
  GColor txt_color = theme_bg();
#else
  GColor pill_bg = (mode == STATUS_BANNER_RAIN)
                   ? theme_accent_orange()
                   : theme_muted();
  // The rain pill's text is black because the accent under it is normally
  // chrome yellow. Big Mode breaks that assumption: its high-contrast policy
  // collapses theme_accent_orange() to theme_fg(), so the pill becomes a solid
  // fg block and black text disappears into it on the light theme (the whole
  // "RAIN IN 30M" alert rendered as an empty slab for half of every toggle
  // cycle). Invert with the pill there, exactly as the PBL_BW branch above.
  GColor txt_color = (mode == STATUS_BANNER_RAIN)
                     ? (big ? theme_bg() : GColorBlack)
                     : theme_fg();
#endif

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

#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
  prv_fit_pill_text(buf, r.size.w - 12);
#endif

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

void ui_draw_awaiting_data(GContext *ctx, GRect bounds) {
  GFont tf = ui_font_header();
  GFont sf = ui_font_caption();
  const char *title = "NO DATA YET";
  // Word-wrapped rather than sized per screen class: at GOTHIC_14 this measures
  // ~119px against the 120px the 144 class has usable, so a single line is a
  // coin flip on the tightest platform. Wrapping lets the small classes break it
  // and the large ones keep it on one line, with no per-class table to drift.
  const char *sub = "WAITING FOR PHONE";

  const int usable = bounds.size.w - 2 * UI_MARGIN_X;
  const int x = bounds.origin.x + UI_MARGIN_X;
  GSize ts = graphics_text_layout_get_content_size(title, tf,
      GRect(0, 0, usable, 40), GTextOverflowModeWordWrap, GTextAlignmentCenter);
  GSize ss = graphics_text_layout_get_content_size(sub, sf,
      GRect(0, 0, usable, 60), GTextOverflowModeWordWrap, GTextAlignmentCenter);

  // Center the pair in the band the card actually owns: below its header, above
  // the status pill. Both ends are real geometry, so this lands correctly on
  // every class and in Big Mode without a per-card offset.
  const int band_top = bounds.origin.y + UI_HEADER_Y + UI_HEADER_HEIGHT;
  const int band_bottom = prv_pill_top(bounds) - 4;
  const int gap = 4;
  int y = band_top + (band_bottom - band_top - (ts.h + gap + ss.h)) / 2;
  if (y < band_top) y = band_top;

  graphics_context_set_text_color(ctx, theme_fg());
  graphics_draw_text(ctx, title, tf, GRect(x, y, usable, ts.h + 4),
                     GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  // theme_secondary(), not theme_muted(): muted is LightGray on the light
  // theme's white and is unreadable as text (it is for tracks and dividers).
  // secondary also collapses to fg in Big Mode, which is what that mode wants.
  graphics_context_set_text_color(ctx, theme_secondary());
  graphics_draw_text(ctx, sub, sf, GRect(x, y + ts.h + gap, usable, ss.h + 4),
                     GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
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

