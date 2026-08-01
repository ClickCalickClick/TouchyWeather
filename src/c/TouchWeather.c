#include <pebble.h>
#include <string.h>
#include "theme.h"
#include "nav.h"
#include "weather_data.h"
#include "comm.h"
#include "anim.h"
#include "settings.h"
#include "ui.h"
#include "refresh_sheet.h"
#include "detail_modal.h"
#include "update_notes.h"
#include "glance.h"
#include "cards/cards.h"

// Card-registry indices for the toggleable cards. Must match the
// nav_register order below — kept in one place so it's obvious what
// shifts if cards are reordered.
#define IDX_ADVICE 1
#define IDX_HOURS  2
#define IDX_WEEK   3
#define IDX_PRECIP 4
#define IDX_UV     5
#define IDX_AQ     6
#define IDX_SUN    7
#define IDX_NIGHT  8
#define IDX_GOLDEN 9
#define IDX_RADAR  10
#define IDX_SETTINGS 11

static const int s_toggle_to_card_idx[SETTINGS_TOGGLEABLE_COUNT] = {
  IDX_HOURS, IDX_WEEK, IDX_PRECIP, IDX_UV, IDX_AQ, IDX_SUN, IDX_NIGHT, IDX_GOLDEN, IDX_RADAR,
  IDX_ADVICE,
};

static void prv_apply_card_visibility(void) {
  for (int i = 0; i < SETTINGS_TOGGLEABLE_COUNT; ++i) {
    // Effective visibility = user enabled AND not auto-hidden (Phase 3.2).
    nav_set_enabled(s_toggle_to_card_idx[i],
                    settings_get_effective_enabled((ToggleId)i));
  }
  // Settings-card opt-out: dropped from traversal + page dots exactly like a
  // disabled card (the radar carve-out pattern — nav_set_traversal always
  // back-fills omitted slots, so disabling is the mechanism, not omission).
  // settings_get_settings_card_hidden() is the gated value: it can only be
  // true while PhoneManagesCards is on, so Clay can always bring it back.
  nav_set_enabled(IDX_SETTINGS, !settings_get_settings_card_hidden());
}

// Builds nav's traversal order from the user's Settings visual order:
//   slot 0           = Main (always first)
//   slots 1..10      = toggleable cards in current visual order
//   slot 11          = Settings (always last)
static void prv_sync_nav_traversal(void) {
  int order[NAV_MAX_CARDS];
  int n = 0;
  order[n++] = 0;  // Main
  for (int i = 0; i < SETTINGS_TOGGLEABLE_COUNT; ++i) {
    ToggleId tid = settings_visual_id(i);
    if ((int)tid >= SETTINGS_TOGGLEABLE_COUNT) continue;
    order[n++] = s_toggle_to_card_idx[tid];
  }
  order[n++] = IDX_SETTINGS;  // Settings (dropped via nav_set_enabled when hidden)
  nav_set_traversal(order, n);
}

// Clay changed per-card visibility and/or the visual order (PhoneManagesCards
// on): re-apply enable flags to nav and rebuild the traversal in one go.
static void prv_cards_changed_from_phone(void) {
  prv_apply_card_visibility();
  prv_sync_nav_traversal();
}

// Touch is plumbed for emery / gabbro hardware. Requires firmware >= 5.92.
// Flip ENABLE_TOUCH to 0 if running against an older simulator that
// doesn't ship the touch_service API.
#define ENABLE_TOUCH 1

static Window *s_window;

#if ENABLE_TOUCH && defined(PBL_TOUCH)
static bool s_tracking = false;
static int16_t s_start_x = 0;
static int16_t s_start_y = 0;

// Defined below; the touch handler's swipe-up shortcut needs it early.
static DetailType prv_detail_for_current(void);

