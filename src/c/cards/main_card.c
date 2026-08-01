#include "cards.h"
#include "../theme.h"
#include "../icons.h"
#include "../ui.h"
#include "../settings.h"
#include "../weather_data.h"
#include "../anim.h"
#include <stdio.h>

#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
#include "../ui_layout.h"

// --- Phase 3 (D4 + D5): small-class flow layout ---------------------------
//
// This card used to place the location label, the hero icon, the big temp and
// the hi/lo column at hard-coded y anchors keyed off emery's 228px screen. At
// 144/180px they all landed in the same ~30px band and drew on top of one
// another: the icon over the location text (#10), the icon over the temp's
// degree glyph (#11), the icon over the hi/lo up-arrow (#12), and on chalk the
// hi value's degree ran under the bezel (#13). Hiding the location left a hole,
// which a `block_shift` fudge tried to re-center by hand.
//
// Every element is now a row in a measured stack (ui_layout.c), so nothing
// shares a band and nothing can overlap; a hidden row reflows the rest natively
// and `block_shift` is gone. The middle row is the watchface's weather idiom —
// [icon] [temp] [hi over lo], three columns on ONE midline, measured as a
// cluster and centered.
//
// Row heights are VISIBLE INK, not the font's layout box: a Pebble system font
// carries dead top-side leading (10px on GOTHIC_24_BOLD), and reserving it is
// what made hand-tuned stacks look top-heavy. Each text box is drawn lifted by
// its font's rise so the glyph, not the box, lands on the reserved band.
#if defined(UI_SCREEN_SMALL_ROUND)
  #define MC_ICON_START     38
  #define MC_CLUSTER_GAP     8
#else  // UI_SCREEN_SMALL_RECT
  #define MC_ICON_START     28
  #define MC_CLUSTER_GAP     6
#endif
// Floor for the cluster's width giveback — below this a sun is a speck, so we
// accept the overflow instead of shrinking further (watchface FZ_ICON_MIN).
#define MC_ICON_MIN         18
#define MC_HILO_PITCH       20  // row-to-row spacing of the stacked hi/lo pair
#define MC_HILO_ARROW_COL   12  // arrow drawn at x+5, its number at x+12
#define MC_ARROW_SIZE       10
#define MC_TEXT_RISE         3  // lift for a vertically CENTERED box (temp, hi/lo)
#define MC_LABEL_INK_H       9  // GOTHIC_14_BOLD ink height...
#define MC_LABEL_RISE        6  // ...and its top-side leading
#define MC_FEELS_INK_H      11  // GOTHIC_18_BOLD ink height...
#define MC_FEELS_RISE        7  // ...and its top-side leading
#define MC_WIND_ROW_H       16  // sized by the row's glyphs, not its 9px of text

