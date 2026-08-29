#include "refresh_sheet.h"
#include "theme.h"
#include "comm.h"
#include "anim.h"
#include "nav.h"
#include "detail_modal.h"
#include "weather_data.h"
// First screen-class dependency in this file (#92). ui_layout.h pulls in ui.h,
// and both are fully guarded to the small classes, so the locked pair sees no
// new code.
#include "ui_layout.h"
#include <stdlib.h>
#include <string.h>

// #92 — the phrase box holds two lines and some phrases need four.
//
// The box is `x 8 .. w-8` of GOTHIC_24_BOLD, which is ~10 characters per line
// on a 144px screen. FALLBACK_PHRASE ("Couldn't reach the forecast just now...",
// 39 chars) needs about four lines and the box has room for two, so it clipped —
// and on chalk the box is a flat rect on a circle, so a second line low on the
// glass was cut at both ends as well. Only a slow or failed refresh reaches this
// text, which is why every happy-path capture looked clean.
//
// 18_BOLD fits three lines in the same height and ~14 characters per line, and
// the box is chord-fitted. The chord is measured at the BOTTOM of a three-line
// block rather than at the box's centre: the box is much taller than the text
// (it runs to the sheet's edge) and the text is top-anchored inside it, so the
// centre of the BOX is nowhere near the text, and on a circle that difference
// is the whole defect. Same reasoning as detail_modal.c's axis labels.
#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
#define RS_PHRASE_FONT ui_font_header()
#define RS_TEXT_DY     22
#define RS_LINE_H      22
#else
#define RS_PHRASE_FONT fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD)
#define RS_TEXT_DY     28
#endif

// Tuning constants ----------------------------------------------------------

// Minimum vertical pull (px from touchdown) to commit to a refresh on
// release. Matches the previous PULLDOWN_THRESHOLD for symmetry.
#define PULL_FULL_THRESHOLD       60

// Below this many px of total movement we don't claim the gesture yet —
// lets horizontal swipes / taps continue to work normally.
#define GESTURE_CLAIM_DEADBAND    8

// Slide animation timing.
#define SLIDE_DURATION_MS         250
#define SLIDE_FRAME_MS            30

// Loading-state phrase rotation.
#define PHRASE_ROTATE_MS          1500

// Safety timeout: if no inbox after this in LOADING, swap to fallback
// phrase, hold briefly, then close.
#define LOADING_TIMEOUT_MS        6000
#define LOADING_FALLBACK_HOLD_MS  1500

// Cold start gets a longer leash than a manual refresh. 6s is generous for a
// refresh on a phone that is already awake and connected; the first fetch of a
// launch is a BLE wake plus three API calls, which routinely runs past it. At
// 6s the sheet would slide away with "Couldn't reach the forecast just now..."
// moments before the data actually landed, which reads as a failure the app
// then contradicts. Timing out here is silent for the same reason.
#define COLD_START_TIMEOUT_MS     10000

// Minimum time the sheet stays open before closing, even on instant data.
#define LOADING_MIN_DISPLAY_MS    900

// Manual-refresh cooldown: within this window of the last successful update,
// a pull/SELECT still opens the sheet (the gesture must never feel dead) but
// shows "Already up to date" instead of firing another 3-call fetch chain.
// After a FAILED fetch last_updated stays old, so retries are never blocked.
#define REFRESH_COOLDOWN_SECS     60

// Phrases ------------------------------------------------------------------

static const char *const s_phrases[] = {
  "Consulting the sky...",
  "Waiting on the latest forecast...",
  "Asking the clouds...",
  "Reading the wind...",
  "Checking the horizon...",
  "Listening for thunder...",
  "Tracking the sun...",
};
#define PHRASE_COUNT ((int)(sizeof(s_phrases) / sizeof(s_phrases[0])))

static const char *const FALLBACK_PHRASE = "Couldn't reach the forecast just now...";

static const char *const UP_TO_DATE_PHRASE = "Already up to date";

// Module state -------------------------------------------------------------

static Layer *s_sheet_layer = NULL;
static RefreshState s_state = REFRESH_IDLE;

// Pull tracking.
static int16_t s_touch_start_x = 0;
static int16_t s_touch_start_y = 0;
static int16_t s_pull_dy = 0;
static bool    s_claimed_gesture = false;

