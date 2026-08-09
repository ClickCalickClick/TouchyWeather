#pragma once
#include <pebble.h>

typedef enum {
  COND_SUNNY = 0,
  COND_PARTLY_CLOUDY = 1,
  COND_CLOUDY = 2,
  COND_RAIN = 3,
  COND_SNOW = 4,
  COND_STORM = 5,
  COND_FOG = 6,
} WeatherCondition;

typedef enum {
  UNITS_IMPERIAL = 0,
  UNITS_METRIC = 1,
} Units;

// Wind speed unit, INDEPENDENT of `units` above. Requested on r/pebble by a
// Danish user for whom m/s is the everyday unit while °C is not in question —
// the two axes genuinely do not travel together outside the US/UK.
//
// This is deliberately NOT a third value on Units. `units` means the whole
// measurement system and is read at ~20 sites that are overwhelmingly about
// TEMPERATURE; a third value would need every one of them to treat it as
// metric, and one missed site renders the wrong scale with no visible tell.
//
// There is no AUTO member on purpose. The Clay select does offer Auto ("follow
// the measurement system"), but PKJS resolves it to a concrete unit before it
// builds the Open-Meteo URL, because the phone has to name the unit in the
// request anyway (wind_speed_unit=mph|kmh|ms). So the watch stores what it was
// actually sent and never consults `units` for wind — that is one branch fewer,
// not one more, which is the only reason this fits the RAM budget.
//
// MPH = 0 so the field's zero value is the pre-existing imperial behaviour.
typedef enum {
  WIND_UNITS_MPH = 0,
  WIND_UNITS_KMH = 1,
  WIND_UNITS_MS  = 2,
} WindUnits;

typedef struct {
  int temp;            // current, in selected unit
  int feels_like;
  int high;
  int low;
  WeatherCondition condition;
  int wind_speed;      // mph or km/h
  char wind_dir[4];    // "NW", "ENE", etc.
  int humidity;        // %
  int dew_point;       // °F or °C (matches `units`)
  bool use_dew_point;  // true → main card shows dew point instead of humidity
  bool show_location;  // true → main card shows the location name (Clay toggle)
  uint8_t wind_units;  // WindUnits — unit `wind_speed`/`hours_wind` arrive in.
                       // Sited HERE, wedged against the two bools, and it is
                       // worth knowing why: the next member is 4-aligned, so
                       // those two bools already sat in a word with two spare
                       // bytes. MEASURED both ways — sizeof is 444 with this
                       // field and 444 without it. Appended to the tail instead
                       // it would have cost 4 bytes of an app that has ~400 to
                       // its name.
  int precip[5];       // 0..100 % for now / +1h / +2h / +3h / +4h
  int uv;              // 0..11+ — current UV (live)
  int uv_max;          // 0..11+ — today's forecast peak (for "PEAK n" subtitle)
  int aqi;             // US AQI 0..500
  char sunrise[10];    // "6:14 AM" / "10:30 PM" (two-digit hour needs 9+NUL)
  char sunset[10];     // "7:45 PM" / "10:30 PM"
  char location_name[32]; // human-readable city, e.g. "San Francisco"
  int rain_alert_min;  // minutes until rain, -1 if none
  Units units;
  uint32_t last_updated; // unix seconds when last refresh was received
  bool valid;          // true once real or mock data populated

  // Phase 10A: Next 6 Hours card. Hours offset 1..6 from current hour.
  // Index 0 = +1h, index 5 = +6h. Hour 0 (now) lives on Main card.
  char hours_label[6][6];   // "2 PM", "11 AM", etc.
  int  hours_temp[6];
  WeatherCondition hours_cond[6];
  uint8_t hours_pop[6];     // precipitation probability 0..100
  int  hours_wind[6];       // wind speed in selected unit (mph/kmh)
  char hours_wind_dir[6][4];// "NW", "ENE", etc.
  int  hours_precip_x10[6]; // precip amount, tenths of in/mm (5 = 0.5)

  // Phase 10B: Week Ahead card. Day 0 = today, day 4 = today+4.
  // Day 0's high/low duplicate `high`/`low` above but are kept here
  // so the card draws uniformly.
  char days_label[5][4];    // "MON", "TUE", "WED", "THU", "FRI"
  int  days_high[5];
  int  days_low[5];
  WeatherCondition days_cond[5];
  uint8_t days_pop[5];      // max precipitation probability 0..100

  // Phase 7: Night Sky card. Phase 0..8 enum, illum 0..100, name like
  // "WAXING CRESCENT" or "FULL".
  uint8_t moon_phase;       // 0=NEW,1=WAX_CRESCENT,2=FIRST_Q,3=WAX_GIBBOUS,
                            // 4=FULL,5=WAN_GIBBOUS,6=LAST_Q,7=WAN_CRESCENT,8=NEW
  uint8_t moon_illum;       // 0..100
  char    moon_name1[10];   // "WAXING" / "FIRST" / "FULL" / "NEW" / "LAST" / "WANING"
  char    moon_name2[10];   // "GIBBOUS" / "QUARTER" / "CRESCENT" / "MOON" / ""

  // Phase 11: Golden Hour card. Four chronological milestones bracketing
  // the morning and evening "magic light" periods. All formatted "h:MM AM/PM".
  //   blue_am  = morning blue hour begins (sun at -6°, civil dawn)
  //   gold_am  = morning golden hour begins (sun at -0.833°, sunrise)
  //   gold_pm  = evening golden hour begins (sun at +6°)
  //   blue_pm  = evening blue hour begins  (sun at -0.833°, sunset)
  // (Evening blue hour ends at civil dusk; we don't store it separately.)
  // Buffer sizes accommodate "12:11 PM" (8 chars + null) plus a byte of
  // slack for safety.
  char    blue_am[10];
  char    gold_am[10];
  char    gold_pm[10];
  char    blue_pm[10];

  // Pollen severity 0..5 (Google Pollen API UPI scale), max of
  // grass/tree/weed indexes. -1 means "unknown / not covered" — the
  // air quality card should skip the pollen badge in that case.
  int pollen_level;

  // Phase 4 detail modals (UV TODAY / AIR DETAIL).
  //   hours_uv[6] — UV index for +1h..+6h, mirroring the 6 Hours window,
  //                 for the UV modal's hourly curve.
  //   pm2_5/pm10/o3/no2 — air-quality pollutant concentrations in µg/m³
  //                 (the US-AQI inputs) for the AIR DETAIL breakdown.
  //                 0 = missing/unavailable (Open-Meteo field was null).
  uint8_t hours_uv[6];
  int pm2_5;
  int pm10;
  int o3;
  int no2;
} WeatherData;