static void touch_handler(const TouchEvent *event, void *context) {
  (void)context;
  switch (event->type) {
    case TouchEvent_Touchdown:
      anim_kick();  // touch activity: resume/extend decorative animation
      s_tracking = true;
      s_start_x = event->x;
      s_start_y = event->y;
      // Detail modal owns all touch while open (drag-down to dismiss handled
      // on liftoff); don't let the refresh sheet start tracking underneath it.
      if (detail_modal_is_active()) break;
      // The What's New modal sits on top, so it gets first crack: a drag
      // there scrolls the notes and consumes the gesture.
      if (update_notes_on_touchdown(event->x, event->y)) break;
      // Give the refresh sheet first crack at the gesture. If it claims
      // it (e.g. sheet is already open), we still record start coords
      // for completeness but later events get routed to the sheet first.
      refresh_sheet_on_touchdown(event->x, event->y);
      break;
    case TouchEvent_PositionUpdate:
      if (!s_tracking) break;
      if (update_notes_on_move(event->x, event->y)) break;
      // Refresh sheet handles pull-down tracking. If it consumes the
      // event we stop here; otherwise nothing else needs to react to
      // mid-gesture moves today.
      refresh_sheet_on_move(event->x, event->y);
      break;
    case TouchEvent_Liftoff: {
      if (!s_tracking) break;
      // Detail modal: horizontal swipes page (Week detail), a downward
      // flick dismisses; any other touch is swallowed so card nav stays
      // locked while it's open.
      if (detail_modal_is_active()) {
        int16_t mdx = event->x - s_start_x;
        int16_t mdy = event->y - s_start_y;
        int16_t madx = mdx < 0 ? -mdx : mdx;
        int16_t mady = mdy < 0 ? -mdy : mdy;
        if (madx > 30 && madx > mady) {
          anim_kick();
          if (mdx < 0) detail_modal_handle_down();  // swipe left = next page
          else         detail_modal_handle_up();    // swipe right = previous
        } else if (mdy > 30 && mady > madx) {
          detail_modal_close();
        }
        s_tracking = false;
        break;
      }
      if (update_notes_on_liftoff(event->x, event->y)) {
        s_tracking = false;
        break;
      }
      // Sheet first — if it consumes the liftoff (committed pull or
      // tracking-cancel), skip the swipe/tap fallthrough entirely.
      if (refresh_sheet_on_liftoff(event->x, event->y)) {
        s_tracking = false;
        break;
      }
      // Don't act on buttons/swipes/taps while the sheet is animating
      // or loading.
      if (refresh_sheet_is_active()) {
        s_tracking = false;
        break;
      }
      int16_t dx = event->x - s_start_x;
      int16_t dy = event->y - s_start_y;
      int16_t adx = dx < 0 ? -dx : dx;
      int16_t ady = dy < 0 ? -dy : dy;
      const int16_t HSWIPE_THRESHOLD = 30;
      const int16_t VSWIPE_THRESHOLD = 30;
      const int16_t TAP_THRESHOLD = 15;
      if (adx > HSWIPE_THRESHOLD && adx > ady) {
        // Horizontal swipe = card nav.
        if (dx < 0) nav_next();
        else        nav_prev();
      } else if (dy < 0 && ady > VSWIPE_THRESHOLD && ady > adx) {
        // Swipe up = open the current card's detail modal (touch shortcut for
        // SELECT-long; drag-down still dismisses). Pull-DOWN is owned by the
        // refresh sheet and handled above, so only an upward flick lands here.
        // No-op on cards without a modal. (Phase 4, requested on touch models.)
        DetailType dt = prv_detail_for_current();
        if (dt != DETAIL_NONE) {
          anim_kick();
          detail_modal_open(dt);
        }
      } else if (adx < TAP_THRESHOLD && ady < TAP_THRESHOLD) {
        // Tap (small movement). On the Settings card, advance the
        // row cursor. Elsewhere we ignore taps for now.
        if (strcmp(nav_current_name(), "Settings") == 0) {
          settings_cursor_advance();
          nav_redraw();
        }
      }
      // Note: pull-down-to-refresh is now owned by refresh_sheet.
      s_tracking = false;
      break;
    }
    default:
      break;
  }
}
#endif

// Phase 4: which detail modal (if any) the current card opens on SELECT-long.
static DetailType prv_detail_for_current(void) {
  const char *n = nav_current_name();
  if (strcmp(n, "6 Hours") == 0) return DETAIL_HOURS;
  if (strcmp(n, "Precipitation") == 0) return DETAIL_PRECIP;
  if (strcmp(n, "Week Ahead") == 0) return DETAIL_WEEK;
  if (strcmp(n, "UV") == 0) return DETAIL_UV;
  if (strcmp(n, "Air Quality") == 0) return DETAIL_AQ;
  return DETAIL_NONE;
}