// Sheet vertical position. 0 = fully closed (off-screen above);
// s_sheet_h = fully open. Driven by finger while TRACKING; tweened
// by the slide timer in OPENING/CLOSING.
static int s_sheet_y = 0;
static int s_sheet_h = 0;

// Slide animation.
static AppTimer *s_slide_timer = NULL;
static uint64_t  s_slide_start_ms = 0;
static int       s_slide_from_y = 0;
static int       s_slide_to_y = 0;

// Loading state.
static AppTimer *s_phrase_timer = NULL;
static AppTimer *s_timeout_timer = NULL;
static AppTimer *s_close_holdoff_timer = NULL;
static int       s_phrase_idx = 0;
static bool      s_show_fallback = false;
static bool      s_up_to_date = false;
// This sheet opened itself at launch rather than being asked for. Changes the
// timeout length, silences the failure phrase, and makes the sheet dismissable.
static bool      s_cold_start = false;
static uint64_t  s_loading_start_ms = 0;
static bool      s_data_received_pending = false;

// Helpers ------------------------------------------------------------------

static uint64_t prv_now_ms(void) {
  time_t s; uint16_t ms;
  time_ms(&s, &ms);
  return (uint64_t)s * 1000ULL + (uint64_t)ms;
}

// Within the manual-refresh cooldown of the last real update?
static bool prv_data_is_fresh(void) {
  WeatherData *d = weather_data_get();
  if (!d->valid || d->last_updated == 0) return false;  // mock/no data
  return comm_secs_since(d->last_updated) < REFRESH_COOLDOWN_SECS;
}

static void prv_cancel_timer(AppTimer **t) {
  if (*t) { app_timer_cancel(*t); *t = NULL; }
}

static void prv_apply_layer_frame(void) {
  if (!s_sheet_layer) return;
  // Resize the overlay layer's frame to the currently-visible portion of
  // the sheet. Pebble layers clip drawing to their own bounds, so this
  // gives us free clip-by-frame: any content positioned past s_sheet_y
  // simply isn't drawn. When fully closed (s_sheet_y == 0) we shrink to
  // 0 height so the layer is effectively gone.
  GRect parent = layer_get_bounds(window_get_root_layer(
      layer_get_window(s_sheet_layer)));
  int h = s_sheet_y;
  if (h < 0) h = 0;
  if (h > s_sheet_h) h = s_sheet_h;
  GRect f = GRect(0, 0, parent.size.w, h);
  layer_set_frame(s_sheet_layer, f);
  layer_set_bounds(s_sheet_layer, GRect(0, 0, parent.size.w, h));
}

static void prv_mark_dirty(void) {
  if (!s_sheet_layer) return;
  prv_apply_layer_frame();
  layer_mark_dirty(s_sheet_layer);
}

// Forward decls.
static void prv_slide_tick(void *ctx);
static void prv_phrase_tick(void *ctx);
static void prv_timeout_tick(void *ctx);
static void prv_close_holdoff_tick(void *ctx);
static void prv_start_close(void);
static void prv_enter_loading(void);

// Slide animation ----------------------------------------------------------

static void prv_start_slide(int to_y) {
  s_slide_from_y = s_sheet_y;
  s_slide_to_y = to_y;
  s_slide_start_ms = prv_now_ms();
  prv_cancel_timer(&s_slide_timer);
  s_slide_timer = app_timer_register(SLIDE_FRAME_MS, prv_slide_tick, NULL);
}

static void prv_slide_tick(void *ctx) {
  (void)ctx;
  s_slide_timer = NULL;
  uint64_t now = prv_now_ms();
  uint64_t elapsed = now - s_slide_start_ms;
  if (elapsed >= SLIDE_DURATION_MS) {
    s_sheet_y = s_slide_to_y;
    prv_mark_dirty();
    if (s_state == REFRESH_OPENING) {
      prv_enter_loading();
    } else if (s_state == REFRESH_CLOSING) {
      s_state = REFRESH_IDLE;
      s_up_to_date = false;
      s_cold_start = false;
      prv_cancel_timer(&s_phrase_timer);
      prv_cancel_timer(&s_timeout_timer);
      prv_cancel_timer(&s_close_holdoff_timer);
      prv_mark_dirty();
    }
    return;
  }
  // Ease-out quadratic.
  int32_t p1k = (int32_t)((elapsed * 1000) / SLIDE_DURATION_MS);
  int32_t inv = 1000 - p1k;
  int32_t eased1k = 1000 - (inv * inv) / 1000;
  s_sheet_y = s_slide_from_y +
              ((s_slide_to_y - s_slide_from_y) * eased1k) / 1000;
  prv_mark_dirty();
  s_slide_timer = app_timer_register(SLIDE_FRAME_MS, prv_slide_tick, NULL);
}

