#include "update_notes.h"
#include "theme.h"
#include "version_gen.h"

// Persisted code of the last release whose notes the user has seen. Distinct
// from the other persist keys (1=theme, 105=cache, 200+=settings, 300=wakeup).
#define PERSIST_KEY_NOTES_VERSION 106

#define MARGIN 6
#define TITLE_H 28
#define BODY_TOP (TITLE_H + 8)

static Window *s_win;
static ScrollLayer *s_scroll;
static TextLayer *s_title;
static TextLayer *s_body;

static void prv_load(Window *window) {
  window_set_background_color(window, theme_bg());
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_bounds(root);
  int w = b.size.w - 2 * MARGIN;

  s_scroll = scroll_layer_create(b);
  scroll_layer_set_shadow_hidden(s_scroll, true);
  // Wires UP/DOWN to scroll; BACK keeps the system default (pop = dismiss).
  scroll_layer_set_click_config_onto_window(s_scroll, window);

  s_title = text_layer_create(GRect(MARGIN, 4, w, TITLE_H));
  text_layer_set_background_color(s_title, GColorClear);
  text_layer_set_text_color(s_title, theme_fg());
  text_layer_set_font(s_title, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_title, GTextAlignmentCenter);
  text_layer_set_text(s_title, "What's New");

  // Measure the wrapped notes height up front so the scroll content fits.
  GFont body_font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  GSize notes = graphics_text_layout_get_content_size(
      APP_UPDATE_NOTES, body_font, GRect(0, 0, w, 4000),
      GTextOverflowModeWordWrap, GTextAlignmentLeft);

  s_body = text_layer_create(GRect(MARGIN, BODY_TOP, w, notes.h + 8));
  text_layer_set_background_color(s_body, GColorClear);
  text_layer_set_text_color(s_body, theme_fg());
  text_layer_set_font(s_body, body_font);
  text_layer_set_text_alignment(s_body, GTextAlignmentLeft);
  text_layer_set_overflow_mode(s_body, GTextOverflowModeWordWrap);
  text_layer_set_text(s_body, APP_UPDATE_NOTES);

  scroll_layer_add_child(s_scroll, text_layer_get_layer(s_title));
  scroll_layer_add_child(s_scroll, text_layer_get_layer(s_body));
  scroll_layer_set_content_size(s_scroll, GSize(b.size.w, BODY_TOP + notes.h + 14));

  layer_add_child(root, scroll_layer_get_layer(s_scroll));
}

static void prv_unload(Window *window) {
  if (s_body) { text_layer_destroy(s_body); s_body = NULL; }
  if (s_title) { text_layer_destroy(s_title); s_title = NULL; }
  if (s_scroll) { scroll_layer_destroy(s_scroll); s_scroll = NULL; }
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
