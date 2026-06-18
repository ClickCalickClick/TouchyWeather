#include "comm.h"
#include "weather_data.h"
#include "theme.h"
#include "settings.h"
#include "refresh_sheet.h"
#include "cards/cards.h"
#include <string.h>
#include <stdlib.h>

static CommUpdateCb s_update_cb = NULL;

// Bumped from 100 -> 101 when last_updated was added to WeatherData,
// to avoid loading a stale-layout blob from older app versions.
// Bumped 101 -> 102 when Phase 11 added blue_am/gold_am/gold_pm/blue_pm
// (10 bytes each).
// Bumped 102 -> 103 when dew_point/use_dew_point/pollen_level were added.
// Bumped 103 -> 104 when uv_max was added (UV card now shows current UV
// as primary, with daily peak as a "PEAK n" subtitle).
// Bumped 104 -> 105 when Week Ahead grew from 4 to 5 days (days_* arrays
// enlarged, shifting every field after them — an old 4-day blob would
// misalign and render a stale/garbage 5th day until the next fetch).
#define PERSIST_KEY_CACHE 105

static void prv_save_cache(void) {
  WeatherData *d = weather_data_get();
  if (d->valid) {
    persist_write_data(PERSIST_KEY_CACHE, d, sizeof(WeatherData));
  }
}

static void prv_load_cache(void) {
  if (persist_exists(PERSIST_KEY_CACHE)) {
    WeatherData *d = weather_data_get();
    persist_read_data(PERSIST_KEY_CACHE, d, sizeof(WeatherData));
  }
}