// Stacked hi/lo mini column: up-arrow + high (orange) over down-arrow + low
// (blue). `x` is the left edge of the arrow gutter and `cy` the vertical CENTER
// of the pair, so the two rows straddle the same midline the icon and temp sit
// on rather than hanging below it — hanging below it is what buried the
// up-arrow under the hero icon (#12).
static void prv_draw_hilo(GContext *ctx, int x, int cy, int w, GFont f,
                          const WeatherData *d) {
  char hi_buf[8], lo_buf[8];
  snprintf(hi_buf, sizeof(hi_buf), "%d°", d->high);
  snprintf(lo_buf, sizeof(lo_buf), "%d°", d->low);
  const int hi_cy = cy - MC_HILO_PITCH / 2;
  const int lo_cy = cy + MC_HILO_PITCH / 2;
  const int tx = x + MC_HILO_ARROW_COL;
  const int tw = w - MC_HILO_ARROW_COL;
  // The box is a couple of px taller than the pitch so an 18px bold line box
  // is never clipped; the extra height hangs BELOW the ink, where nothing else
  // is drawn, and the text is top-anchored so the glyph doesn't move.
  const int th = MC_HILO_PITCH + 4;
  icon_draw_arrow_up(ctx, GPoint(x + 5, hi_cy), MC_ARROW_SIZE,
                     theme_accent_orange());
  graphics_context_set_text_color(ctx, theme_accent_orange());
  graphics_draw_text(ctx, hi_buf, f,
      GRect(tx, hi_cy - MC_HILO_PITCH / 2 - MC_TEXT_RISE, tw, th),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  icon_draw_arrow_down(ctx, GPoint(x + 5, lo_cy), MC_ARROW_SIZE,
                       theme_accent_blue());
  graphics_context_set_text_color(ctx, theme_accent_blue());
  graphics_draw_text(ctx, lo_buf, f,
      GRect(tx, lo_cy - MC_HILO_PITCH / 2 - MC_TEXT_RISE, tw, th),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}
#endif  // UI_SCREEN_SMALL_RECT || UI_SCREEN_SMALL_ROUND

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

#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
  // --- Normal mode, small classes: measured row stack (see the header) ---
  (void)H;
  (void)margin;

  // The weather row is as tall as its tallest member — the icon or the stacked
  // hi/lo pair. The temp's ink is shorter than both at this tier.
  const int weather_h = (MC_ICON_START > 2 * MC_HILO_PITCH)
                        ? MC_ICON_START : 2 * MC_HILO_PITCH;

  // D5: the location is its own row, so switching it off (Clay "Show location",
  // default OFF) recenters everything below with no hole left behind. Order is
  // the D4 sketch's: it also lands the widest row — the weather cluster — on the
  // band's midline, which on chalk is where the chord is widest.
  enum { MC_ROW_LOC, MC_ROW_FEELS, MC_ROW_WEATHER, MC_ROW_WIND, MC_ROW_COUNT };
  UILayoutRow rows[MC_ROW_COUNT] = {
    [MC_ROW_LOC]     = { .present = d->show_location && d->location_name[0],
                         .h = MC_LABEL_INK_H },
    [MC_ROW_FEELS]   = { .present = true, .h = MC_FEELS_INK_H },
    [MC_ROW_WEATHER] = { .present = true, .h = weather_h },
    [MC_ROW_WIND]    = { .present = true, .h = MC_WIND_ROW_H },
  };
  GRect avail = GRect(ox, bounds.origin.y, W,
                      ui_content_bottom(bounds) - bounds.origin.y);
  ui_layout_solve(rows, MC_ROW_COUNT, avail);

  // Location name. rows[].w is chord-clamped on chalk, so a long name
  // ellipsizes at the glass edge instead of running under the bezel.
  if (rows[MC_ROW_LOC].present) {
    graphics_context_set_text_color(ctx, theme_fg());
    graphics_draw_text(ctx, d->location_name, ui_font_label(),
        GRect(rows[MC_ROW_LOC].x, rows[MC_ROW_LOC].y - MC_LABEL_RISE,
              rows[MC_ROW_LOC].w, MC_LABEL_RISE + rows[MC_ROW_LOC].h + 4),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }

  // "FEELS 75°" — ui_font_header (18B), NOT the 24B the large classes use. At
  // 24B against a 28B hero temp the two readings are the same visual weight,
  // and since D4 puts this row ABOVE the temp the eye lands on the feels-like
  // number first. 18B makes it read as the secondary value it is and gives the
  // card a clean 28 / 18 / 14 ramp.
  {
    char feels_buf[16];
    snprintf(feels_buf, sizeof(feels_buf), "FEELS %d°", d->feels_like);
    graphics_context_set_text_color(ctx, theme_fg());
    graphics_draw_text(ctx, feels_buf, ui_font_header(),
        GRect(rows[MC_ROW_FEELS].x, rows[MC_ROW_FEELS].y - MC_FEELS_RISE,
              rows[MC_ROW_FEELS].w, MC_FEELS_RISE + rows[MC_ROW_FEELS].h + 4),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  }

  // Weather row: [icon] [temp] [hi over lo] as one centered cluster on a single
  // midline. The temp and the hi/lo column are MEASURED, so the cluster centers
  // on real content instead of on a fixed reserve that pads out for short
  // values and clips for long ones.
  //
  // The temp font is GOTHIC_28_BOLD (ui_font_title), not the LECO the large
  // classes hero with: "72°" is 66px of LECO_36, over half the 120px line, and
  // no icon and hi/lo column fit beside it. This is the tier the watchface
  // settled on for the same row on the same hardware, and being a full font it
  // carries real degree and minus glyphs.
  {
    const int row_cy = rows[MC_ROW_WEATHER].cy;
    const int row_w = rows[MC_ROW_WEATHER].w;
    GFont temp_font = ui_font_title();
    GFont hilo_font = ui_font_header();

    char temp_buf[8];
    snprintf(temp_buf, sizeof(temp_buf), "%d°", d->temp);
    GSize temp_sz = graphics_text_layout_get_content_size(temp_buf, temp_font,
        GRect(0, 0, W, 50), GTextOverflowModeFill, GTextAlignmentLeft);

    char hi_b[8], lo_b[8];
    snprintf(hi_b, sizeof(hi_b), "%d°", d->high);
    snprintf(lo_b, sizeof(lo_b), "%d°", d->low);
    GSize hi_sz = graphics_text_layout_get_content_size(hi_b, hilo_font,
        GRect(0, 0, W, 30), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
    GSize lo_sz = graphics_text_layout_get_content_size(lo_b, hilo_font,
        GRect(0, 0, W, 30), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
    const int hilo_w = MC_HILO_ARROW_COL +
                       (hi_sz.w > lo_sz.w ? hi_sz.w : lo_sz.w);

    int icon = MC_ICON_START;
    int cluster = icon + MC_CLUSTER_GAP + temp_sz.w + MC_CLUSTER_GAP + hilo_w;
    // Too wide (a three-digit or sub-zero temp, or simply a tight line)? Give
    // width back from the icon rather than let the readings run off the edge —
    // a smaller sun still reads as a sun, a clipped "↑96°" does not.
    if (cluster > row_w) {
      int over = cluster - row_w;
      int room = icon - MC_ICON_MIN;
      int shrink = (room < over) ? room : over;
      if (shrink > 0) {
        icon -= shrink;
        cluster -= shrink;
      }
    }
    int cx = rows[MC_ROW_WEATHER].x + (row_w - cluster) / 2;
    if (cx < rows[MC_ROW_WEATHER].x) cx = rows[MC_ROW_WEATHER].x;

    icon_draw_condition_animated(ctx, GPoint(cx + icon / 2, row_cy),
                                 icon, d->condition, anim_get_frame());
    graphics_context_set_text_color(ctx, theme_fg());
    graphics_draw_text(ctx, temp_buf, temp_font,
        GRect(cx + icon + MC_CLUSTER_GAP,
              row_cy - temp_sz.h / 2 - MC_TEXT_RISE,
              temp_sz.w + 4, temp_sz.h + 2),
        GTextOverflowModeFill, GTextAlignmentLeft, NULL);
    prv_draw_hilo(ctx, cx + icon + MC_CLUSTER_GAP + temp_sz.w + MC_CLUSTER_GAP,
                  row_cy, hilo_w, hilo_font, d);
  }

  // Bottom row: [wind glyph] 12MPH NW | [droplet] 58%, measured as one cluster
  // and centered — the old fixed W/2 split gave the wind column a ~2px left
  // gutter against the card's 12px right one (#17).
  //
  // The leading glyphs are the first thing to go when the line won't hold: at
  // GOTHIC_14_BOLD a metric worst case ("12KMH NNW") is a few px wider than the
  // 120px rect line can take with them, and a clipped reading is worse than a
  // missing decoration.
  {
    const int row_cy = rows[MC_ROW_WIND].cy;
    const int row_w = rows[MC_ROW_WIND].w;
    GFont lf = ui_font_label();

    char wind_buf[16];
    const char *wind_unit = (d->units == UNITS_METRIC) ? "KMH" : "MPH";
    snprintf(wind_buf, sizeof(wind_buf), "%d%s %s",
             d->wind_speed, wind_unit, d->wind_dir);
    // Humidity or dew point, per the Clay "Show dew point" switch.
    char hum_buf[8];
    if (d->use_dew_point) {
      snprintf(hum_buf, sizeof(hum_buf), "%d°", d->dew_point);
    } else {
      snprintf(hum_buf, sizeof(hum_buf), "%d%%", d->humidity);
    }

    GSize wind_sz = graphics_text_layout_get_content_size(wind_buf, lf,
        GRect(0, 0, W, 30), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
    GSize hum_sz = graphics_text_layout_get_content_size(hum_buf, lf,
        GRect(0, 0, W, 30), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);

#if defined(UI_SCREEN_SMALL_ROUND)
    const int wind_icon = 14;  // chalk's ~156px line holds both glyphs
#else
    // SMALL_RECT's 120px line holds one glyph, not two — at GOTHIC_14_BOLD a
    // two-digit wind speed spends every pixel the divider leaves over. The
    // swoosh is the one to lose: "MPH"/"KMH" already labels that reading. The
    // droplet stays, because it is what keeps "49%" from reading as a rain
    // chance and, in dew-point mode, "49°" from reading as a temperature.
    const int wind_icon = 0;
#endif
    // 14px, not 12: below that the droplet loses its shape and renders as a
    // plain disc.
    const int hum_icon = 14, glyph_gap = 3, sep_gap = 4;
    const int chrome = sep_gap + 1 + sep_gap;  // the divider and its air
    const int text_w = wind_sz.w + hum_sz.w;
    const int glyph_w = (wind_icon ? wind_icon + glyph_gap : 0) +
                        (hum_icon ? hum_icon + glyph_gap : 0);
    // Backstop for the widest data (metric "12KMH NNW" against "100%"): the
    // glyphs are decoration, the readings are not.
    const bool show_glyphs = (text_w + chrome + glyph_w <= row_w);
    const int cluster = text_w + chrome + (show_glyphs ? glyph_w : 0);

    int x = rows[MC_ROW_WIND].x + (row_w - cluster) / 2;
    if (x < rows[MC_ROW_WIND].x) x = rows[MC_ROW_WIND].x;
    // Text sits on its own ink band, centered on the row's midline; the glyphs
    // center on the same midline.
    const int ty = row_cy - MC_LABEL_INK_H / 2 - MC_LABEL_RISE;
    const int th = MC_LABEL_RISE + MC_LABEL_INK_H + 4;

    if (show_glyphs && wind_icon) {
      icon_draw_wind(ctx, GPoint(x + wind_icon / 2, row_cy), wind_icon,
                     theme_fg());
      x += wind_icon + glyph_gap;
    }
    graphics_context_set_text_color(ctx, theme_fg());
    graphics_draw_text(ctx, wind_buf, lf, GRect(x, ty, wind_sz.w + 2, th),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    x += wind_sz.w + sep_gap;

    // Solid 1px in secondary, NOT muted: a 1px muted stroke quantizes to the
    // background on the 1-bit platforms and the divider vanishes (D6 rule 1).
    graphics_context_set_stroke_color(ctx, theme_secondary());
    graphics_context_set_stroke_width(ctx, 1);
    graphics_draw_line(ctx, GPoint(x, row_cy - MC_WIND_ROW_H / 2),
                       GPoint(x, row_cy + MC_WIND_ROW_H / 2));
    x += 1 + sep_gap;

    if (show_glyphs && hum_icon) {
      icon_draw_droplet(ctx, GPoint(x + hum_icon / 2, row_cy), hum_icon,
                        theme_accent_blue());
      x += hum_icon + glyph_gap;
    }
    graphics_context_set_text_color(ctx, theme_fg());
    graphics_draw_text(ctx, hum_buf, lf, GRect(x, ty, hum_sz.w + 2, th),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }
#else
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
  //
  // LOCKED PATH: emery (large-rect) and gabbro (large-round) only. Every
  // expression below is the verbatim pre-Phase-5 one and must stay that way —
  // tools/lock_guard.py fails the build if their binaries move. The two small
  // classes took their own compressed anchors here until Phase 3 replaced the
  // whole approach with the measured row stack above.
  //
  // When the location label is hidden (Clay "Show location" OFF, the default)
  // the hero block re-centers vertically between the top of the screen and the
  // wind row, so the card isn't top-heavy with the location gone. The wind row,
  // pill and page indicator stay put — nothing below moves and no other card is
  // affected. When location is ON, block_shift is 0 and the layout matches the
  // location-enabled design.
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
  // from temp_y, so they follow. Kept at this position (after block_shift) so
  // gabbro's codegen is byte-for-byte unchanged.
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
#endif  // UI_SCREEN_SMALL_RECT || UI_SCREEN_SMALL_ROUND

  // Rotating status banner (rain ⇄ updated). The lowered pill position now
  // lives in ui_draw_status_banner's pad_bottom so every card matches, so we
  // hand it the plain bounds here (no per-card shift).
  ui_draw_auto_banner(ctx, bounds, d->rain_alert_min, d->last_updated,
                      anim_get_frame());
}
