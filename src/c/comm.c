#include "comm.h"
#include "weather_data.h"
#include "theme.h"
#include "settings.h"
#include "refresh_sheet.h"
#include "anim.h"
#include "cards/cards.h"
#include <string.h>
#include <stdlib.h>

static CommUpdateCb s_update_cb = NULL;
static CommVisibilityCb s_visibility_cb = NULL;

// Phase 3.2 auto-hide tuning (see DECISIONS.md 3.2 to change these).
#define AUTO_HIDE_STALE_SECS   (3 * 60 * 60)  // fail open if data older than 3h
#define AUTO_HIDE_POP_THRESHOLD 40            // % chance in the next 6h → "rain"
#define AUTO_HIDE_DRY_UPDATES   2             // consecutive dry evals before hide
static int s_dry_updates = 0;                 // hysteresis counter
static void prv_evaluate_auto_hide(void);

// Background update state
static int s_bg_fail_count = 0;
static const int s_max_backoff_mins = 240;  // 4 hours cap
static AppTimer *s_bg_timeout_timer = NULL;
static bool s_is_background_mode = false;
static WakeupId s_wakeup_id = -1;
static AppTimer *s_bg_exit_timer = NULL;
// Off-screen keep-alive window for background (wakeup) launches. Pebble's
// app_event_loop() returns the instant the window stack is empty, so a
// headless fetch with no window pushed would exit before any async
// AppMessage round-trip. We push this on background init and pop it once
// the fetch finishes (or times out) so the loop stays alive in between.
static Window *s_bg_window = NULL;

// Persistent storage key for wakeup ID
#define PERSIST_KEY_WAKEUP_ID 300