// Loading state ------------------------------------------------------------

static void prv_phrase_tick(void *ctx) {
  (void)ctx;
  s_phrase_timer = NULL;
  if (s_state != REFRESH_LOADING) return;
  if (!s_show_fallback) {
    s_phrase_idx = (s_phrase_idx + 1) % PHRASE_COUNT;
    prv_mark_dirty();
    s_phrase_timer = app_timer_register(PHRASE_ROTATE_MS, prv_phrase_tick, NULL);
  }
}

static void prv_timeout_tick(void *ctx) {
  (void)ctx;
  s_timeout_timer = NULL;
  if (s_state != REFRESH_LOADING) return;
  if (s_cold_start) {
    // Nobody asked for this sheet, so it does not get to report a failure.
    // Slide away and leave the card's own NO DATA YET panel to speak.
    prv_start_close();
    return;
  }
  s_show_fallback = true;
  prv_cancel_timer(&s_phrase_timer);
  prv_mark_dirty();
  s_close_holdoff_timer = app_timer_register(LOADING_FALLBACK_HOLD_MS,
                                             prv_close_holdoff_tick, NULL);
}

static void prv_close_holdoff_tick(void *ctx) {
  (void)ctx;
  s_close_holdoff_timer = NULL;
  if (s_state != REFRESH_LOADING) return;
  prv_start_close();
}

static void prv_enter_loading(void) {
  s_state = REFRESH_LOADING;
  s_loading_start_ms = prv_now_ms();
  s_show_fallback = false;
  s_phrase_idx = (int)(rand() % PHRASE_COUNT);
  prv_cancel_timer(&s_phrase_timer);
  prv_cancel_timer(&s_timeout_timer);
  prv_cancel_timer(&s_close_holdoff_timer);
  if (s_up_to_date) {
    // Cooldown hit: no fetch is in flight, so skip the phrase rotation and
    // the 6s timeout — just hold the message briefly, then slide away.
    s_close_holdoff_timer = app_timer_register(LOADING_FALLBACK_HOLD_MS,
                                               prv_close_holdoff_tick, NULL);
    prv_mark_dirty();
    return;
  }
  s_phrase_timer = app_timer_register(PHRASE_ROTATE_MS, prv_phrase_tick, NULL);
  s_timeout_timer = app_timer_register(
      s_cold_start ? COLD_START_TIMEOUT_MS : LOADING_TIMEOUT_MS,
      prv_timeout_tick, NULL);
  prv_mark_dirty();

  // If data already landed before we finished opening, schedule a close
  // after the min-display window.
  if (s_data_received_pending) {
    s_data_received_pending = false;
    uint64_t elapsed = prv_now_ms() - s_loading_start_ms;
    uint32_t wait = (elapsed >= LOADING_MIN_DISPLAY_MS)
                    ? 1U
                    : (uint32_t)(LOADING_MIN_DISPLAY_MS - elapsed);
    prv_cancel_timer(&s_close_holdoff_timer);
    s_close_holdoff_timer = app_timer_register(wait,
                                               prv_close_holdoff_tick, NULL);
  }
}

static void prv_start_close(void) {
  s_state = REFRESH_CLOSING;
  prv_cancel_timer(&s_phrase_timer);
  prv_cancel_timer(&s_timeout_timer);
  prv_cancel_timer(&s_close_holdoff_timer);
  prv_start_slide(0);
}

// Indicator drawing --------------------------------------------------------

