#include "weather_data.h"
#include <string.h>

static WeatherData s_data;

#ifdef TW_MOCK_DATA
// Hardcoded sample forecast, for LAYOUT WORK ONLY — it lets a card be tuned on
// the emulator without waiting on PKJS. Never compiled into a shipped build:
// TW_MOCK_DATA is defined nowhere, so this whole function emits no code and
// cannot move the locked emery/gabbro binaries (tools/lock_guard.py).
//
// It used to run unconditionally at startup with valid = true, which is what
// put an invented imperial forecast on screen for every user whose cache did
// not load. If you enable it, remember that prv_save_cache() persists anything
// flagged valid — a mock run will write this fiction into the real cache.
static void prv_init_mock(void) {
  s_data.temp = 72;
  s_data.feels_like = 75;
  s_data.high = 5;   // shown as ↑5° relative-ish per render
  s_data.low = 2;    // shown as ↓2°
  s_data.condition = COND_SUNNY;
  s_data.wind_speed = 12;
  strncpy(s_data.wind_dir, "NW", sizeof(s_data.wind_dir));
  s_data.humidity = 15;
  s_data.dew_point = 52;
  s_data.use_dew_point = false;
  s_data.show_location = false;
  s_data.precip[0] = 25;
  s_data.precip[1] = 90;
  s_data.precip[2] = 65;
  s_data.precip[3] = 45;
  s_data.precip[4] = 20;
  s_data.uv = 4;
  s_data.uv_max = 7;
  s_data.aqi = 42;
  // Phase 4 UV modal: hourly UV curve (+1h..+6h), a plausible afternoon
  // decline toward evening.
  s_data.hours_uv[0] = 6; s_data.hours_uv[1] = 5; s_data.hours_uv[2] = 4;
  s_data.hours_uv[3] = 3; s_data.hours_uv[4] = 1; s_data.hours_uv[5] = 0;
  // Phase 4 AIR DETAIL: pollutant concentrations (µg/m³).
  s_data.pm2_5 = 12;
  s_data.pm10  = 24;
  s_data.o3    = 68;
  s_data.no2   = 18;
  strncpy(s_data.sunrise, "6:14 AM", sizeof(s_data.sunrise));
  strncpy(s_data.sunset, "7:45 PM", sizeof(s_data.sunset));
  strncpy(s_data.location_name, "San Francisco", sizeof(s_data.location_name));
  s_data.rain_alert_min = 15;
  s_data.units = UNITS_IMPERIAL;
  s_data.last_updated = 0;
  s_data.valid = true;

  // Phase 10A: Next 6 Hours mock (extends the original 4-hour set with
  // two more hours so we can verify both rect + round layouts before
  // PKJS sends real data).
  strncpy(s_data.hours_label[0], "2 PM", sizeof(s_data.hours_label[0]));
  strncpy(s_data.hours_label[1], "3 PM", sizeof(s_data.hours_label[1]));
  strncpy(s_data.hours_label[2], "4 PM", sizeof(s_data.hours_label[2]));
  strncpy(s_data.hours_label[3], "5 PM", sizeof(s_data.hours_label[3]));
  strncpy(s_data.hours_label[4], "6 PM", sizeof(s_data.hours_label[4]));
  strncpy(s_data.hours_label[5], "7 PM", sizeof(s_data.hours_label[5]));
  s_data.hours_temp[0] = 74; s_data.hours_temp[1] = 75;
  s_data.hours_temp[2] = 73; s_data.hours_temp[3] = 68;
  s_data.hours_temp[4] = 66; s_data.hours_temp[5] = 63;
  s_data.hours_cond[0] = COND_SUNNY;
  s_data.hours_cond[1] = COND_SUNNY;
  s_data.hours_cond[2] = COND_CLOUDY;
  s_data.hours_cond[3] = COND_RAIN;
  s_data.hours_cond[4] = COND_RAIN;
  s_data.hours_cond[5] = COND_PARTLY_CLOUDY;
  // Pop is shown only when >= 30%. Mock includes a few rainy hours.
  s_data.hours_pop[0] = 5;  s_data.hours_pop[1] = 10;
  s_data.hours_pop[2] = 35; s_data.hours_pop[3] = 80;
  s_data.hours_pop[4] = 60; s_data.hours_pop[5] = 20;
  // Wind (mph) for dry hours; direction varies to exercise the arrow.
  s_data.hours_wind[0] = 8;  s_data.hours_wind[1] = 12;
  s_data.hours_wind[2] = 15; s_data.hours_wind[3] = 10;
  s_data.hours_wind[4] = 6;  s_data.hours_wind[5] = 9;
  strncpy(s_data.hours_wind_dir[0], "N",  sizeof(s_data.hours_wind_dir[0]));
  strncpy(s_data.hours_wind_dir[1], "NE", sizeof(s_data.hours_wind_dir[1]));
  strncpy(s_data.hours_wind_dir[2], "E",  sizeof(s_data.hours_wind_dir[2]));
  strncpy(s_data.hours_wind_dir[3], "SE", sizeof(s_data.hours_wind_dir[3]));
  strncpy(s_data.hours_wind_dir[4], "SW", sizeof(s_data.hours_wind_dir[4]));
  strncpy(s_data.hours_wind_dir[5], "W",  sizeof(s_data.hours_wind_dir[5]));
  // Precip amount (tenths of in/mm). Only the rainy hours have an amount;
  // those rows show the amount instead of the wind column.
  s_data.hours_precip_x10[0] = 0; s_data.hours_precip_x10[1] = 0;
  s_data.hours_precip_x10[2] = 0; s_data.hours_precip_x10[3] = 3;
  s_data.hours_precip_x10[4] = 2; s_data.hours_precip_x10[5] = 0;

  // Phase 10B: Week Ahead mock — 5 days.
  strncpy(s_data.days_label[0], "TUE", sizeof(s_data.days_label[0]));
  strncpy(s_data.days_label[1], "WED", sizeof(s_data.days_label[1]));
  strncpy(s_data.days_label[2], "THU", sizeof(s_data.days_label[2]));
  strncpy(s_data.days_label[3], "FRI", sizeof(s_data.days_label[3]));
  strncpy(s_data.days_label[4], "SAT", sizeof(s_data.days_label[4]));
  s_data.days_high[0] = 78; s_data.days_low[0] = 58; s_data.days_cond[0] = COND_SUNNY;        s_data.days_pop[0] = 5;
  s_data.days_high[1] = 72; s_data.days_low[1] = 60; s_data.days_cond[1] = COND_RAIN;         s_data.days_pop[1] = 80;
  s_data.days_high[2] = 68; s_data.days_low[2] = 54; s_data.days_cond[2] = COND_CLOUDY;       s_data.days_pop[2] = 25;
  s_data.days_high[3] = 75; s_data.days_low[3] = 56; s_data.days_cond[3] = COND_PARTLY_CLOUDY; s_data.days_pop[3] = 10;
  s_data.days_high[4] = 80; s_data.days_low[4] = 59; s_data.days_cond[4] = COND_SUNNY;        s_data.days_pop[4] = 0;

  // Phase 7: Night Sky mock — Waning Gibbous, 73% illuminated.
  s_data.moon_phase = 5;
  s_data.moon_illum = 73;
  strncpy(s_data.moon_name1, "WANING", sizeof(s_data.moon_name1));
  strncpy(s_data.moon_name2, "GIBBOUS", sizeof(s_data.moon_name2));

  // Phase 11: Golden Hour mock — typical late-spring NA latitudes.
  strncpy(s_data.blue_am, "5:42 AM", sizeof(s_data.blue_am));
  strncpy(s_data.gold_am, "6:14 AM", sizeof(s_data.gold_am));
  strncpy(s_data.gold_pm, "7:32 PM", sizeof(s_data.gold_pm));
  strncpy(s_data.blue_pm, "8:18 PM", sizeof(s_data.blue_pm));

  // Pollen unknown until PKJS proxy responds; -1 hides the badge.
  s_data.pollen_level = -1;
}
#endif  // TW_MOCK_DATA