// Forward declarations
static void prv_reschedule_wakeup(bool success);
static void prv_wakeup_handler(WakeupId id, int32_t reason);
static void prv_bg_exit(void *data);

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
// Bumped 105 -> 106 when location_name[32] was added to WeatherData
// (shifts every field after sunset[8]; an old blob would misalign).
// Bumped 106 -> 107 when show_location bool was added to WeatherData.
// Bumped 107 -> 108 when Phase 4's UV/AQI detail modals added hours_uv[6]
// + pm2_5/pm10/o3/no2 (shifts nothing before pollen_level, but the struct
// grew — an old 107 blob would leave the new fields as garbage).
#define PERSIST_KEY_CACHE 108

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
  if ((t = dict_find(iter, MESSAGE_KEY_ShowLocation))) {
    d->show_location = (t->value->int32 != 0);
  }
  if ((t = dict_find(iter, MESSAGE_KEY_LoopNavigation))) {
    settings_set_loop_nav(t->value->int32 != 0);
  }
  if ((t = dict_find(iter, MESSAGE_KEY_AnimationsEnabled))) {
    // Clay toggle → int 0/1; accept a CSTRING form too, for robustness.
    bool on = (t->type == TUPLE_CSTRING) ? (atoi(t->value->cstring) != 0)
                                         : (t->value->int32 != 0);
    settings_set_animations_enabled(on);
    // Resume decorative animation immediately when turned on; when turned
    // off, anim_kick() is harmless (the next tick re-settles and stops).
    anim_kick();
    if (s_update_cb) s_update_cb();
  }
  if ((t = dict_find(iter, MESSAGE_KEY_SelectTogglesTheme))) {
    bool on = (t->type == TUPLE_CSTRING) ? (atoi(t->value->cstring) != 0)
                                         : (t->value->int32 != 0);
    settings_set_select_toggles_theme(on);
  }
  if ((t = dict_find(iter, MESSAGE_KEY_BigMode))) {
    // Big Mode (Stage B): larger fonts + high contrast. Repaint immediately so
    // the current card switches look the moment the toggle changes.
    bool on = (t->type == TUPLE_CSTRING) ? (atoi(t->value->cstring) != 0)
                                         : (t->value->int32 != 0);
    settings_set_big_mode(on);
    if (s_update_cb) s_update_cb();
  }
  // Phase 2.2 + Settings-in-Clay: per-card visibility, card order and the
  // hide-Settings-card opt-in from Clay. Visibility/order only apply when the
  // user has opted into phone control (PhoneManagesCards); otherwise on-watch
  // card management is left untouched.
  bool cards_changed = false;
  if ((t = dict_find(iter, MESSAGE_KEY_PhoneManagesCards))) {
    bool on = (t->type == TUPLE_CSTRING) ? (atoi(t->value->cstring) != 0)
                                         : (t->value->int32 != 0);
    // A gate flip changes the EFFECTIVE hide-Settings state (it's an AND of
    // the two toggles), so nav must re-apply — e.g. turning phone management
    // off must immediately bring the Settings card back.
    if (on != settings_get_phone_manages_cards()) cards_changed = true;
    settings_set_phone_manages_cards(on);
  }
  if ((t = dict_find(iter, MESSAGE_KEY_HideSettingsCard))) {
    bool on = (t->type == TUPLE_CSTRING) ? (atoi(t->value->cstring) != 0)
                                         : (t->value->int32 != 0);
    // Stored unconditionally; only effective while PhoneManagesCards is on
    // (settings_get_settings_card_hidden ANDs the two — the fail-safe).
    if (on != settings_get_hide_settings_card()) cards_changed = true;
    settings_set_hide_settings_card(on);
  }
  if (settings_get_phone_manages_cards()) {
    // Message key → ToggleId. Keys carry 1 = show, 0 = hide per card.
    // Not static: MESSAGE_KEY_* are runtime symbols, not constant expressions.
    const struct { uint32_t key; ToggleId tid; } vis_map[] = {
      { MESSAGE_KEY_CardEnabledHours,  TOGGLE_HOURS  },
      { MESSAGE_KEY_CardEnabledWeek,   TOGGLE_WEEK   },
      { MESSAGE_KEY_CardEnabledPrecip, TOGGLE_PRECIP },
      { MESSAGE_KEY_CardEnabledUV,     TOGGLE_UV     },
      { MESSAGE_KEY_CardEnabledAQ,     TOGGLE_AQ     },
      { MESSAGE_KEY_CardEnabledSun,    TOGGLE_SUN    },
      { MESSAGE_KEY_CardEnabledNight,  TOGGLE_NIGHT  },
      { MESSAGE_KEY_CardEnabledGolden, TOGGLE_GOLDEN },
      { MESSAGE_KEY_CardEnabledRadar,  TOGGLE_RADAR  },
      { MESSAGE_KEY_CardEnabledAdvice, TOGGLE_ADVICE },
    };
    for (unsigned i = 0; i < sizeof(vis_map) / sizeof(vis_map[0]); ++i) {
      Tuple *vt = dict_find(iter, vis_map[i].key);
      if (!vt) continue;
      bool en = (vt->type == TUPLE_CSTRING) ? (atoi(vt->value->cstring) != 0)
                                            : (vt->value->int32 != 0);
      settings_set_enabled(vis_map[i].tid, en);
      cards_changed = true;
    }
    // Card ORDER from Clay (the drag-reorder control) — a CSV of ToggleIds.
    // Strictly validated in settings_apply_order_csv; a bad string is ignored.
    Tuple *ot = dict_find(iter, MESSAGE_KEY_CardOrder);
    if (ot && ot->type == TUPLE_CSTRING &&
        settings_apply_order_csv(ot->value->cstring)) {
      cards_changed = true;
    }
  }
  // Re-apply the (now-updated) enable flags, visual order and Settings-card
  // visibility to nav so the rotation and page dots reflect Clay immediately.
  if (cards_changed && s_visibility_cb) s_visibility_cb();
  if ((t = dict_find(iter, MESSAGE_KEY_AutoHidePrecip))) {
    bool on = (t->type == TUPLE_CSTRING) ? (atoi(t->value->cstring) != 0)
                                         : (t->value->int32 != 0);
    settings_set_auto_hide_precip(on);
    // Re-evaluate immediately so turning it on/off takes effect at once.
    prv_evaluate_auto_hide();
  }
  if ((t = dict_find(iter, MESSAGE_KEY_BackgroundUpdateInterval))) {
    int old_interval = settings_get_background_interval();
    // Clay's `select` control delivers its string option values
    // ("0"/"1800"/"3600") as TUPLE_CSTRING, just like the Theme
    // radiogroup above. Reading ->int32 here would interpret the raw
    // ASCII bytes ("1800" -> 0x30303831 ~= 808M) and schedule a wakeup
    // ~25 years out, silently killing background updates. Accept both
    // encodings (direct AppMessage tests still send TUPLE_INT).
    int new_interval;
    if (t->type == TUPLE_CSTRING) {
      new_interval = atoi(t->value->cstring);
    } else {
      new_interval = (int)t->value->int32;
    }
    settings_set_background_interval(new_interval);
    
    // If interval changed, reschedule or cancel wakeups
    if (new_interval != old_interval) {
      if (new_interval == 0) {
        // Disabled - cancel all wakeups
        wakeup_cancel_all();
        APP_LOG(APP_LOG_LEVEL_INFO, "BG: Disabled, cancelled wakeups");
      } else {
        // Enabled or interval changed - reschedule
        prv_reschedule_wakeup(true);
        APP_LOG(APP_LOG_LEVEL_INFO, "BG: Interval changed to %d secs", new_interval);
      }
    }
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
  if ((t = dict_find(iter, MESSAGE_KEY_PM25))) { d->pm2_5 = t->value->int32; }
  if ((t = dict_find(iter, MESSAGE_KEY_PM10))) { d->pm10 = t->value->int32; }
  if ((t = dict_find(iter, MESSAGE_KEY_O3)))   { d->o3 = t->value->int32; }
  if ((t = dict_find(iter, MESSAGE_KEY_NO2)))  { d->no2 = t->value->int32; }
  if ((t = dict_find(iter, MESSAGE_KEY_Sunrise))) {
    strncpy(d->sunrise, t->value->cstring, sizeof(d->sunrise) - 1);
    d->sunrise[sizeof(d->sunrise) - 1] = '\0';
  }
  if ((t = dict_find(iter, MESSAGE_KEY_Sunset))) {
    strncpy(d->sunset, t->value->cstring, sizeof(d->sunset) - 1);
    d->sunset[sizeof(d->sunset) - 1] = '\0';
  }
  if ((t = dict_find(iter, MESSAGE_KEY_LocationName))) {
    strncpy(d->location_name, t->value->cstring, sizeof(d->location_name) - 1);
    d->location_name[sizeof(d->location_name) - 1] = '\0';
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
  uint32_t hour_uv_keys[6] = {
    MESSAGE_KEY_Hour1Uv, MESSAGE_KEY_Hour2Uv,
    MESSAGE_KEY_Hour3Uv, MESSAGE_KEY_Hour4Uv,
    MESSAGE_KEY_Hour5Uv, MESSAGE_KEY_Hour6Uv
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
    if ((t = dict_find(iter, hour_uv_keys[i]))) {
      d->hours_uv[i] = (uint8_t)t->value->int32;
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
    prv_evaluate_auto_hide();  // Phase 3.2: re-hide/show precip+radar per forecast
    anim_kick();  // fresh data: wake the hero icon for another window
    if (s_update_cb) s_update_cb();
    // Notify the pull-to-refresh sheet so it can close. Safe in any
    // state — it's a no-op unless the sheet is currently loading.
    refresh_sheet_on_data_received();

    // If we're in background mode, cancel timeout and reschedule next wakeup
    if (s_is_background_mode) {
      if (s_bg_timeout_timer) {
        app_timer_cancel(s_bg_timeout_timer);
        s_bg_timeout_timer = NULL;
      }
      APP_LOG(APP_LOG_LEVEL_INFO, "BG: Data received successfully");
      prv_reschedule_wakeup(true);  // Success - normal interval
      s_is_background_mode = false;

      // Schedule delayed exit to allow cleanup to complete
      s_bg_exit_timer = app_timer_register(100, prv_bg_exit, NULL);
    }
  }
}

static void prv_inbox_dropped(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "AppMessage dropped: %d", (int)reason);

  // If we're in background mode, this is a failure - reschedule with backoff
  if (s_is_background_mode) {
    if (s_bg_timeout_timer) {
      app_timer_cancel(s_bg_timeout_timer);
      s_bg_timeout_timer = NULL;
    }
    APP_LOG(APP_LOG_LEVEL_ERROR, "BG: Message dropped, rescheduling with backoff");
    prv_reschedule_wakeup(false);  // Failure - exponential backoff
    s_is_background_mode = false;

    // Schedule delayed exit to allow cleanup to complete
    s_bg_exit_timer = app_timer_register(100, prv_bg_exit, NULL);
  }
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

// --- Watch→Clay card-state seed ---------------------------------------------
// PKJS caches this state and injects it into Clay's persisted settings before
// the config page opens, so Clay always shows the watch's true card config
// (and a Clay save can no longer silently wipe on-watch changes).

#define CARD_STATE_DEBOUNCE_MS 900
#define CARD_STATE_RETRY_MS    2000
#define CARD_STATE_MAX_TRIES   5

static AppTimer *s_card_state_timer = NULL;
static int s_card_state_tries = 0;

static void prv_send_card_state_now(void *ctx) {
  (void)ctx;
  s_card_state_timer = NULL;
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) != APP_MSG_OK) {
    // Outbox busy (a refresh request may still be in flight). Retry a few
    // times, then give up — the next change or launch re-sends anyway.
    if (++s_card_state_tries < CARD_STATE_MAX_TRIES) {
      s_card_state_timer =
          app_timer_register(CARD_STATE_RETRY_MS, prv_send_card_state_now, NULL);
    }
    return;
  }

  // Visual order as a CSV of ToggleIds ("9,0,1,..."), matching the format the
  // Clay card-order control round-trips back.
  char order_csv[SETTINGS_TOGGLEABLE_COUNT * 2 + 1];
  int pos = 0;
  for (int i = 0; i < SETTINGS_TOGGLEABLE_COUNT; ++i) {
    if (i) order_csv[pos++] = ',';
    order_csv[pos++] = (char)('0' + (int)settings_visual_id(i));
  }
  order_csv[pos] = '\0';
  dict_write_cstring(iter, MESSAGE_KEY_CardOrder, order_csv);

  // Per-card enable flags, keyed exactly like the Clay toggles so PKJS can
  // inject them 1:1. Not static: MESSAGE_KEY_* are runtime symbols.
  const struct { uint32_t key; ToggleId tid; } vis_map[] = {
    { MESSAGE_KEY_CardEnabledHours,  TOGGLE_HOURS  },
    { MESSAGE_KEY_CardEnabledWeek,   TOGGLE_WEEK   },
    { MESSAGE_KEY_CardEnabledPrecip, TOGGLE_PRECIP },
    { MESSAGE_KEY_CardEnabledUV,     TOGGLE_UV     },
    { MESSAGE_KEY_CardEnabledAQ,     TOGGLE_AQ     },
    { MESSAGE_KEY_CardEnabledSun,    TOGGLE_SUN    },
    { MESSAGE_KEY_CardEnabledNight,  TOGGLE_NIGHT  },
    { MESSAGE_KEY_CardEnabledGolden, TOGGLE_GOLDEN },
    { MESSAGE_KEY_CardEnabledRadar,  TOGGLE_RADAR  },
    { MESSAGE_KEY_CardEnabledAdvice, TOGGLE_ADVICE },
  };
  for (unsigned i = 0; i < sizeof(vis_map) / sizeof(vis_map[0]); ++i) {
    dict_write_uint8(iter, vis_map[i].key,
                     settings_get_enabled(vis_map[i].tid) ? 1 : 0);
  }
  dict_write_uint8(iter, MESSAGE_KEY_PhoneManagesCards,
                   settings_get_phone_manages_cards() ? 1 : 0);
  // Raw toggle value (not the gated AND) so Clay shows what the user chose.
  dict_write_uint8(iter, MESSAGE_KEY_HideSettingsCard,
                   settings_get_hide_settings_card() ? 1 : 0);
  dict_write_end(iter);
  app_message_outbox_send();
}

