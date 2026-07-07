#include "glance.h"
#include "weather_data.h"
#include <pebble.h>
#include <stdio.h>

// How long a published slice stays up before the launcher falls back to the
// plain app name. Users with background updates (30/60 min wakeups) keep the
// glance perpetually fresh; with them off, a 3h-old temp quietly retires
// instead of lingering stale.
#define GLANCE_TTL_SECS (3 * 60 * 60)

// Indexed by WeatherCondition (weather_data.h). Conditions are icon-only
// everywhere else in the app; the glance is text, so it needs this table.
static const char *const s_cond_labels[] = {
  "Sunny", "Partly Cloudy", "Cloudy", "Rain", "Snow", "Storm", "Fog",
};
#define COND_LABEL_COUNT ((int)(sizeof(s_cond_labels) / sizeof(s_cond_labels[0])))

static char s_subtitle[32];
static time_t s_expiration;

static void prv_reload(AppGlanceReloadSession *session, size_t limit,
                       void *context) {
  (void)context;
  if (limit < 1) return;
  const AppGlanceSlice slice = {
    .layout = {
      .icon = APP_GLANCE_SLICE_DEFAULT_ICON,
      .subtitle_template_string = s_subtitle,
    },
    .expiration_time = s_expiration,
  };
  AppGlanceResult r = app_glance_add_slice(session, slice);
  if (r != APP_GLANCE_RESULT_SUCCESS) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "Glance: add_slice failed (0x%x)", (int)r);
  }
}

void glance_update(void) {
  WeatherData *d = weather_data_get();
  // last_updated == 0 means mock or never-fetched data (mock init sets
  // valid=true, so valid alone would publish the fake 72°). Bail WITHOUT
  // reloading so a previous real slice — and its own expiration — survives.
  if (!d->valid || d->last_updated == 0) return;
  s_expiration = (time_t)d->last_updated + GLANCE_TTL_SECS;
  if (s_expiration <= time(NULL) + 60) return;  // would expire immediately

  int cond = (int)d->condition;
  const char *label = (cond >= 0 && cond < COND_LABEL_COUNT)
                      ? s_cond_labels[cond] : "";
  snprintf(s_subtitle, sizeof(s_subtitle), "%d° %s", d->temp, label);
  app_glance_reload(prv_reload, NULL);
}
