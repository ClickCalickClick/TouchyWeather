#include "anim.h"
#include "nav.h"
#include "weather_data.h"
#include "refresh_sheet.h"
#include "settings.h"
#include <string.h>

#define ANIM_PERIOD_MS 100

// Decorative animation (hero icon, rotating banners, settings footer hint)
// freezes this many frames (~8 s at 10 Hz) after the last activity, so the
// continuous 10 Hz redraw doesn't drain the battery when the app is left
// foregrounded. Tunable 50-100 (5-10 s). The refresh-sheet spinner is exempt:
// while the sheet is active the ticker keeps running regardless.
#define ANIM_TIMEOUT_FRAMES 80

static AppTimer *s_timer = NULL;
static uint32_t s_frame = 0;
// Decorative animation is allowed while s_frame < s_deadline_frame. anim_kick()
// pushes this forward on activity; when the deadline passes (and no sheet is
// active) the ticker stops entirely and re-arms on the next anim_kick().
static uint32_t s_deadline_frame = 0;

static void prv_tick(void *ctx);

// Decorative animation runs only when the setting is on AND we're inside the
// post-activity window. The refresh-sheet spinner is handled separately.
static bool prv_decorative_active(void) {
  if (!settings_get_animations_enabled()) return false;
  return s_frame < s_deadline_frame;
}

static void prv_ensure_running(void) {
  if (!s_timer) {
    s_timer = app_timer_register(ANIM_PERIOD_MS, prv_tick, NULL);
  }
}

static void prv_tick(void *ctx) {
  (void)ctx;
  s_timer = NULL;  // the timer that just fired is spent
  s_frame++;

  bool decorative = prv_decorative_active();
  bool sheet = refresh_sheet_is_active();

  bool needs_redraw = false;
  if (decorative) {
    // Main card (index 0) has the animated hero icon; redraw every tick.
    int idx = nav_current_index();
    needs_redraw = (idx == 0);
    if (!needs_redraw) {
      WeatherData *d = weather_data_get();
      bool is_sun_cycle = (strcmp(nav_current_name(), "Sun Cycle") == 0);
      // Rain/updated status banner rotates ~every 4 s on most cards.
      if (d->rain_alert_min >= 0 && !is_sun_cycle) {
        if ((s_frame % 40) == 0) needs_redraw = true;
      }
    }
    // Settings card has a rotating footer hint; refresh every ~2.5 s.
    if (!needs_redraw && strcmp(nav_current_name(), "Settings") == 0) {
      if ((s_frame % 25) == 0) needs_redraw = true;
    }
  }
  // Refresh sheet has its own animated indicator; keep ticking while it's
  // open so the spinner advances on every card (its rotation reads s_frame).
  if (sheet) needs_redraw = true;

  if (needs_redraw) {
    nav_redraw();
  }

  // Keep the timer alive only while something still needs it. Once decorative
  // animation has frozen and no sheet is animating, stop entirely (zero idle
  // cost); anim_kick() re-arms on the next activity.
  if (decorative || sheet) {
    s_timer = app_timer_register(ANIM_PERIOD_MS, prv_tick, NULL);
  }
}

void anim_init(void) {
  s_frame = 0;
  // Animate through the first post-launch window, then settle.
  s_deadline_frame = ANIM_TIMEOUT_FRAMES;
  s_timer = app_timer_register(ANIM_PERIOD_MS, prv_tick, NULL);
}

void anim_deinit(void) {
  if (s_timer) {
    app_timer_cancel(s_timer);
    s_timer = NULL;
  }
}

void anim_kick(void) {
  // Reset the decorative idle deadline and make sure the ticker is running.
  // Safe to call even when animations are disabled or a sheet is open: the
  // next tick re-evaluates and stops the ticker again if nothing needs it.
  s_deadline_frame = s_frame + ANIM_TIMEOUT_FRAMES;
  prv_ensure_running();
}

uint32_t anim_get_frame(void) { return s_frame; }