void comm_send_card_state(void) {
  s_card_state_tries = 0;
  if (s_card_state_timer) app_timer_cancel(s_card_state_timer);
  s_card_state_timer =
      app_timer_register(CARD_STATE_DEBOUNCE_MS, prv_send_card_state_now, NULL);
}

void comm_set_update_callback(CommUpdateCb cb) {
  s_update_cb = cb;
}

void comm_set_visibility_callback(CommVisibilityCb cb) {
  s_visibility_cb = cb;
}

// Phase 3.2: is measurable rain expected soon? Cautious (false-negative averse):
// any of the near-term signals firing means "rain" → keep the cards shown.
static bool prv_rain_expected(WeatherData *d) {
  if (d->rain_alert_min >= 0) return true;  // data source flagged imminent rain
  for (int i = 0; i < 6; ++i) {             // high chance in the next 6 hours
    if (d->hours_pop[i] >= AUTO_HIDE_POP_THRESHOLD) return true;
  }
  return false;
}

// Decide whether precip+radar should be auto-hidden right now and apply it.
// Guardrails: fail open on bad/stale data, hysteresis (hide slowly, show fast),
// and never mutate the user's persisted enable flags (auto-hidden is separate;
// effective visibility = user_enabled AND NOT auto_hidden).
static void prv_evaluate_auto_hide(void) {
  bool want_hidden;
  if (!settings_get_auto_hide_precip()) {
    want_hidden = false;      // feature off → never auto-hide
    s_dry_updates = 0;
  } else {
    WeatherData *d = weather_data_get();
    uint32_t now = (uint32_t)time(NULL);
    bool stale = (d->last_updated == 0) ||
                 (now - d->last_updated > AUTO_HIDE_STALE_SECS);
    if (!d->valid || stale) {
      want_hidden = false;    // fail open: never hide on missing/old data
      s_dry_updates = 0;
    } else if (prv_rain_expected(d)) {
      want_hidden = false;    // rain → show immediately, reset hysteresis
      s_dry_updates = 0;
    } else {
      s_dry_updates++;        // dry → hide only after N consecutive dry evals
      want_hidden = (s_dry_updates >= AUTO_HIDE_DRY_UPDATES);
    }
  }
  bool changed =
      (settings_get_auto_hidden(TOGGLE_PRECIP) != want_hidden) ||
      (settings_get_auto_hidden(TOGGLE_RADAR)  != want_hidden);
  if (changed) {
    settings_set_auto_hidden(TOGGLE_PRECIP, want_hidden);
    settings_set_auto_hidden(TOGGLE_RADAR, want_hidden);
    if (s_visibility_cb) s_visibility_cb();  // re-apply effective visibility
  }
}