static void prv_select_click(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  anim_kick();  // user activity: resume/extend decorative animation
  // While a detail modal is open, SELECT toggles its secondary overlay.
  if (detail_modal_is_active()) { detail_modal_handle_select(); return; }
  if (refresh_sheet_is_active()) return;
  // Context-aware short-press:
  //   Radar    → retry fetch (bypasses 60s cooldown)
  //   Settings → toggle the highlighted row
  //   Elsewhere → toggle Light/Dark theme
  if (strcmp(nav_current_name(), "Radar") == 0) {
    card_radar_force_refresh();
    nav_redraw();
    return;
  }
  if (strcmp(nav_current_name(), "Settings") == 0) {
#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
    // D1 status card: SELECT is the theme escape hatch. Deliberately NOT
    // gated by SelectTogglesTheme, so the watch always keeps one local
    // control; the card's THEME row repaints as its own feedback.
    theme_set(theme_get() == THEME_LIGHT ? THEME_DARK : THEME_LIGHT);
    nav_redraw();
    return;
#else
    int cur = settings_cursor();
    ToggleId tid = SETTINGS_VIS_ID(cur);
    bool now = !settings_get_enabled(tid);
    settings_set_enabled(tid, now);
    nav_set_enabled(s_toggle_to_card_idx[tid], now);
    nav_redraw();
    comm_send_card_state();  // keep the phone's Clay seed in sync
    return;
#endif
  }
  // Main card: manual weather refresh (Task 5.1). This is the only manual
  // refresh path button-only platforms have; on touch models it complements
  // pull-to-refresh. Unconditional — the freed gesture's assigned future per
  // the gesture budget.
  if (strcmp(nav_current_name(), "Main") == 0) {
    refresh_sheet_show_programmatic();
    return;
  }
  // Ordinary cards: toggle theme only if the user hasn't disabled it
  // (SelectTogglesTheme, Task 1.2). When off this is a no-op; Phase 4 will
  // claim SELECT-short for in-modal overlay toggling.
  if (settings_get_select_toggles_theme()) {
    theme_set(theme_get() == THEME_LIGHT ? THEME_DARK : THEME_LIGHT);
    nav_redraw();
  }
}

static void prv_select_long(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  anim_kick();
  if (detail_modal_is_active()) return;  // no re-trigger while modal is open
  if (refresh_sheet_is_active()) return;
  if (strcmp(nav_current_name(), "Settings") == 0) return;
  // Phase 4: on forecast cards that have a detail view, SELECT-long opens the
  // bottom-sheet detail modal (claims the gesture unconditionally there).
  DetailType dt = prv_detail_for_current();
  if (dt != DETAIL_NONE) {
    detail_modal_open(dt);
    return;
  }
  // Elsewhere, SELECT-long still toggles theme (gated by SelectTogglesTheme).
  if (settings_get_select_toggles_theme()) {
    theme_set(theme_get() == THEME_LIGHT ? THEME_DARK : THEME_LIGHT);
    nav_redraw();
  }
}

static void prv_up_click(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  anim_kick();
  if (detail_modal_is_active()) { detail_modal_handle_up(); return; }
  if (refresh_sheet_is_active()) return;
  nav_prev();
}

static void prv_down_click(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  anim_kick();
  if (detail_modal_is_active()) { detail_modal_handle_down(); return; }
  if (refresh_sheet_is_active()) return;
  nav_next();
}

// Long-press UP / DOWN on the Settings card reorders the highlighted
// toggleable row. On every other card these are no-ops (short-click
// already handled the nav action on press-down).
static void prv_up_long(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (detail_modal_is_active()) return;
  if (refresh_sheet_is_active()) return;
#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
  // D1: on-watch reorder compiled out — Clay owns card order on these classes.
#else
  if (strcmp(nav_current_name(), "Settings") != 0) return;
  if (settings_move_up(settings_cursor())) {
    prv_sync_nav_traversal();
    nav_redraw();
    comm_send_card_state();  // debounced, so chained holds coalesce
  }
#endif
}

static void prv_down_long(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  if (detail_modal_is_active()) return;
  if (refresh_sheet_is_active()) return;
#if defined(UI_SCREEN_SMALL_RECT) || defined(UI_SCREEN_SMALL_ROUND)
  // D1: on-watch reorder compiled out — Clay owns card order on these classes.
#else
  if (strcmp(nav_current_name(), "Settings") != 0) return;
  if (settings_move_down(settings_cursor())) {
    prv_sync_nav_traversal();
    nav_redraw();
    comm_send_card_state();  // debounced, so chained holds coalesce
  }
#endif
}

// BACK: dismiss an open detail modal; otherwise exit the app. Single-click on
// BACK IS delivered on this SDK (unlike long/raw — see DECISIONS.md 2.1) and
// fires on release with no added latency, so exit timing is preserved.
// Subscribing it overrides the firmware default, so exit is re-implemented here.
static void prv_back_click(ClickRecognizerRef r, void *ctx) {
  (void)r; (void)ctx;
  anim_kick();
  if (detail_modal_handle_back()) return;  // modal open → dismiss, don't exit
  window_stack_pop_all(true);
}

