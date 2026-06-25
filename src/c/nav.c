#include "nav.h"
#include "theme.h"
#include "settings.h"

static Card s_cards[NAV_MAX_CARDS];
static bool s_enabled[NAV_MAX_CARDS];
static int s_card_count = 0;
static int s_current = 0;

// Traversal order: s_traversal[pos] = card index visited at slot `pos`.
// Defaults to identity (registration order) and stays in sync as cards
// register. Callers may override via nav_set_traversal().
static int s_traversal[NAV_MAX_CARDS];

static Layer *s_card_layer = NULL;
static Layer *s_indicator_layer = NULL;

// Phase 10F: slide transition state.
//
// When the user navigates between cards we run a 200ms horizontal
// push slide. We DON'T spin up a new AppTimer for each transition;
// instead nav_tick_transition() is called from the existing 100ms
// anim ticker, plus we register a short-lived 30ms AppTimer for the
// duration of the slide so the motion stays smooth without bumping
// the global tick rate.
#define NAV_TRANSITION_DURATION_MS 200
#define NAV_TRANSITION_FRAME_MS    30

static bool s_anim_active = false;
static int  s_anim_from_idx = 0;
static int  s_anim_to_idx = 0;
static int  s_anim_dir = 0;          // +1 = next, -1 = prev
static uint64_t s_anim_start_ms = 0;
static AppTimer *s_anim_timer = NULL;

static uint64_t prv_now_ms(void) {
  time_t s; uint16_t ms;
  time_ms(&s, &ms);
  return (uint64_t)s * 1000ULL + (uint64_t)ms;
}

static void prv_anim_tick(void *ctx) {
  (void)ctx;
  s_anim_timer = NULL;
  if (!s_anim_active) return;
  uint64_t now = prv_now_ms();
  uint64_t elapsed = now - s_anim_start_ms;
  if (elapsed >= NAV_TRANSITION_DURATION_MS) {
    // Snap to final state.
    s_anim_active = false;
    s_current = s_anim_to_idx;
    if (s_card_layer) layer_mark_dirty(s_card_layer);
    if (s_indicator_layer) layer_mark_dirty(s_indicator_layer);
    return;
  }
  if (s_card_layer) layer_mark_dirty(s_card_layer);
  s_anim_timer = app_timer_register(NAV_TRANSITION_FRAME_MS,
                                    prv_anim_tick, NULL);
}

static void prv_start_transition(int new_idx, int dir) {
  if (s_anim_active) return;
  if (new_idx == s_current) return;
  s_anim_from_idx = s_current;
  s_anim_to_idx = new_idx;
  s_anim_dir = dir;
  s_anim_active = true;
  s_anim_start_ms = prv_now_ms();
  // Indicator jumps immediately to the new card so dot sync feels
  // tight; only the card-layer content slides.
  s_current = new_idx;
  if (s_indicator_layer) layer_mark_dirty(s_indicator_layer);
  if (s_card_layer) layer_mark_dirty(s_card_layer);
  if (s_anim_timer) app_timer_cancel(s_anim_timer);
  s_anim_timer = app_timer_register(NAV_TRANSITION_FRAME_MS,
                                    prv_anim_tick, NULL);
}

bool nav_is_transitioning(void) { return s_anim_active; }

static void card_layer_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, theme_bg());
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  if (s_anim_active) {
    // Eased progress (ease-out quadratic): visible motion is fast at
    // the start and decelerates into the new card.
    uint64_t now = prv_now_ms();
    uint64_t elapsed = now - s_anim_start_ms;
    if (elapsed > NAV_TRANSITION_DURATION_MS) elapsed = NAV_TRANSITION_DURATION_MS;
    int32_t p1k = (int32_t)((elapsed * 1000) / NAV_TRANSITION_DURATION_MS); // 0..1000
    // ease-out: 1 - (1-p)^2  with fixed-point math.
    int32_t inv = 1000 - p1k;
    int32_t eased1k = 1000 - (inv * inv) / 1000;  // 0..1000

    int W = bounds.size.w;
    // shift: from card moves -dir*W*eased; to card moves from +dir*W to 0
    int from_dx = -s_anim_dir * (W * eased1k) / 1000;
    int to_dx   =  s_anim_dir * W + from_dx;

    GRect from_b = bounds; from_b.origin.x = bounds.origin.x + from_dx;
    GRect to_b   = bounds; to_b.origin.x   = bounds.origin.x + to_dx;

    if (s_anim_from_idx >= 0 && s_anim_from_idx < s_card_count &&
        s_cards[s_anim_from_idx].draw) {
      s_cards[s_anim_from_idx].draw(ctx, from_b);
    }
    if (s_anim_to_idx >= 0 && s_anim_to_idx < s_card_count &&
        s_cards[s_anim_to_idx].draw) {
      s_cards[s_anim_to_idx].draw(ctx, to_b);
    }
    return;
  }

  if (s_current >= 0 && s_current < s_card_count && s_cards[s_current].draw) {
    s_cards[s_current].draw(ctx, bounds);
  }
}