static void prv_inbox_received(DictionaryIterator *iter, void *context) {
  WeatherData *d = weather_data_get();
  bool got_anything = false;

  Tuple *t;

  // Phase 12: Radar pixel chunks. Routed to the radar card and treated
  // as out-of-band — no cache writeback, no got_anything flag.
  Tuple *t_chunk_idx = dict_find(iter, MESSAGE_KEY_RadarChunkIdx);
  Tuple *t_chunk_data = dict_find(iter, MESSAGE_KEY_RadarChunkData);
  if (t_chunk_idx && t_chunk_data) {
    int idx = (int)t_chunk_idx->value->int32;
    Tuple *t_total = dict_find(iter, MESSAGE_KEY_RadarChunkTotal);
    Tuple *t_w = dict_find(iter, MESSAGE_KEY_RadarWidth);
    Tuple *t_h = dict_find(iter, MESSAGE_KEY_RadarHeight);
    Tuple *t_ts = dict_find(iter, MESSAGE_KEY_RadarTimestamp);
    int total = t_total ? (int)t_total->value->int32 : 0;
    int w = t_w ? (int)t_w->value->int32 : 0;
    int h = t_h ? (int)t_h->value->int32 : 0;
    uint32_t ts = t_ts ? (uint32_t)t_ts->value->int32 : 0;
    card_radar_receive_chunk(idx, total,
                             t_chunk_data->value->data,
                             (int)t_chunk_data->length,
                             w, h, ts);
    if (s_update_cb) s_update_cb();
    return;
  }
  Tuple *t_status = dict_find(iter, MESSAGE_KEY_RadarStatus);
  if (t_status) {
    card_radar_receive_status((int)t_status->value->int32);
    if (s_update_cb) s_update_cb();
    return;
  }

  if ((t = dict_find(iter, MESSAGE_KEY_Theme))) {
    // Clay radiogroup with string values delivers as TUPLE_CSTRING; older
    // builds / direct AppMessage tests deliver TUPLE_INT. Accept both.
    int theme_val = 0;
    if (t->type == TUPLE_CSTRING) {
      theme_val = atoi(t->value->cstring);
    } else {
      theme_val = (int)t->value->int32;
    }
    theme_set(theme_val ? THEME_DARK : THEME_LIGHT);
    if (s_update_cb) s_update_cb();
  }
  if ((t = dict_find(iter, MESSAGE_KEY_Temp))) { d->temp = t->value->int32; got_anything = true; }
  if ((t = dict_find(iter, MESSAGE_KEY_FeelsLike))) { d->feels_like = t->value->int32; }
  if ((t = dict_find(iter, MESSAGE_KEY_High))) { d->high = t->value->int32; }
  if ((t = dict_find(iter, MESSAGE_KEY_Low))) { d->low = t->value->int32; }
  if ((t = dict_find(iter, MESSAGE_KEY_Condition))) { d->condition = (WeatherCondition)t->value->int32; }
  if ((t = dict_find(iter, MESSAGE_KEY_Wind))) { d->wind_speed = t->value->int32; }
  if ((t = dict_find(iter, MESSAGE_KEY_WindDir))) {
    strncpy(d->wind_dir, t->value->cstring, sizeof(d->wind_dir) - 1);
    d->wind_dir[sizeof(d->wind_dir) - 1] = '\0';
  }
  if ((t = dict_find(iter, MESSAGE_KEY_Humidity))) { d->humidity = t->value->int32; }
  if ((t = dict_find(iter, MESSAGE_KEY_DewPoint))) { d->dew_point = t->value->int32; }
  if ((t = dict_find(iter, MESSAGE_KEY_UseDewPoint))) {
    d->use_dew_point = (t->value->int32 != 0);
  }
  if ((t = dict_find(iter, MESSAGE_KEY_LoopNavigation))) {
    settings_set_loop_nav(t->value->int32 != 0);
  }
  if ((t = dict_find(iter, MESSAGE_KEY_PollenLevel))) {
    d->pollen_level = (int)t->value->int32;
  }
  if ((t = dict_find(iter, MESSAGE_KEY_Precip0))) { d->precip[0] = t->value->int32; }
  if ((t = dict_find(iter, MESSAGE_KEY_Precip1))) { d->precip[1] = t->value->int32; }
  if ((t = dict_find(iter, MESSAGE_KEY_Precip2))) { d->precip[2] = t->value->int32; }
  if ((t = dict_find(iter, MESSAGE_KEY_Precip3))) { d->precip[3] = t->value->int32; }
  if ((t = dict_find(iter, MESSAGE_KEY_Precip4))) { d->precip[4] = t->value->int32; }
  if ((t = dict_find(iter, MESSAGE_KEY_UV))) { d->uv = t->value->int32; }
  if ((t = dict_find(iter, MESSAGE_KEY_UVMax))) { d->uv_max = t->value->int32; }
  if ((t = dict_find(iter, MESSAGE_KEY_AQI))) { d->aqi = t->value->int32; }
  if ((t = dict_find(iter, MESSAGE_KEY_Sunrise))) {
    strncpy(d->sunrise, t->value->cstring, sizeof(d->sunrise) - 1);
    d->sunrise[sizeof(d->sunrise) - 1] = '\0';
  }
  if ((t = dict_find(iter, MESSAGE_KEY_Sunset))) {
    strncpy(d->sunset, t->value->cstring, sizeof(d->sunset) - 1);
    d->sunset[sizeof(d->sunset) - 1] = '\0';
  }
  if ((t = dict_find(iter, MESSAGE_KEY_RainAlertMinutes))) { d->rain_alert_min = t->value->int32; }
  if ((t = dict_find(iter, MESSAGE_KEY_Units))) { d->units = (Units)t->value->int32; }
  if ((t = dict_find(iter, MESSAGE_KEY_LastUpdated))) {
    // PKJS sends the unix-second timestamp of the fetch. Sentinel "1" from
    // our refresh-trigger doesn't carry useful time info; treat any value
    // < 100000 as "not a real timestamp" and use device-clock time instead.
    uint32_t v = (uint32_t)t->value->int32;
    if (v >= 100000U) {
      d->last_updated = v;
    } else {
      d->last_updated = (uint32_t)time(NULL);
    }
    got_anything = true;
  }

  // Phase 10A: Next 6 Hours (was 4 in Phase 5).
  uint32_t hour_label_keys[6] = {
    MESSAGE_KEY_Hour1Label, MESSAGE_KEY_Hour2Label,
    MESSAGE_KEY_Hour3Label, MESSAGE_KEY_Hour4Label,
    MESSAGE_KEY_Hour5Label, MESSAGE_KEY_Hour6Label
  };
  uint32_t hour_temp_keys[6] = {
    MESSAGE_KEY_Hour1Temp, MESSAGE_KEY_Hour2Temp,
    MESSAGE_KEY_Hour3Temp, MESSAGE_KEY_Hour4Temp,
    MESSAGE_KEY_Hour5Temp, MESSAGE_KEY_Hour6Temp
  };
  uint32_t hour_cond_keys[6] = {
    MESSAGE_KEY_Hour1Cond, MESSAGE_KEY_Hour2Cond,
    MESSAGE_KEY_Hour3Cond, MESSAGE_KEY_Hour4Cond,
    MESSAGE_KEY_Hour5Cond, MESSAGE_KEY_Hour6Cond
  };
  uint32_t hour_pop_keys[6] = {
    MESSAGE_KEY_Hour1Pop, MESSAGE_KEY_Hour2Pop,
    MESSAGE_KEY_Hour3Pop, MESSAGE_KEY_Hour4Pop,
    MESSAGE_KEY_Hour5Pop, MESSAGE_KEY_Hour6Pop
  };
  uint32_t hour_wind_keys[6] = {
    MESSAGE_KEY_Hour1Wind, MESSAGE_KEY_Hour2Wind,
    MESSAGE_KEY_Hour3Wind, MESSAGE_KEY_Hour4Wind,
    MESSAGE_KEY_Hour5Wind, MESSAGE_KEY_Hour6Wind
  };
  uint32_t hour_wdir_keys[6] = {
    MESSAGE_KEY_Hour1WindDir, MESSAGE_KEY_Hour2WindDir,
    MESSAGE_KEY_Hour3WindDir, MESSAGE_KEY_Hour4WindDir,
    MESSAGE_KEY_Hour5WindDir, MESSAGE_KEY_Hour6WindDir
  };
  uint32_t hour_precip_keys[6] = {
    MESSAGE_KEY_Hour1Precip, MESSAGE_KEY_Hour2Precip,
    MESSAGE_KEY_Hour3Precip, MESSAGE_KEY_Hour4Precip,
    MESSAGE_KEY_Hour5Precip, MESSAGE_KEY_Hour6Precip
  };
  for (int i = 0; i < 6; i++) {
    if ((t = dict_find(iter, hour_label_keys[i]))) {
      strncpy(d->hours_label[i], t->value->cstring,
              sizeof(d->hours_label[i]) - 1);
      d->hours_label[i][sizeof(d->hours_label[i]) - 1] = '\0';
    }
    if ((t = dict_find(iter, hour_temp_keys[i]))) {
      d->hours_temp[i] = t->value->int32;
    }
    if ((t = dict_find(iter, hour_cond_keys[i]))) {
      d->hours_cond[i] = (WeatherCondition)t->value->int32;
    }
    if ((t = dict_find(iter, hour_pop_keys[i]))) {
      d->hours_pop[i] = (uint8_t)t->value->int32;
    }
    if ((t = dict_find(iter, hour_wind_keys[i]))) {
      d->hours_wind[i] = t->value->int32;
    }
    if ((t = dict_find(iter, hour_wdir_keys[i]))) {
      strncpy(d->hours_wind_dir[i], t->value->cstring,
              sizeof(d->hours_wind_dir[i]) - 1);
      d->hours_wind_dir[i][sizeof(d->hours_wind_dir[i]) - 1] = '\0';
    }
    if ((t = dict_find(iter, hour_precip_keys[i]))) {
      d->hours_precip_x10[i] = t->value->int32;
    }
  }

  // Phase 10B: Week Ahead — 5 days (was 4).
  uint32_t day_label_keys[5] = {
    MESSAGE_KEY_Day0Label, MESSAGE_KEY_Day1Label,
    MESSAGE_KEY_Day2Label, MESSAGE_KEY_Day3Label,
    MESSAGE_KEY_Day4Label
  };
  uint32_t day_high_keys[5] = {
    MESSAGE_KEY_Day0High, MESSAGE_KEY_Day1High,
    MESSAGE_KEY_Day2High, MESSAGE_KEY_Day3High,
    MESSAGE_KEY_Day4High
  };
  uint32_t day_low_keys[5] = {
    MESSAGE_KEY_Day0Low, MESSAGE_KEY_Day1Low,
    MESSAGE_KEY_Day2Low, MESSAGE_KEY_Day3Low,
    MESSAGE_KEY_Day4Low
  };
  uint32_t day_cond_keys[5] = {
    MESSAGE_KEY_Day0Cond, MESSAGE_KEY_Day1Cond,
    MESSAGE_KEY_Day2Cond, MESSAGE_KEY_Day3Cond,
    MESSAGE_KEY_Day4Cond
  };
  uint32_t day_pop_keys[5] = {
    MESSAGE_KEY_Day0Pop, MESSAGE_KEY_Day1Pop,
    MESSAGE_KEY_Day2Pop, MESSAGE_KEY_Day3Pop,
    MESSAGE_KEY_Day4Pop
  };
  for (int i = 0; i < 5; i++) {
    if ((t = dict_find(iter, day_label_keys[i]))) {
      strncpy(d->days_label[i], t->value->cstring,
              sizeof(d->days_label[i]) - 1);
      d->days_label[i][sizeof(d->days_label[i]) - 1] = '\0';
    }
    if ((t = dict_find(iter, day_high_keys[i]))) d->days_high[i] = t->value->int32;
    if ((t = dict_find(iter, day_low_keys[i])))  d->days_low[i]  = t->value->int32;
    if ((t = dict_find(iter, day_cond_keys[i]))) d->days_cond[i] = (WeatherCondition)t->value->int32;
    if ((t = dict_find(iter, day_pop_keys[i])))  d->days_pop[i]  = (uint8_t)t->value->int32;
  }

  // Phase 7: Moon.
  if ((t = dict_find(iter, MESSAGE_KEY_MoonPhase))) d->moon_phase = (uint8_t)t->value->int32;
  if ((t = dict_find(iter, MESSAGE_KEY_MoonIllum))) d->moon_illum = (uint8_t)t->value->int32;
  if ((t = dict_find(iter, MESSAGE_KEY_MoonName1))) {
    strncpy(d->moon_name1, t->value->cstring, sizeof(d->moon_name1) - 1);
    d->moon_name1[sizeof(d->moon_name1) - 1] = '\0';
  }
  if ((t = dict_find(iter, MESSAGE_KEY_MoonName2))) {
    strncpy(d->moon_name2, t->value->cstring, sizeof(d->moon_name2) - 1);
    d->moon_name2[sizeof(d->moon_name2) - 1] = '\0';
  }

  // Phase 11: Golden Hour milestone times (formatted strings).
  if ((t = dict_find(iter, MESSAGE_KEY_BlueAm))) {
    strncpy(d->blue_am, t->value->cstring, sizeof(d->blue_am) - 1);
    d->blue_am[sizeof(d->blue_am) - 1] = '\0';
  }
  if ((t = dict_find(iter, MESSAGE_KEY_GoldAm))) {
    strncpy(d->gold_am, t->value->cstring, sizeof(d->gold_am) - 1);
    d->gold_am[sizeof(d->gold_am) - 1] = '\0';
  }
  if ((t = dict_find(iter, MESSAGE_KEY_GoldPm))) {
    strncpy(d->gold_pm, t->value->cstring, sizeof(d->gold_pm) - 1);
    d->gold_pm[sizeof(d->gold_pm) - 1] = '\0';
  }
  if ((t = dict_find(iter, MESSAGE_KEY_BluePm))) {
    strncpy(d->blue_pm, t->value->cstring, sizeof(d->blue_pm) - 1);
    d->blue_pm[sizeof(d->blue_pm) - 1] = '\0';
  }

  if (got_anything) {
    d->valid = true;
    prv_save_cache();
    if (s_update_cb) s_update_cb();
    // Notify the pull-to-refresh sheet so it can close. Safe in any
    // state — it's a no-op unless the sheet is currently loading.
    refresh_sheet_on_data_received();
  }
}