static void prv_initial_refresh(void *ctx) {
  (void)ctx;
  
  WeatherData *d = weather_data_get();
  
  // If no cached data exists, always fetch
  if (!d->valid || d->last_updated == 0) {
    APP_LOG(APP_LOG_LEVEL_INFO, "No cached data, fetching on open");
    comm_request_refresh();
    return;
  }
  
  // Check age of cached data
  uint32_t now = (uint32_t)time(NULL);
  uint32_t age_secs = now - d->last_updated;
  uint32_t threshold_secs = 15 * 60;  // 15 minutes
  
  if (age_secs < threshold_secs) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Data is %lu secs old (<%lu threshold), skipping fetch", 
            (unsigned long)age_secs, (unsigned long)threshold_secs);
    return;
  }
  
  APP_LOG(APP_LOG_LEVEL_INFO, "Data is %lu secs old (>%lu threshold), fetching on open", 
          (unsigned long)age_secs, (unsigned long)threshold_secs);
  comm_request_refresh();
}

void comm_load_cache(void) {
  prv_load_cache();
  // Phase 3.2: evaluate auto-hide against the just-loaded cache so the rotation
  // reflects the forecast from the first draw (hysteresis means one dry cache
  // read won't hide yet — it takes effect after a subsequent dry update).
  prv_evaluate_auto_hide();
  // Trigger redraw if cache was loaded so the screen reconciles immediately.
  // This prevents the imperial mock flash for metric users.
  if (s_update_cb && weather_data_get()->valid) {
    s_update_cb();
  }
}

