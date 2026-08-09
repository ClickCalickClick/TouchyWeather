#include "cards.h"
#include "../theme.h"
#include "../icons.h"
#include "../ui.h"
#include "../settings.h"
#include "../anim.h"
#include "../weather_data.h"
#include <string.h>

// Copies a "H:MM AM"/"H:MM PM" time string up to (not including) the space,
// so the AM/PM suffix can be dropped when a column header already carries it.
static void prv_strip_ampm(const char *src, char *dst, size_t n) {
  size_t i = 0;
  for (; i + 1 < n && src[i] && src[i] != ' '; ++i) dst[i] = src[i];
  dst[i] = '\0';
}

// Phase 11: Golden Hour card.
//
// Displays the four chronological "magic light" milestones for the
// current day:
//
//   ▮ BLUE  5:42 AM  (morning blue hour begins, sun -6°)
//   ▯ GOLD  6:14 AM  (morning golden hour begins, sun -0.833°)
//   ▯ GOLD  7:32 PM  (evening golden hour begins, sun +6°)
//   ▮ BLUE  8:18 PM  (evening blue hour begins, sun -0.833°)
//
// Layout pattern follows the Sun Cycle card: small color-swatch icon +
// label + time, with all rows vertically equally spaced and uniform
// column widths so times line up cleanly.

typedef struct {
  const char *kind;     // "BLUE" or "GOLD"
  const char *time_str; // h:MM AM/PM
  GColor swatch;        // pill color (yellow/orange for gold, blue for blue)
} GHRow;

static int prv_max_w(GFont f, const char *labels[], int n) {
  int max_w = 0;
  for (int i = 0; i < n; ++i) {
    GSize s = graphics_text_layout_get_content_size(labels[i], f,
        GRect(0, 0, 200, 30), GTextOverflowModeTrailingEllipsis,
        GTextAlignmentLeft);
    if (s.w > max_w) max_w = s.w;
  }
  return max_w;
}