// 444 is MEASURED, not inherited: CLAUDE.md and comm.c both said ~480 for a
// long time and both were wrong. Do not restore that number.
//
// `wind_units` went into existing padding, so sizeof did NOT move — which means
// comm.c's size guard cannot see the change, and CACHE_LAYOUT_VERSION is the
// only thing standing between an old blob and a wind unit read out of a padding
// byte. If a later edit pushes the struct to a new size this fires; that is the
// signal the size guard has taken over and the layout version is no longer
// carrying the load alone. Emits no code.
_Static_assert(sizeof(WeatherData) == 444,
               "WeatherData changed size - revisit comm.c's cache guard");

// Put WeatherData into the honest "nothing known yet" state: every field
// zeroed, `valid` false, `last_updated` 0, and the two fields that already
// carry an explicit unknown sentinel (rain_alert_min, pollen_level) set to -1.
//
// This replaces the old weather_data_init_mock(), which filled the struct with
// a hardcoded IMPERIAL forecast (72°, San Francisco, "RAIN IN 15M", and a week
// of 78/58 · 72/60 · ...) and set valid = true. That mock reached the screen
// whenever no cache loaded — not only on a first-ever install but after every
// app update that bumped PERSIST_KEY_CACHE, which orphaned the user's blob. A
// metric user then saw five rows of unlabeled Fahrenheit on the Week card and
// reasonably read it as a units bug. See comm.c's key comment for the other
// half of that fix. The mock still exists for layout work, behind TW_MOCK_DATA
// (off), so it can never ship.
void weather_data_init_empty(void);

// True once a real reading — cached or live — has populated the struct.
// Cards MUST gate any rendered weather value on this: at false, every number
// in WeatherData is a zero, not a measurement, and drawing one fabricates a
// reading. Use ui_draw_awaiting_data() (or dashes) instead.
bool weather_data_has_reading(void);

WeatherData *weather_data_get(void);

const char *uv_label(int uv);
const char *aqi_label(int aqi);

// "MPH" / "KMH" / "M/S" for d->wind_units. ONE definition on purpose: this
// string was previously an inline ternary duplicated in main_card.c's Normal
// and Large layouts, and a third case in each is exactly the duplicated-helper
// shape that has produced three separate defects in this repo. Callers must not
// re-derive it from d->units.
const char *wind_unit_label(const WeatherData *d);