// Reschedule wakeup with exponential backoff on failure
static void prv_reschedule_wakeup(bool success) {
  int interval = settings_get_background_interval();
  if (interval == 0) return;  // Disabled

  int delay_secs;
  if (success) {
    s_bg_fail_count = 0;
    delay_secs = interval;  // Normal interval (30 or 60 mins)
  } else {
    s_bg_fail_count++;
    // Exponential: 5, 10, 20, 40, 80, 160 mins (cap at 4 hrs)
    int backoff_mins = 5 * (1 << (s_bg_fail_count - 1));
    if (backoff_mins > s_max_backoff_mins) {
      backoff_mins = s_max_backoff_mins;
    }
    delay_secs = backoff_mins * 60;
  }

  time_t next_wakeup = time(NULL) + delay_secs;
  
  // Cancel existing wakeup
  if (s_wakeup_id >= 0) {
    wakeup_cancel(s_wakeup_id);
    s_wakeup_id = -1;
  }
  wakeup_cancel_all();  // Extra safety - cancel any orphaned wakeups
  
  // Retry wakeup scheduling with incremental 1-minute offsets to handle
  // slot conflicts. Pattern from pebble/goodthink - try up to 15 times.
  WakeupId wid = -1;
  uint8_t try = 0;
  while (wid < 0 && try < 15) {
    wid = wakeup_schedule(next_wakeup + (SECONDS_PER_MINUTE * try), 0, true);
    try++;
  }
  
  if (wid >= 0) {
    s_wakeup_id = wid;
    persist_write_int(PERSIST_KEY_WAKEUP_ID, wid);
    APP_LOG(APP_LOG_LEVEL_INFO, "BG: Wakeup scheduled in %d seconds (wid=%d, attempts=%d)", 
            delay_secs, (int)wid, try);
  } else {
    APP_LOG(APP_LOG_LEVEL_ERROR, "BG: Wakeup scheduling FAILED after %d attempts", try);
    persist_delete(PERSIST_KEY_WAKEUP_ID);
  }
}