void weather_data_init_empty(void) {
  memset(&s_data, 0, sizeof(s_data));
  // Two fields already have an "unknown" sentinel the cards understand, and
  // zero is a MEANING for both: rain_alert_min 0 is "rain this minute" (it
  // would paint a rain pill on every card) and pollen_level 0 is "none",
  // which is a claim. -1 is the absence of a claim in both.
  s_data.rain_alert_min = -1;
  s_data.pollen_level = -1;
  // Everything else stays zeroed, including:
  //   valid        — false, so weather_data_has_reading() is false, no card
  //                  draws a number, and prv_save_cache() refuses to persist
  //                  this struct over a real cache on a background wakeup.
  //   last_updated — 0, which glance.c, refresh_sheet.c and the status pill
  //                  already read as "never fetched" (the pill shows
  //                  "UPDATED --").
  //   units        — UNITS_IMPERIAL by value, but it is NOT a default we show:
  //                  the real units arrive with the payload and are not
  //                  persisted in settings, so while has_reading() is false no
  //                  card may print an F/C letter or an MPH/KMH label. There is
  //                  no honest unit to name before the first reading.
#ifdef TW_MOCK_DATA
  prv_init_mock();
#endif
}

bool weather_data_has_reading(void) { return s_data.valid; }

WeatherData *weather_data_get(void) { return &s_data; }

const char *uv_label(int uv) {
  if (uv <= 2) return "LOW";
  if (uv <= 5) return "MODERATE";
  if (uv <= 7) return "HIGH";
  if (uv <= 10) return "V.HIGH";
  return "EXTREME";
}

const char *wind_unit_label(const WeatherData *d) {
  // "M/S" and not "MS": the slash is what stops it reading as an abbreviation
  // of something else, and it costs no width that matters — at GOTHIC_14_BOLD
  // it measures within a pixel of "KMH", so the main card's already-tight
  // 144px worst case ("12KMH NNW" against 120px usable) is not made worse.
  //
  // One packed literal indexed by a shift, rather than a switch or an array of
  // three pointers. All three labels are 3 chars, so they sit on a 4-byte
  // stride and the whole function is a shift and an add — no branch table, no
  // 12-byte pointer array. That is not premature cleverness: the first draft of
  // this feature cost 120 bytes against ~400 of total headroom, and this is one
  // of the two edits that bought most of it back.
  //
  // Safe to index unguarded ONLY because comm.c clamps wind_units on receipt.
  static const char k[] = "MPH\0KMH\0M/S";
  return k + (d->wind_units << 2);
}

const char *aqi_label(int aqi) {
  // EPA AQI bands — must match the gauge color helper in cards/air_quality.c.
  if (aqi <= 50) return "GOOD";
  if (aqi <= 100) return "MODERATE";
  if (aqi <= 150) return "SENSITIVE";
  if (aqi <= 200) return "UNHEALTHY";
  if (aqi <= 300) return "V.UNHEALTHY";
  return "HAZARDOUS";
}