// Three-dot orbit indicator. `alpha1k` is 0..1000 (used to scale during
// pull). Rotation comes from `frame` (10 Hz from anim).
static void prv_draw_indicator(GContext *ctx, GPoint center,
                               int r, int dot_r, uint32_t frame,
                               int alpha1k, GColor color) {
  if (alpha1k <= 0) return;
  graphics_context_set_fill_color(ctx, color);
  int rr = (r * alpha1k) / 1000;
  if (rr < 4) rr = 4;
  // ~1 rev / 1.2s @ 10Hz = 12 frames/rev.
  int32_t base = (int32_t)((frame % 12U) * (TRIG_MAX_ANGLE / 12));
  for (int i = 0; i < 3; ++i) {
    int32_t a = base + (TRIG_MAX_ANGLE / 3) * i;
    int32_t cx = center.x + (sin_lookup(a) * rr) / TRIG_MAX_RATIO;
    int32_t cy = center.y - (cos_lookup(a) * rr) / TRIG_MAX_RATIO;
    graphics_fill_circle(ctx, GPoint(cx, cy), dot_r);
  }
}

// Sheet draw proc ----------------------------------------------------------

// Bottom-anchored chevron drawn during TRACKING. Points down below
// threshold, flips up at/past threshold. `cx,cy` = center of the chevron.
// `size` = half-width of the V; the V is `size` wide on each side.
static void prv_draw_chevron(GContext *ctx, int cx, int cy,
                             int size, bool point_up, GColor color) {
  graphics_context_set_stroke_color(ctx, color);
  graphics_context_set_stroke_width(ctx, 3);
  if (point_up) {
    graphics_draw_line(ctx, GPoint(cx - size, cy + size/2),
                            GPoint(cx, cy - size/2));
    graphics_draw_line(ctx, GPoint(cx, cy - size/2),
                            GPoint(cx + size, cy + size/2));
  } else {
    graphics_draw_line(ctx, GPoint(cx - size, cy - size/2),
                            GPoint(cx, cy + size/2));
    graphics_draw_line(ctx, GPoint(cx, cy + size/2),
                            GPoint(cx + size, cy - size/2));
  }
}

static void prv_sheet_update(Layer *layer, GContext *ctx) {
  if (s_sheet_y <= 0) return;

  // The layer frame has already been resized to (w, s_sheet_y) by
  // prv_apply_layer_frame, so layer bounds == visible sheet rect.
  GRect b = layer_get_bounds(layer);

  // Fill the visible portion of the sheet.
  graphics_context_set_fill_color(ctx, theme_bg());
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  // Bottom edge line so the sheet has a visible boundary against the
  // card beneath during slide.
  graphics_context_set_stroke_color(ctx, theme_muted());
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx,
                     GPoint(b.origin.x, b.origin.y + b.size.h - 1),
                     GPoint(b.origin.x + b.size.w - 1,
                            b.origin.y + b.size.h - 1));

  bool past_threshold = (s_pull_dy >= PULL_FULL_THRESHOLD);
  int cx = b.origin.x + b.size.w / 2;
  uint32_t frame = anim_get_frame();

  if (s_state == REFRESH_TRACKING) {
    // Bottom-anchored chevron — rides the leading edge of the sheet so
    // it's always inside the visible region, no matter how small the
    // pull. As the user pulls past threshold, chevron flips up and
    // changes color to signal "ready to release".
    GColor chev_color = past_threshold ? theme_accent_orange()
                                       : theme_secondary();
    int chev_cy = b.origin.y + b.size.h - 14;
    if (chev_cy < b.origin.y + 8) chev_cy = b.origin.y + 8;
    prv_draw_chevron(ctx, cx, chev_cy, 8, past_threshold, chev_color);

    // Hint label only renders when there's enough vertical room above
    // the chevron to fit it without overlapping. ~50px tall sheet is
    // the visual sweet spot for "now we have space for text".
    if (b.size.h >= 50) {
      const char *hint = past_threshold ? "RELEASE TO REFRESH"
                                        : "PULL TO REFRESH";
      int label_y = chev_cy - 24;
      GRect tr = GRect(b.origin.x, label_y, b.size.w, 18);
      graphics_context_set_text_color(ctx, theme_secondary());
      graphics_draw_text(ctx, hint,
                         fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                         tr, GTextOverflowModeTrailingEllipsis,
                         GTextAlignmentCenter, NULL);
    }
    return;
  }

  // OPENING / LOADING / CLOSING — top-anchored full content.
  // Indicator center: ~40% from top of the visible sheet, with floor.
  int cy = b.origin.y + b.size.h * 2 / 5;
  int min_cy = b.origin.y + 32;
  if (cy < min_cy) cy = min_cy;
  GColor ind_color = theme_accent_orange();
  prv_draw_indicator(ctx, GPoint(cx, cy), 14, 4, frame,
                     1000, ind_color);

  // Phrase text only when the sheet is mostly open.
  if (b.size.h >= s_sheet_h * 2 / 3) {
    const char *phrase = s_up_to_date   ? UP_TO_DATE_PHRASE
                        : s_show_fallback ? FALLBACK_PHRASE
                                          : s_phrases[s_phrase_idx];
    int text_y = cy + RS_TEXT_DY;
    int text_h = b.size.h - (text_y - b.origin.y) - 8;
    if (text_h < 24) text_h = 24;
#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
    int tw = ui_band_w(b, text_y + 3 * RS_LINE_H - 1, 1);
    GRect tr = GRect(b.origin.x + (b.size.w - tw) / 2, text_y, tw, text_h);
#else
    GRect tr = GRect(b.origin.x + 8, text_y,
                     b.size.w - 16, text_h);
#endif
    graphics_context_set_text_color(ctx, theme_fg());
    graphics_draw_text(ctx, phrase,
                       RS_PHRASE_FONT,
                       tr, GTextOverflowModeWordWrap,
                       GTextAlignmentCenter, NULL);
  }
}