// Wakeup handler - called when wakeup fires while app is running
static void prv_wakeup_handler(WakeupId id, int32_t reason) {
  (void)reason;
  APP_LOG(APP_LOG_LEVEL_INFO, "BG: Wakeup fired while app running (wid=%d)", (int)id);
  
  // Clean up persisted ID
  if (persist_exists(PERSIST_KEY_WAKEUP_ID)) {
    persist_delete(PERSIST_KEY_WAKEUP_ID);
  }
  s_wakeup_id = -1;
  
  // Trigger a fetch
  comm_request_refresh();
  
  // Schedule next wakeup
  prv_reschedule_wakeup(true);
}

// Delayed exit callback - closes AppMessage and allows app to terminate
static void prv_bg_exit(void *data) {
  (void)data;
  APP_LOG(APP_LOG_LEVEL_INFO, "BG: Closing AppMessage and exiting");
  app_message_deregister_callbacks();
  s_bg_exit_timer = NULL;
  // Pop (and free) the keep-alive window so the window stack empties and
  // app_event_loop() returns, letting the app terminate cleanly.
  if (s_bg_window) {
    window_stack_remove(s_bg_window, false);
    window_destroy(s_bg_window);
    s_bg_window = NULL;
  }
}

// Background timeout - no data arrived within 28 seconds
static void prv_bg_timeout(void *data) {
  (void)data;
  APP_LOG(APP_LOG_LEVEL_WARNING, "BG: Timeout - no data received");
  prv_reschedule_wakeup(false);  // Failure - exponential backoff
  s_bg_timeout_timer = NULL;
  s_is_background_mode = false;

  // Schedule delayed exit to allow cleanup to complete
  s_bg_exit_timer = app_timer_register(100, prv_bg_exit, NULL);
}

