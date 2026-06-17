#include "update_notes.h"
#include "theme.h"
#include "ui.h"
#include "icons.h"
#include "anim.h"
#include "weather_data.h"
#include "version_gen.h"
#include <string.h>

// Persisted code of the last release whose notes the user has seen. Distinct
// from the other persist keys (1=theme, 105=cache, 200+=settings, 300=wakeup).
#define PERSIST_KEY_NOTES_VERSION 106

// Poetic, on-brand headline (matches the app's "Checking the horizon..." voice).
// One-line change if you want different copy.
#define HEADLINE "New on the horizon"

#define SUN_SIZE 32
#define MARKER_INDENT 14   // text indent past the accent marker

static Window *s_win;
static Layer *s_header;
static ScrollLayer *s_scroll;
static Layer *s_body;
static AppTimer *s_timer;

// Computed in load so the header lays out around the (possibly wrapped) headline.
static int s_sun_cy, s_head_y, s_head_h, s_ver_y;

// ---------------------------------------------------------------------------
// Body: one accent-marked bullet per CHANGELOG line. Shared by measure + draw
// (ctx == NULL measures only). Returns the total content height.
// ---------------------------------------------------------------------------
static int prv_layout_body(GContext *ctx, int w) {
  GFont f = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  int x_text = UI_MARGIN_X + MARKER_INDENT;
  int w_text = w - x_text - UI_MARGIN_X;
  if (w_text < 20) w_text = 20;

  // strtok mutates its buffer, so work on a copy of the notes each call.
  static char buf[600];
  strncpy(buf, APP_UPDATE_NOTES, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';

  // Split ALL lines up front with a manual '\n' scan (strtok proved unreliable
  // here — likely clobbered by graphics_text_layout's own string handling).
  char *lines[16];
  int n = 0;
  lines[n++] = buf;
  for (char *p = buf; *p && n < 16; p++) {
    if (*p == '\n') {
      *p = '\0';
      lines[n++] = p + 1;
    }
  }

  int y = 6;
  for (int i = 0; i < n; i++) {
    char *line = lines[i];
    GSize ls = graphics_text_layout_get_content_size(
        line, f, GRect(0, 0, w_text, 1000),
        GTextOverflowModeWordWrap, GTextAlignmentLeft);

    if (ctx) {
      // Accent triangle marker, aligned to the first text line.
      int my = y + 8;
      static GPoint tp[3];
      tp[0] = GPoint(UI_MARGIN_X, my - 4);
      tp[1] = GPoint(UI_MARGIN_X, my + 4);
      tp[2] = GPoint(UI_MARGIN_X + 7, my);
      GPathInfo pi = { 3, tp };
      GPath *tri = gpath_create(&pi);
      graphics_context_set_fill_color(ctx, theme_accent_orange());
      gpath_draw_filled(ctx, tri);
      gpath_destroy(tri);

      graphics_context_set_text_color(ctx, theme_fg());
      graphics_draw_text(ctx, line, f, GRect(x_text, y, w_text, ls.h + 6),
                         GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
    }
    y += ls.h + 10;
  }
  return y + 6;
}

static void prv_body_update(Layer *layer, GContext *ctx) {
  prv_layout_body(ctx, layer_get_bounds(layer).size.w);
}

// ---------------------------------------------------------------------------
// Header: animated sun, headline, and a "····  vX.Y.Z  ····" version divider.
// ---------------------------------------------------------------------------
static void prv_header_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  int cx = b.size.w / 2;

  // Rotating sun (chrome-yellow rays) — the app's signature icon.
  icon_draw_condition_animated(ctx, GPoint(cx, s_sun_cy), SUN_SIZE,
                               COND_SUNNY, anim_get_frame());

  // Headline.
  graphics_context_set_text_color(ctx, theme_fg());
  graphics_draw_text(ctx, HEADLINE,
                     fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                     GRect(4, s_head_y, b.size.w - 8, s_head_h + 4),
                     GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);

  // Version centered in a dotted divider: "····  v1.1.0  ····".
  const char *ver = "v" APP_VERSION_LABEL;
  GFont vf = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
  GSize vs = graphics_text_layout_get_content_size(
      ver, vf, GRect(0, 0, b.size.w, 20),
      GTextOverflowModeFill, GTextAlignmentCenter);
  int line_y = s_ver_y + vs.h / 2;
  ui_draw_dotted_hline(ctx, UI_MARGIN_X, cx - vs.w / 2 - 8, line_y, theme_muted());
  ui_draw_dotted_hline(ctx, cx + vs.w / 2 + 8, b.size.w - UI_MARGIN_X, line_y,
                       theme_muted());
  graphics_context_set_text_color(ctx, theme_secondary());
  graphics_draw_text(ctx, ver, vf,
                     GRect(cx - vs.w / 2 - 6, s_ver_y, vs.w + 12, vs.h + 4),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

// Repaint the header ~10 Hz so the sun animates. anim_get_frame() advances
// globally; we just need to mark the header dirty.
static void prv_tick(void *data) {
  if (s_header) layer_mark_dirty(s_header);
  s_timer = app_timer_register(100, prv_tick, NULL);
}

static void prv_load(Window *window) {
  window_set_background_color(window, theme_bg());
  Layer *root = window_get_root_layer(window);
  GRect rb = layer_get_bounds(root);
  int w = rb.size.w;

  // Lay out the header around the (possibly two-line) headline.
  GSize hs = graphics_text_layout_get_content_size(
      HEADLINE, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
      GRect(0, 0, w - 8, 64), GTextOverflowModeWordWrap, GTextAlignmentCenter);
  s_sun_cy = UI_HEADER_Y + SUN_SIZE / 2;
  s_head_y = s_sun_cy + SUN_SIZE / 2 + 6;
  s_head_h = hs.h;
  s_ver_y = s_head_y + s_head_h + 8;
  int header_h = s_ver_y + 24;

  s_header = layer_create(GRect(0, 0, w, header_h));
  layer_set_update_proc(s_header, prv_header_update);
  layer_add_child(root, s_header);

  s_scroll = scroll_layer_create(GRect(0, header_h, w, rb.size.h - header_h));
  scroll_layer_set_shadow_hidden(s_scroll, true);
  // Wires UP/DOWN to scroll; BACK keeps the default pop (dismiss).
  scroll_layer_set_click_config_onto_window(s_scroll, window);

  int body_h = prv_layout_body(NULL, w);
  s_body = layer_create(GRect(0, 0, w, body_h));
  layer_set_update_proc(s_body, prv_body_update);
  scroll_layer_add_child(s_scroll, s_body);
  scroll_layer_set_content_size(s_scroll, GSize(w, body_h));
  layer_add_child(root, scroll_layer_get_layer(s_scroll));

  s_timer = app_timer_register(100, prv_tick, NULL);
}

static void prv_unload(Window *window) {
  if (s_timer) { app_timer_cancel(s_timer); s_timer = NULL; }
  if (s_body) { layer_destroy(s_body); s_body = NULL; }
  if (s_scroll) { scroll_layer_destroy(s_scroll); s_scroll = NULL; }
  if (s_header) { layer_destroy(s_header); s_header = NULL; }
}

void update_notes_maybe_show(void) {
  int32_t seen = persist_exists(PERSIST_KEY_NOTES_VERSION)
      ? persist_read_int(PERSIST_KEY_NOTES_VERSION) : -1;
  if (seen == APP_VERSION_CODE) return;  // already seen this version

  s_win = window_create();
  window_set_background_color(s_win, theme_bg());
  window_set_window_handlers(s_win, (WindowHandlers) {
    .load = prv_load,
    .unload = prv_unload,
  });
  window_stack_push(s_win, true);

  // Record immediately so the screen shows at most once, even if the user
  // force-quits before dismissing it.
  persist_write_int(PERSIST_KEY_NOTES_VERSION, APP_VERSION_CODE);
}

void update_notes_deinit(void) {
  if (s_win) { window_destroy(s_win); s_win = NULL; }
}

// --- Touch-drag scrolling -------------------------------------------------
// The screen is "showing" exactly while its layers are loaded (s_scroll is
// created in .load and cleared in .unload), so these no-op once dismissed.

static int16_t s_drag_start_y;
static int s_drag_start_off;

static bool prv_showing(void) { return s_scroll != NULL; }

bool update_notes_on_touchdown(int16_t x, int16_t y) {
  (void)x;
  if (!prv_showing()) return false;
  s_drag_start_y = y;
  s_drag_start_off = scroll_layer_get_content_offset(s_scroll).y;
  return true;
}

bool update_notes_on_move(int16_t x, int16_t y) {
  (void)x;
  if (!prv_showing()) return false;
  // Content follows the finger: drag up → scroll down (offset more negative).
  int off = s_drag_start_off + (y - s_drag_start_y);
  GSize content = scroll_layer_get_content_size(s_scroll);
  int frame_h = layer_get_frame(scroll_layer_get_layer(s_scroll)).size.h;
  int min_off = frame_h - content.h;   // most-negative (bottom); 0 if it fits
  if (min_off > 0) min_off = 0;
  if (off > 0) off = 0;
  if (off < min_off) off = min_off;
  scroll_layer_set_content_offset(s_scroll, GPoint(0, off), false);
  return true;
}

bool update_notes_on_liftoff(int16_t x, int16_t y) {
  (void)x; (void)y;
  return prv_showing();  // swallow liftoff so it isn't treated as a tap/swipe
}