void card_golden_hour_draw(GContext *ctx, GRect bounds) {
  WeatherData *d = weather_data_get();
  int W = bounds.size.w;
  int ox = bounds.origin.x;

  // Header.
  int header_y = UI_HEADER_Y;

  // --- Big Mode (Stage B): 2x2 AM/PM grid — KEEPS all 4 milestones. ---
  // (An earlier plan cut the two blue-hour rows; not allowed — no modal exists
  // for this card, so all four must stay visible.) Rows = blue / gold, columns =
  // AM / PM. Band identity is carried by SHAPE (filled square = blue, outline =
  // gold) so it survives the Big-Mode contrast collapse; the AM/PM column header
  // lets the time suffix drop. Two times per row is wide, so times use
  // ui_font_header (18B small / 24B large) and BLUE/GOLD word labels appear on
  // the large classes only. Mandatory banner kept. Returns early -> Normal 4-row
  // layout below untouched, Big-OFF pixel-identical.
  if (settings_get_big_mode()) {
    ui_draw_card_header_with_icon(ctx, bounds, "GOLDEN HOUR", theme_fg(),
                                  header_y, 18, icon_draw_horizon_sun);
    char am_t[2][8], pm_t[2][8];
    prv_strip_ampm(d->blue_am, am_t[0], sizeof(am_t[0]));  // blue row, AM col
    prv_strip_ampm(d->blue_pm, pm_t[0], sizeof(pm_t[0]));  // blue row, PM col
    prv_strip_ampm(d->gold_am, am_t[1], sizeof(am_t[1]));  // gold row, AM col
    prv_strip_ampm(d->gold_pm, pm_t[1], sizeof(pm_t[1]));  // gold row, PM col
    static const char *words[2] = {"BLUE", "GOLD"};

    GFont tf = ui_font_header();   // 18B small / 24B large (2 times per row)
    GFont hf = ui_font_caption();  // AM/PM column headers
    GFont wf = ui_font_label();    // BLUE/GOLD words (large only)
    int shape, gap, colgap, hdr_y, row1_y, row2_y, th, showword;
#if defined(UI_SCREEN_SMALL_RECT)
    shape = 14; gap = 6; colgap = 12; hdr_y = header_y + UI_HEADER_HEIGHT + 8;  row1_y = 62;  row2_y = 90;  th = 26; showword = 0;
#elif defined(UI_SCREEN_SMALL_ROUND)
    shape = 14; gap = 6; colgap = 12; hdr_y = header_y + UI_HEADER_HEIGHT + 8;  row1_y = 76;  row2_y = 104; th = 26; showword = 0;
#elif defined(UI_SCREEN_LARGE_RECT)
    shape = 18; gap = 10; colgap = 18; hdr_y = header_y + UI_HEADER_HEIGHT + 10; row1_y = 74;  row2_y = 118; th = 32; showword = 1;
#else  // UI_SCREEN_LARGE_ROUND
    shape = 18; gap = 10; colgap = 18; hdr_y = header_y + UI_HEADER_HEIGHT + 14; row1_y = 96;  row2_y = 142; th = 32; showword = 1;
#endif
    // Uniform time-column width across all four cells.
    int time_w = 0, kind_w = 0;
    for (int r = 0; r < 2; ++r) {
      GSize a = graphics_text_layout_get_content_size(am_t[r], tf,
          GRect(0, 0, W, 30), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
      if (a.w > time_w) time_w = a.w;
      GSize p = graphics_text_layout_get_content_size(pm_t[r], tf,
          GRect(0, 0, W, 30), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
      if (p.w > time_w) time_w = p.w;
    }
    if (showword) kind_w = prv_max_w(wf, words, 2);
    int word_col = showword ? (kind_w + gap) : 0;
    int cluster_w = word_col + shape + gap + time_w + colgap + time_w;
    int cluster_x = ox + (W - cluster_w) / 2;
    int floor_x = ox + UI_MARGIN_X;
    if (cluster_x < floor_x) cluster_x = floor_x;
    int shape_x = cluster_x + word_col;
    int am_x = shape_x + shape + gap;
    int pm_x = am_x + time_w + colgap;

    // Column headers.
    graphics_context_set_text_color(ctx, theme_secondary());
    graphics_draw_text(ctx, "AM", hf, GRect(am_x, hdr_y, time_w + 4, 18),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    graphics_draw_text(ctx, "PM", hf, GRect(pm_x, hdr_y, time_w + 4, 18),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

    for (int r = 0; r < 2; ++r) {
      int y = (r == 0) ? row1_y : row2_y;
      GRect sq = GRect(shape_x, y + (th - shape) / 2, shape, shape);
      if (r == 0) {  // BLUE — filled square
        graphics_context_set_fill_color(ctx, theme_fg());
        graphics_fill_rect(ctx, sq, 2, GCornersAll);
      } else {       // GOLD — outline square
        graphics_context_set_stroke_color(ctx, theme_fg());
        graphics_context_set_stroke_width(ctx, 2);
        graphics_draw_rect(ctx, sq);
      }
      if (showword) {
        graphics_context_set_text_color(ctx, theme_fg());
        graphics_draw_text(ctx, words[r], wf, GRect(cluster_x, y, kind_w + 4, th),
            GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
      }
      graphics_context_set_text_color(ctx, theme_fg());
      graphics_draw_text(ctx, am_t[r], tf, GRect(am_x, y, time_w + 4, th),
          GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
      graphics_draw_text(ctx, pm_t[r], tf, GRect(pm_x, y, time_w + 4, th),
          GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    }
    ui_draw_auto_banner(ctx, bounds, d->rain_alert_min, d->last_updated,
                        anim_get_frame());
    return;
  }

  ui_draw_card_header_with_icon(ctx, bounds, "GOLDEN HOUR",
                                theme_fg(),
                                header_y, 18, icon_draw_horizon_sun);

  // Small classes (144x168 / 180x180): the chronological 4-row list needs
  // 4 x 26px below the header, which put its LAST row (evening blue hour)
  // underneath the status pill — a card whose entire purpose is four times
  // showed three (#36). Re-flow to the 2x2 AM/PM grid the Big Mode path above
  // already proved on this hardware: rows = BLUE/GOLD, columns = AM/PM. Two
  // rows of 26px clear the pill with room to spare and all four milestones
  // stay visible. The BLUE/GOLD word column goes with the list (#38): it cost
  // ~46px of a 120px line, and the AM/PM headers plus the chip already say
  // which cell is which.
  //
  // Band identity is carried by SHAPE as well as hue — filled chip = blue
  // hour, outlined chip = golden hour. That is the 1-bit fix (#37, pulled
  // forward from Phase 5/D6): on diorite and flint theme_accent_blue() and
  // theme_accent_orange() both collapse to theme_fg(), so the two chips
  // rendered as identical solid black rectangles and the column carried no
  // information at all. Colour platforms keep the hue on top of the shape.
#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
  char am_t[2][8], pm_t[2][8];
  prv_strip_ampm(d->blue_am, am_t[0], sizeof(am_t[0]));  // blue row, AM col
  prv_strip_ampm(d->blue_pm, pm_t[0], sizeof(pm_t[0]));  // blue row, PM col
  prv_strip_ampm(d->gold_am, am_t[1], sizeof(am_t[1]));  // gold row, AM col
  prv_strip_ampm(d->gold_pm, pm_t[1], sizeof(pm_t[1]));  // gold row, PM col

  GFont grid_font = ui_font_header();   // 18B — two times per row
  GFont col_font  = ui_font_caption();  // AM/PM column headers
  int chip = 14, gap = 6, colgap = 12, th = 26;
  int hdr_y = header_y + UI_HEADER_HEIGHT + 8;
#if defined(UI_SCREEN_SMALL_RECT)
  int row1_y = 62, row2_y = 90;   // row2 ink bottom 116 vs pill top 126
#else
  int row1_y = 76, row2_y = 104;  // chalk: bottom 130 vs pill top 140
#endif

  // Uniform time-column width across all four cells so the columns align.
  int time_w = 0;
  for (int r = 0; r < 2; ++r) {
    GSize a = graphics_text_layout_get_content_size(am_t[r], grid_font,
        GRect(0, 0, W, 30), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
    if (a.w > time_w) time_w = a.w;
    GSize p = graphics_text_layout_get_content_size(pm_t[r], grid_font,
        GRect(0, 0, W, 30), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
    if (p.w > time_w) time_w = p.w;
  }
  int cluster_w = chip + gap + time_w + colgap + time_w;
  int cluster_x = ox + (W - cluster_w) / 2;
  int floor_x = ox + UI_MARGIN_X;
  if (cluster_x < floor_x) cluster_x = floor_x;
  int am_x = cluster_x + chip + gap;
  int pm_x = am_x + time_w + colgap;

  graphics_context_set_text_color(ctx, theme_secondary());
  graphics_draw_text(ctx, "AM", col_font, GRect(am_x, hdr_y, time_w + 4, 18),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  graphics_draw_text(ctx, "PM", col_font, GRect(pm_x, hdr_y, time_w + 4, 18),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  for (int r = 0; r < 2; ++r) {
    int y = (r == 0) ? row1_y : row2_y;
    GRect chip_r = GRect(cluster_x, y + (th - chip) / 2, chip, chip);
    if (r == 0) {  // blue hour — filled chip
      graphics_context_set_fill_color(ctx, theme_accent_blue());
      graphics_fill_rect(ctx, chip_r, 2, GCornersAll);
    } else {       // golden hour — outlined chip
      graphics_context_set_stroke_color(ctx, theme_accent_orange());
      graphics_context_set_stroke_width(ctx, 2);
      graphics_draw_rect(ctx, chip_r);
    }
    graphics_context_set_text_color(ctx, theme_fg());
    graphics_draw_text(ctx, am_t[r], grid_font, GRect(am_x, y, time_w + 4, th),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    graphics_draw_text(ctx, pm_t[r], grid_font, GRect(pm_x, y, time_w + 4, th),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }
#else
  // Build the 4 rows in chronological order. The "swatch" is the small
  // colored pill drawn before the label to communicate band identity.
  GHRow rows[4] = {
    { "BLUE", d->blue_am, theme_accent_blue()   },
    { "GOLD", d->gold_am, theme_accent_orange() },
    { "GOLD", d->gold_pm, theme_accent_orange() },
    { "BLUE", d->blue_pm, theme_accent_blue()   },
  };

  // Uniform column widths: measure max kind-label and max time-string
  // so all four rows align cleanly regardless of "AM"/"PM" character
  // count.
  GFont label_font = ui_font_header();
  GFont time_font  = ui_font_header();

  const char *kinds[4] = { rows[0].kind, rows[1].kind, rows[2].kind, rows[3].kind };
  const char *times[4] = { rows[0].time_str, rows[1].time_str, rows[2].time_str, rows[3].time_str };
  int kind_w = prv_max_w(label_font, kinds, 4);
  int time_w = prv_max_w(time_font, times, 4);

  // Layout constants.
  int swatch_w = 8;
  int swatch_h = 14;
  int gap = 8;
  int cluster_w = swatch_w + gap + kind_w + gap + time_w;
  int cluster_x = ox + (W - cluster_w) / 2;
  int floor_x = ox + UI_MARGIN_X;
  if (cluster_x < floor_x) cluster_x = floor_x;

  // Vertical layout: distribute 4 rows in the area below the header.
  int top_y = header_y + UI_HEADER_HEIGHT + PBL_IF_ROUND_ELSE(14, 10);
  int row_h = PBL_IF_ROUND_ELSE(28, 26);

  for (int i = 0; i < 4; ++i) {
    int y = top_y + i * row_h;

    // Color swatch — small filled pill on the left.
    GRect sw = GRect(cluster_x, y + 4, swatch_w, swatch_h);
    graphics_context_set_fill_color(ctx, rows[i].swatch);
    graphics_fill_rect(ctx, sw, 2, GCornersAll);

    // Kind label ("BLUE" / "GOLD") — colored to match swatch for legibility.
    int label_x = cluster_x + swatch_w + gap;
    graphics_context_set_text_color(ctx, rows[i].swatch);
    graphics_draw_text(ctx, rows[i].kind, label_font,
        GRect(label_x, y, kind_w + 4, 22),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

    // Time — primary fg color for max contrast.
    int time_x = label_x + kind_w + gap;
    graphics_context_set_text_color(ctx, theme_fg());
    graphics_draw_text(ctx, rows[i].time_str, time_font,
        GRect(time_x, y, time_w + 4, 22),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }
#endif

  ui_draw_auto_banner(ctx, bounds, d->rain_alert_min, d->last_updated,
                      anim_get_frame());
}