// Public API ---------------------------------------------------------------

void refresh_sheet_init(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect rb = layer_get_bounds(root);
  s_sheet_h = rb.size.h;
  // Start with a zero-height frame; we resize it as the sheet grows.
  s_sheet_layer = layer_create(GRect(0, 0, rb.size.w, 0));
  layer_set_clips(s_sheet_layer, true);
  layer_set_update_proc(s_sheet_layer, prv_sheet_update);
  layer_add_child(root, s_sheet_layer);

  s_state = REFRESH_IDLE;
  s_sheet_y = 0;
  s_pull_dy = 0;
  s_claimed_gesture = false;
  s_up_to_date = false;
}

void refresh_sheet_deinit(void) {
  prv_cancel_timer(&s_slide_timer);
  prv_cancel_timer(&s_phrase_timer);
  prv_cancel_timer(&s_timeout_timer);
  prv_cancel_timer(&s_close_holdoff_timer);
  s_state = REFRESH_IDLE;
  if (s_sheet_layer) {
    layer_destroy(s_sheet_layer);
    s_sheet_layer = NULL;
  }
}

bool refresh_sheet_is_active(void) {
  return s_state != REFRESH_IDLE;
}

static void prv_show(bool cold) {
  // One overlay at a time: ignore if a sheet or detail modal is up.
  if (s_state != REFRESH_IDLE) return;
  if (detail_modal_is_active()) return;
  // Enter the same OPENING → LOADING path a committed pull uses, minus the
  // touch tracking: start closed (y=0) and slide fully open. This gives
  // button-only platforms (and button users on touch models) identical
  // spinner/phrase feedback.
  s_state = REFRESH_OPENING;
  s_cold_start = cold;
  s_data_received_pending = false;
  s_claimed_gesture = false;
  s_pull_dy = 0;
  s_sheet_y = 0;
  anim_kick();  // ensure the 10 Hz ticker runs so the spinner rotates
  // A cold start has no reading at all, so the freshness cooldown cannot
  // apply, and comm_init()'s launch request is already in flight — this sheet
  // watches that fetch rather than starting a second one.
  s_up_to_date = cold ? false : prv_data_is_fresh();
  if (!cold && !s_up_to_date) comm_request_refresh();
  if (cold) {
    // No slide-down. The point of the cold-start sheet is to REPLACE the
    // NO DATA YET panel, and a 250ms open animation shows that panel through
    // the gap under the sheet's leading edge for the whole slide. Opened from
    // prv_window_load this puts the sheet fully open in the window's FIRST
    // paint, so the panel is never on screen at all — the only motion is the
    // slide-up when the forecast lands.
    s_sheet_y = s_sheet_h;
    prv_mark_dirty();
    prv_enter_loading();
    return;
  }
  prv_start_slide(s_sheet_h);
}

void refresh_sheet_show_programmatic(void) { prv_show(false); }

void refresh_sheet_show_cold_start(void) { prv_show(true); }