// Background mode initialization - minimal init, no UI
void comm_background_init(void) {
  int interval = settings_get_background_interval();
  if (interval == 0) {
    APP_LOG(APP_LOG_LEVEL_INFO, "BG: Disabled, skipping");
    return;
  }

  // Clean up the persisted wakeup ID that triggered this launch
  if (persist_exists(PERSIST_KEY_WAKEUP_ID)) {
    WakeupId triggered_id = persist_read_int(PERSIST_KEY_WAKEUP_ID);
    APP_LOG(APP_LOG_LEVEL_INFO, "BG: Cleaning up triggered wakeup (wid=%d)", (int)triggered_id);
    persist_delete(PERSIST_KEY_WAKEUP_ID);
  }
  s_wakeup_id = -1;

  APP_LOG(APP_LOG_LEVEL_INFO, "BG: Starting background fetch");
  s_is_background_mode = true;

  // Keep the event loop alive: without a window on the stack,
  // app_event_loop() returns immediately and the OS tears us down before
  // any data arrives. This window is never drawn to (no handlers); it
  // exists purely to hold the loop open until prv_bg_exit pops it.
  s_bg_window = window_create();
  window_stack_push(s_bg_window, false);

  // Open AppMessage (triggers PKJS ready event)
  app_message_register_inbox_received(prv_inbox_received);
  app_message_register_inbox_dropped(prv_inbox_dropped);
  app_message_open(2048, 256);

  // Request weather
  comm_request_refresh();

  // Set 28-second timeout (OS kills us at ~30 seconds).
  // Increased from 25s to allow more time for geolocation + API fetch.
  s_bg_timeout_timer = app_timer_register(28000, prv_bg_timeout, NULL);
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
  
  int bg_interval = settings_get_background_interval();
  if (bg_interval > 0) {
    // Subscribe to wakeup events while app is running (backgrounded or foreground)
    wakeup_service_subscribe(prv_wakeup_handler);
    
    // Check if we have a persisted wakeup from previous launch
    bool wakeup_valid = false;
    if (persist_exists(PERSIST_KEY_WAKEUP_ID)) {
      s_wakeup_id = persist_read_int(PERSIST_KEY_WAKEUP_ID);
      
      // Verify it's still scheduled (not expired/cancelled)
      time_t wakeup_time = 0;
      if (wakeup_query(s_wakeup_id, &wakeup_time)) {
        wakeup_valid = true;
        time_t now = time(NULL);
        int secs_until = (int)(wakeup_time - now);
        APP_LOG(APP_LOG_LEVEL_INFO, "BG: Existing wakeup found (wid=%d, in %d secs)", 
                (int)s_wakeup_id, secs_until);
      } else {
        APP_LOG(APP_LOG_LEVEL_INFO, "BG: Persisted wakeup invalid, cleaning up");
        persist_delete(PERSIST_KEY_WAKEUP_ID);
        s_wakeup_id = -1;
      }
    }
    
    // Schedule new wakeup if none exists
    if (!wakeup_valid) {
      prv_reschedule_wakeup(true);
      APP_LOG(APP_LOG_LEVEL_INFO, "BG: Scheduling initial wakeup (%d secs)", bg_interval);
    }
  }

  // Seed the phone with the watch's card order + visibility on every launch,
  // so the Clay config page opens on the watch's true state. The debounce
  // (plus busy retries) keeps it clear of the 750ms initial refresh request.
  comm_send_card_state();
}

void comm_deinit(void) {
  app_message_deregister_callbacks();
  // Note: wakeup_service_unsubscribe() doesn't exist in Pebble SDK
  // Subscription is automatically cleaned up when app closes
}