static void indicator_layer_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  // Build the enabled-only view: count = enabled cards, active = position
  // of current card within the enabled subset.
  nav_draw_page_indicator(ctx, b,
                          nav_active_enabled_index(),
                          nav_count_enabled());
}

void nav_draw_page_indicator(GContext *ctx, GRect bounds, int active_index, int total) {
  if (total <= 0) return;
  // Active dot = pill (16w x 4h, radius 2). Inactive = circle 4x4.
  const int dot_size = 4;
  const int active_w = 16;
  const int gap = 6;
  int total_w = total * dot_size + (total - 1) * gap + (active_w - dot_size);
  int x = bounds.origin.x + (bounds.size.w - total_w) / 2;
  int y = bounds.origin.y + (bounds.size.h - dot_size) / 2;

  for (int i = 0; i < total; ++i) {
    if (i == active_index) {
      GRect r = GRect(x, y, active_w, dot_size);
      graphics_context_set_fill_color(ctx, theme_indicator_active());
      graphics_fill_rect(ctx, r, 2, GCornersAll);
      x += active_w + gap;
    } else {
      GRect r = GRect(x, y, dot_size, dot_size);
      graphics_context_set_fill_color(ctx, theme_indicator_inactive());
      graphics_fill_rect(ctx, r, 2, GCornersAll);
      x += dot_size + gap;
    }
  }
}

void nav_init(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect rb = layer_get_bounds(root);

  // Card layer fills root.
  s_card_layer = layer_create(rb);
  layer_set_update_proc(s_card_layer, card_layer_update);
  layer_add_child(root, s_card_layer);

  // Page indicator overlay anchored near bottom safe-zone. Shared overlay
  // drawn for every card, so it stays unified across cards.
  // gabbro round 260x260 → y=229 (224 + 5, tracking the down-nudged layout).
  // emery 200x228 → y = h-14 (was h-16; +2 to track the 2px down-nudge).
  int indicator_y = PBL_IF_ROUND_ELSE(229, rb.size.h - 14);
  GRect ib = GRect(0, indicator_y, rb.size.w, 8);
  s_indicator_layer = layer_create(ib);
  layer_set_update_proc(s_indicator_layer, indicator_layer_update);
  layer_add_child(root, s_indicator_layer);
}

void nav_deinit(void) {
  if (s_anim_timer) { app_timer_cancel(s_anim_timer); s_anim_timer = NULL; }
  s_anim_active = false;
  if (s_card_layer) { layer_destroy(s_card_layer); s_card_layer = NULL; }
  if (s_indicator_layer) { layer_destroy(s_indicator_layer); s_indicator_layer = NULL; }
}

void nav_register(const char *name, CardDrawFn draw) {
  if (s_card_count >= NAV_MAX_CARDS) return;
  s_cards[s_card_count].name = name;
  s_cards[s_card_count].draw = draw;
  s_enabled[s_card_count] = true;
  s_traversal[s_card_count] = s_card_count;
  s_card_count++;
}

int nav_current_index(void) { return s_current; }
int nav_count(void) { return s_card_count; }

const char *nav_current_name(void) {
  if (s_current < 0 || s_current >= s_card_count) return "";
  return s_cards[s_current].name ? s_cards[s_current].name : "";
}