bool refresh_sheet_consume_input(void) {
  if (s_state == REFRESH_IDLE) return false;
  // A sheet the user never asked for must never be a wall: the first press or
  // tap sends it away. It is still consumed — the press dismisses, it does not
  // also page a card — which is the same bargain any dismissable overlay makes.
  if (s_cold_start && s_state != REFRESH_CLOSING) prv_start_close();
  return true;
}

bool refresh_sheet_on_touchdown(int16_t x, int16_t y) {
  // Lock out any new gesture while non-idle.
  if (s_state != REFRESH_IDLE) {
    return true;  // consume the event so caller skips swipe/tap logic
  }
  s_touch_start_x = x;
  s_touch_start_y = y;
  s_pull_dy = 0;
  s_claimed_gesture = false;
  return false;  // not yet claimed; let other handlers see liftoff/tap
}

bool refresh_sheet_on_move(int16_t x, int16_t y) {
  if (s_state == REFRESH_LOADING ||
      s_state == REFRESH_OPENING ||
      s_state == REFRESH_CLOSING) {
    return true;  // swallow movement while sheet is non-tracking-active
  }

  int16_t dx = x - s_touch_start_x;
  int16_t dy = y - s_touch_start_y;
  int16_t adx = dx < 0 ? -dx : dx;
  int16_t ady = dy < 0 ? -dy : dy;

  if (!s_claimed_gesture) {
    // Wait until movement clears the deadband, then decide.
    if (adx < GESTURE_CLAIM_DEADBAND && ady < GESTURE_CLAIM_DEADBAND) {
      return false;
    }
    // Claim only if downward and dominant-vertical.
    if (dy > 0 && ady > adx) {
      s_claimed_gesture = true;
      s_state = REFRESH_TRACKING;
    } else {
      // Not a pull-down. Bail; let the caller's swipe/tap path own it.
      return false;
    }
  }

  // Tracking: rubber-band sheet to finger.
  s_pull_dy = dy > 0 ? dy : 0;
  // Cap visual extension to 1.5x threshold for a slight resistance feel.
  int max_y = (PULL_FULL_THRESHOLD * 3) / 2;
  int target = s_pull_dy;
  if (target > max_y) target = max_y;
  // Map pull dy directly to sheet_y, but never below 0.
  s_sheet_y = target;
  if (s_sheet_y > s_sheet_h) s_sheet_y = s_sheet_h;
  prv_mark_dirty();
  return true;
}

bool refresh_sheet_on_liftoff(int16_t x, int16_t y) {
  (void)x; (void)y;
  if (s_state == REFRESH_LOADING ||
      s_state == REFRESH_OPENING ||
      s_state == REFRESH_CLOSING) {
    // Already in a non-tracking active phase: swallow — and dismiss it if
    // this is the cold-start sheet, so tap-to-dismiss matches the buttons.
    return refresh_sheet_consume_input();
  }
  if (!s_claimed_gesture || s_state != REFRESH_TRACKING) {
    // Never claimed — let caller do its swipe/tap thing.
    s_claimed_gesture = false;
    return false;
  }

  // Decide commit vs cancel.
  if (s_pull_dy >= PULL_FULL_THRESHOLD) {
    s_state = REFRESH_OPENING;
    s_data_received_pending = false;
    s_up_to_date = prv_data_is_fresh();
    if (!s_up_to_date) comm_request_refresh();
    prv_start_slide(s_sheet_h);
  } else {
    // Cancel — slide back up.
    s_state = REFRESH_CLOSING;
    prv_start_slide(0);
  }
  s_claimed_gesture = false;
  s_pull_dy = 0;
  return true;
}

void refresh_sheet_on_data_received(void) {
  if (s_state != REFRESH_LOADING && s_state != REFRESH_OPENING) {
    return;
  }
  if (s_state == REFRESH_OPENING) {
    // Data landed before we finished opening; remember and let
    // prv_enter_loading handle it.
    s_data_received_pending = true;
    return;
  }
  // LOADING: schedule close after min-display window.
  prv_cancel_timer(&s_close_holdoff_timer);
  uint64_t elapsed = prv_now_ms() - s_loading_start_ms;
  uint32_t wait = (elapsed >= LOADING_MIN_DISPLAY_MS)
                  ? 1U
                  : (uint32_t)(LOADING_MIN_DISPLAY_MS - elapsed);
  s_close_holdoff_timer = app_timer_register(wait,
                                             prv_close_holdoff_tick, NULL);
}