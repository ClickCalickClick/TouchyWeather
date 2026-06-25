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
  int precip[5];       // 0..100 % for now / +1h / +2h / +3h / +4h
  int uv;              // 0..11+ — current UV (live)
  int uv_max;          // 0..11+ — today's forecast peak (for "PEAK n" subtitle)
  int aqi;             // US AQI 0..500
  char sunrise[8];     // "6:14 AM"
  char sunset[8];      // "7:45 PM"
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
} WeatherData;

void weather_data_init_mock(void);
WeatherData *weather_data_get(void);

const char *uv_label(int uv);
const char *aqi_label(int aqi);