static void prv_inbox_dropped(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "AppMessage dropped: %d", (int)reason);
}

void comm_request_refresh(void) {
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) != APP_MSG_OK) return;
  // Send a sentinel key to trigger PKJS fetch.
  dict_write_uint8(iter, MESSAGE_KEY_LastUpdated, 1);
  // Tell PKJS the watch's system clock style so "Match watch" time format
  // works. PKJS does all time formatting; only the C side knows this.
  dict_write_uint8(iter, MESSAGE_KEY_ClockIs24h, clock_is_24h_style() ? 1 : 0);
  dict_write_end(iter);
  app_message_outbox_send();
}

void comm_request_radar(void) {
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) != APP_MSG_OK) return;
  dict_write_uint8(iter, MESSAGE_KEY_RadarRequest, 1);
  dict_write_end(iter);
  app_message_outbox_send();
}

void comm_set_update_callback(CommUpdateCb cb) {
  s_update_cb = cb;
}

static void prv_initial_refresh(void *ctx) {
  (void)ctx;
  comm_request_refresh();
}

void comm_load_cache(void) {
  prv_load_cache();
  // Trigger redraw if cache was loaded so the screen reconciles immediately.
  // This prevents the imperial mock flash for metric users.
  if (s_update_cb && weather_data_get()->valid) {
    s_update_cb();
  }
}

void comm_init(void) {
  // Cache loading now happens separately in prv_init(), before window push.
  app_message_register_inbox_received(prv_inbox_received);
  app_message_register_inbox_dropped(prv_inbox_dropped);
  // Inbox bumped 1024 -> 2048 in Phase 12 to fit a 1500-byte radar
  // pixel chunk plus message tuple overhead.
  app_message_open(2048, 256);
  // Refresh-on-open: PKJS 'ready' usually fires soon after launch, but in
  // background-relaunch / cached-PKJS scenarios it doesn't. Send our own
  // request after a short delay so AppMessage is fully open first.
  app_timer_register(750, prv_initial_refresh, NULL);
}

void comm_deinit(void) {
  app_message_deregister_callbacks();
}