void nav_show_index(int idx) {
  if (s_card_count == 0) return;
  // Wrap.
  while (idx < 0) idx += s_card_count;
  idx %= s_card_count;
  s_current = idx;
  // Cancel any in-flight transition — this is a hard jump.
  if (s_anim_timer) { app_timer_cancel(s_anim_timer); s_anim_timer = NULL; }
  s_anim_active = false;
  nav_redraw();
}

static int prv_pos_for_idx(int card_idx) {
  for (int i = 0; i < s_card_count; ++i) {
    if (s_traversal[i] == card_idx) return i;
  }
  return -1;
}

static int prv_step_skip(int from, int dir) {
  // Step from `from` in traversal order `dir` (+1 or -1), skipping
  // disabled cards. Always returns a valid index because at minimum
  // Main and Touch & Go (both permanently enabled) are present.
  if (s_card_count == 0) return from;
  int pos = prv_pos_for_idx(from);
  if (pos < 0) pos = 0;
  for (int i = 0; i < s_card_count; ++i) {
    pos += dir;
    while (pos < 0) pos += s_card_count;
    pos %= s_card_count;
    int idx = s_traversal[pos];
    if (s_enabled[idx]) return idx;
  }
  return from;
}
static int prv_step_no_wrap(int from, int dir) {
  // Like prv_step_skip but never wraps past the array boundary. Returns
  // -1 when there is no enabled card in direction `dir` before the edge,
  // signalling that the user has navigated off the end of the carousel.
  if (s_card_count == 0) return -1;
  int pos = prv_pos_for_idx(from);
  if (pos < 0) pos = 0;
  pos += dir;
  while (pos >= 0 && pos < s_card_count) {
    int idx = s_traversal[pos];
    if (s_enabled[idx]) return idx;
    pos += dir;
  }
  return -1;
}

static void prv_nav_step(int dir) {
  if (s_anim_active) return;
  if (!settings_get_loop_nav()) {
    // Non-looping: stepping past the first/last card exits the app so it
    // can be used as a Quick Launch replacement.
    int dst = prv_step_no_wrap(s_current, dir);
    if (dst < 0) { window_stack_pop_all(true); return; }
    if (dst == s_current) return;
    prv_start_transition(dst, dir);
    return;
  }
  int dst = prv_step_skip(s_current, dir);
  if (dst == s_current) return;
  prv_start_transition(dst, dir);
}

void nav_next(void) {
  prv_nav_step(+1);
}
void nav_prev(void) {
  prv_nav_step(-1);
}

void nav_redraw(void) {
  if (s_card_layer) layer_mark_dirty(s_card_layer);
  if (s_indicator_layer) layer_mark_dirty(s_indicator_layer);
}

void nav_set_enabled(int idx, bool enabled) {
  if (idx < 0 || idx >= s_card_count) return;
  s_enabled[idx] = enabled;
  nav_redraw();
}

bool nav_is_enabled(int idx) {
  if (idx < 0 || idx >= s_card_count) return false;
  return s_enabled[idx];
}

int nav_count_enabled(void) {
  int n = 0;
  for (int i = 0; i < s_card_count; ++i) if (s_enabled[i]) n++;
  return n;
}

int nav_active_enabled_index(void) {
  int n = 0;
  for (int i = 0; i < s_card_count; ++i) {
    int idx = s_traversal[i];
    if (idx == s_current) return n;
    if (s_enabled[idx]) n++;
  }
  return 0;
}

void nav_set_traversal(const int *order, int count) {
  if (s_card_count == 0) return;
  bool seen[NAV_MAX_CARDS] = {0};
  int next_pos = 0;
  if (order && count > 0) {
    for (int i = 0; i < count && next_pos < s_card_count; ++i) {
      int idx = order[i];
      if (idx < 0 || idx >= s_card_count) continue;
      if (seen[idx]) continue;
      s_traversal[next_pos++] = idx;
      seen[idx] = true;
    }
  }
  // Fill any remaining slots in registration order so traversal is
  // always a complete permutation of [0..s_card_count).
  for (int i = 0; i < s_card_count && next_pos < s_card_count; ++i) {
    if (!seen[i]) {
      s_traversal[next_pos++] = i;
      seen[i] = true;
    }
  }
  nav_redraw();
}