static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click);
  window_long_click_subscribe(BUTTON_ID_SELECT, 600, prv_select_long, NULL);
  window_single_click_subscribe(BUTTON_ID_UP, prv_up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_down_click);
  window_long_click_subscribe(BUTTON_ID_UP, 500, prv_up_long, NULL);
  window_long_click_subscribe(BUTTON_ID_DOWN, 500, prv_down_long, NULL);
  window_single_click_subscribe(BUTTON_ID_BACK, prv_back_click);
}

static void prv_window_load(Window *window) {
  theme_apply_to_window(window);
  nav_init(window);

  nav_register("Main", card_main_draw);
  nav_register("Touch & Go", card_advice_draw);
  nav_register("6 Hours", card_hours_draw);
  nav_register("Week Ahead", card_week_draw);
  nav_register("Precipitation", card_precipitation_draw);
  nav_register("UV", card_uv_draw);
  nav_register("Air Quality", card_air_quality_draw);
  nav_register("Sun Cycle", card_sun_cycle_draw);
  nav_register("Night Sky", card_night_sky_draw);
  nav_register("Golden Hour", card_golden_hour_draw);
  nav_register("Radar", card_radar_draw);
  nav_register("Settings", card_settings_draw);
  prv_apply_card_visibility();
  prv_sync_nav_traversal();
  nav_show_index(0);

  // Pull-to-refresh sheet sits above the nav layers so it can paint
  // over any card content while open.
  refresh_sheet_init(window);
  // Detail modal (Phase 4) sits above everything else.
  detail_modal_init(window);

#if ENABLE_TOUCH && defined(PBL_TOUCH)
  if (touch_service_is_enabled()) {
    touch_service_subscribe(touch_handler, NULL);
  }
#endif
}

static void prv_window_unload(Window *window) {
  (void)window;
#if ENABLE_TOUCH && defined(PBL_TOUCH)
  touch_service_unsubscribe();
#endif
  detail_modal_deinit();
  refresh_sheet_deinit();
  nav_deinit();
}

static void prv_init(void) {
  theme_init();
  settings_load();
  weather_data_init_mock();

  // Load cached data BEFORE first window draw to prevent units flash.
  // The callback must be set first so comm_load_cache() can trigger a redraw.
  comm_set_update_callback(nav_redraw);
  // Re-apply enable flags AND rebuild the traversal when Clay changes card
  // visibility or order (the traversal rebuild is cheap, so one combined
  // callback covers both kinds of change).
  comm_set_visibility_callback(prv_cards_changed_from_phone);
  comm_load_cache();

  s_window = window_create();
  window_set_click_config_provider(s_window, prv_click_config_provider);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  window_stack_push(s_window, true);

  // One-time "What's New" modal on top of the main window after an update.
  update_notes_maybe_show();

  comm_init();
  anim_init();
}

static void prv_deinit(void) {
  anim_deinit();
  comm_deinit();
  update_notes_deinit();
  window_destroy(s_window);
  // Publish the launcher glance on the way out so the launcher subtitle
  // shows the freshest temp/condition we have.
  glance_update();
}

int main(void) {
  // Check if this is a background wakeup
  if (launch_reason() == APP_LAUNCH_WAKEUP) {
    // Background fetch mode - no UI, just fetch and exit
    // APP_LOG hand-expanded with the line argument pinned: __LINE__ here is
    // baked into EVERY platform's binary, so an edit anywhere above main()
    // would shift it and break the emery/gabbro byte lock (tools/lock_guard.py)
    // even when nothing real changed. Same for the exit log below.
    app_log(APP_LOG_LEVEL_INFO, __FILE_NAME__, 413, "Launched from wakeup");

    // Initialize weather data structure (required for cache writes)
    weather_data_init_mock();

    // Load settings to check interval
    settings_load();

    // Load the real cached weather before any background payload arrives.
    // Otherwise prv_save_cache() would persist the mock struct back over the
    // real cache for every field the background fetch doesn't carry (pollen,
    // sunrise, high/low, etc.). Safe headless: comm_load_cache() only fires
    // the redraw callback when one is registered (none is here).
    comm_load_cache();

    // Init background fetch (minimal init, no UI)
    comm_background_init();

    // CRITICAL: Must run event loop for AppMessage callbacks to fire.
    // The OS will kill us after ~30 seconds, or we'll exit when data
    // arrives or timeout fires (both call app_event_loop_exit()).
    app_event_loop();

    app_log(APP_LOG_LEVEL_INFO, __FILE_NAME__, 436, "BG: Event loop exited");
    return 0;
  }

  // Normal launch - full UI mode
  prv_init();
  app_event_loop();
  prv_deinit();
}
