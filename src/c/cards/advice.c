#include "cards.h"
#include "../theme.h"
#include "../icons.h"
#include "../ui.h"
#include "../weather_data.h"
#include "../anim.h"
#include <stdio.h>
#include <time.h>

// Debug-only deterministic matrix case selector.
//
// Usage:
//   Set TW_ADVICE_MATRIX_CASE to a non-zero case id (1..15) to force
//   synthetic WeatherData for matrix validation. Keep at 0 for normal app.
//
// Case ids map to EDGE_CASE_MATRIX.md rows M01..M15.
#ifndef TW_ADVICE_MATRIX_CASE
#define TW_ADVICE_MATRIX_CASE 0
#endif

#if TW_ADVICE_MATRIX_CASE > 0
typedef struct {
  bool force_daytime_set;
  bool force_daytime;
  bool force_hour_set;
  int force_hour;
} AdviceDebugContext;

static AdviceDebugContext s_debug_ctx;

static const WeatherData *prv_effective_weather(const WeatherData *src,
                                                WeatherData *scratch) {
  *scratch = *src;
  s_debug_ctx.force_daytime_set = false;
  s_debug_ctx.force_daytime = false;
  s_debug_ctx.force_hour_set = false;
  s_debug_ctx.force_hour = 12;

  switch (TW_ADVICE_MATRIX_CASE) {
    case 1: // M01 stale packet
      scratch->valid = true;
      scratch->last_updated = (uint32_t)time(NULL) - (4u * 60u * 60u);
      break;
    case 2: // M02 invalid packet
      scratch->valid = false;
      break;
    case 3: // M03 active storm
      scratch->valid = true;
      scratch->condition = COND_STORM;
      scratch->rain_alert_min = -1;
      break;
    case 4: // M04 rain soon with wind
      scratch->valid = true;
      scratch->condition = COND_CLOUDY;
      scratch->rain_alert_min = 10;
      scratch->wind_speed = (scratch->units == UNITS_METRIC) ? 40 : 25;
      break;
    case 5: // M05 rain now near freezing
      scratch->valid = true;
      scratch->condition = COND_RAIN;
      scratch->rain_alert_min = -1;
      scratch->temp = (scratch->units == UNITS_METRIC) ? 1 : 33;
      scratch->feels_like = (scratch->units == UNITS_METRIC) ? 1 : 33;
      break;
    case 6: // M06 snow now
      scratch->valid = true;
      scratch->condition = COND_SNOW;
      scratch->rain_alert_min = -1;
      scratch->temp = (scratch->units == UNITS_METRIC) ? -2 : 28;
      break;
    case 7: // M07 hot stress
      scratch->valid = true;
      scratch->condition = COND_PARTLY_CLOUDY;
      scratch->rain_alert_min = -1;
      scratch->feels_like = (scratch->units == UNITS_METRIC) ? 34 : 95;
      break;
    case 8: // M08 cold stress
      scratch->valid = true;
      scratch->condition = COND_PARTLY_CLOUDY;
      scratch->rain_alert_min = -1;
      scratch->feels_like = (scratch->units == UNITS_METRIC) ? 4 : 38;
      break;
    case 9: // M09 late-night cool fallback
      scratch->valid = true;
      scratch->condition = COND_PARTLY_CLOUDY;
      scratch->rain_alert_min = -1;
      scratch->feels_like = (scratch->units == UNITS_METRIC) ? 12 : 53;
      s_debug_ctx.force_hour_set = true;
      s_debug_ctx.force_hour = 23;
      s_debug_ctx.force_daytime_set = true;
      s_debug_ctx.force_daytime = false;
      break;
    case 10: // M10 high wind only
      scratch->valid = true;
      scratch->condition = COND_PARTLY_CLOUDY;
      scratch->rain_alert_min = -1;
      scratch->temp = (scratch->units == UNITS_METRIC) ? 18 : 66;
      scratch->feels_like = (scratch->units == UNITS_METRIC) ? 18 : 66;
      scratch->wind_speed = (scratch->units == UNITS_METRIC) ? 36 : 23;
      scratch->aqi = 40;
      scratch->uv = 4;
      scratch->humidity = 55;
      break;
    case 11: // M11 daytime severe UV
      scratch->valid = true;
      scratch->condition = COND_SUNNY;
      scratch->rain_alert_min = -1;
      scratch->temp = (scratch->units == UNITS_METRIC) ? 24 : 75;
      scratch->feels_like = (scratch->units == UNITS_METRIC) ? 24 : 75;
      scratch->uv = 10;
      scratch->aqi = 40;
      scratch->wind_speed = (scratch->units == UNITS_METRIC) ? 12 : 7;
      scratch->humidity = 50;
      s_debug_ctx.force_daytime_set = true;
      s_debug_ctx.force_daytime = true;
      s_debug_ctx.force_hour_set = true;
      s_debug_ctx.force_hour = 13;
      break;
    case 12: // M12 bad air and muggy
      scratch->valid = true;
      scratch->condition = COND_CLOUDY;
      scratch->rain_alert_min = -1;
      scratch->aqi = 140;
      scratch->humidity = 85;
      scratch->temp = (scratch->units == UNITS_METRIC) ? 27 : 80;
      scratch->feels_like = (scratch->units == UNITS_METRIC) ? 24 : 75;
      scratch->wind_speed = (scratch->units == UNITS_METRIC) ? 12 : 7;
      scratch->uv = 3;
      break;
    case 13: // M13 pleasant daytime mild
      scratch->valid = true;
      scratch->condition = COND_PARTLY_CLOUDY;
      scratch->rain_alert_min = -1;
      scratch->temp = (scratch->units == UNITS_METRIC) ? 22 : 72;
      scratch->feels_like = (scratch->units == UNITS_METRIC) ? 22 : 72;
      scratch->aqi = 40;
      scratch->uv = 4;
      scratch->humidity = 55;
      scratch->wind_speed = (scratch->units == UNITS_METRIC) ? 14 : 9;
      s_debug_ctx.force_daytime_set = true;
      s_debug_ctx.force_daytime = true;
      s_debug_ctx.force_hour_set = true;
      s_debug_ctx.force_hour = 13;
      break;
    case 14: // M14 pleasant nighttime mild
      scratch->valid = true;
      scratch->condition = COND_PARTLY_CLOUDY;
      scratch->rain_alert_min = -1;
      scratch->temp = (scratch->units == UNITS_METRIC) ? 18 : 64;
      scratch->feels_like = (scratch->units == UNITS_METRIC) ? 18 : 64;
      scratch->aqi = 40;
      scratch->uv = 0;
      scratch->humidity = 55;
      scratch->wind_speed = (scratch->units == UNITS_METRIC) ? 12 : 7;
      s_debug_ctx.force_daytime_set = true;
      s_debug_ctx.force_daytime = false;
      s_debug_ctx.force_hour_set = true;
      s_debug_ctx.force_hour = 23;
      break;
    case 15: // M15 pleasant daytime cool
      scratch->valid = true;
      scratch->condition = COND_PARTLY_CLOUDY;
      scratch->rain_alert_min = -1;
      scratch->temp = (scratch->units == UNITS_METRIC) ? 13 : 56;
      scratch->feels_like = (scratch->units == UNITS_METRIC) ? 11 : 52;
      scratch->aqi = 40;
      scratch->uv = 4;
      scratch->humidity = 55;
      scratch->wind_speed = (scratch->units == UNITS_METRIC) ? 12 : 7;
      s_debug_ctx.force_daytime_set = true;
      s_debug_ctx.force_daytime = true;
      s_debug_ctx.force_hour_set = true;
      s_debug_ctx.force_hour = 13;
      break;
    default:
      break;
  }

  return scratch;
}
#else
static const WeatherData *prv_effective_weather(const WeatherData *src,
                                                WeatherData *scratch) {
  (void)scratch;
  return src;
}
#endif

// "Touch & Go" advice card.
//
// Classifies the current weather into one of N tiers, then picks a phrase
// from that tier's pool. Phrase selection is seeded by `last_updated` so
// the line stays stable for the duration of a refresh cycle (no flicker
// while the user stares at the card) and rotates organically when fresh
// weather data arrives.
//
// All phrases are stored locally — no remote calls, no persistence. The
// tier classifier is priority-ordered: the first matching tier wins.

typedef enum {
  ADV_STORM = 0,
  ADV_RAIN_SOON,
  ADV_RAIN_NOW,
  ADV_SNOW,
  ADV_HOT,
  ADV_COLD,
  ADV_WIND,
  ADV_HIGH_UV,
  ADV_BAD_AIR,
  ADV_MUGGY,
  ADV_DATA_STALE,
  ADV_PLEASANT,
  ADV_TIER_COUNT,
} AdviceTier;

typedef struct {
  const char *label;
  const char *const *phrases;
  int phrase_count;
} AdviceTierDef;

// ---- Phrase pools (~10 per tier). Practical-with-a-wink voice. ----

static const char *const PHRASES_STORM[] = {
  "Storm overhead. Stay inside and off the roof.",
  "Lightning is present. Reschedule outdoor plans.",
  "Sky's throwing a tantrum. Best to wait it out.",
  "Thunder's active. Good day to stay in with a book.",
  "Active storm. Best to stay inside and wait it out.",
  "Cancel the picnic. The sky said no.",
  "Electrical storm active. Don't be a conductor.",
  "Heavy skies ahead. Find shelter soon.",
  "Big thunder nearby. Keep everyone inside."
};

static const char *const PHRASES_RAIN_SOON[] = {
  "Rain incoming. This is your last call for sunshine.",
  "Rain's about to start. Head for cover soon.",
  "Wet weather inbound. Time to grab the jacket.",
  "Heads up, rain is close. Beat it indoors.",
  "Pop-up shower in 15. The pavement will shine.",
  "Rain is on the way. Keep a hood handy.",
  "Window's closing. Move fast.",
  "Sky says hurry. Or get wet, your choice.",
  "Damp incoming. Last dry minutes of the hour."
};

static const char *const PHRASES_RAIN_NOW[] = {
  "It is raining. Embrace the wet or escape it.",
  "Active drizzle. Boots beat sneakers today.",
  "Currently wet outside. Plan for a slower trip.",
  "Sky's washing the world. Dodge it if you can.",
  "Slick streets. Slower steps.",
  "Wet phone risk: high. Pocket it safely.",
  "It's pouring. Best to keep warm and stay dry.",
  "Rain mode engaged. Umbrellas are mandatory.",
  "Outside is wet. Inside is perfectly fine."
};

static const char *const PHRASES_RAIN_COLD[] = {
  "Cold rain active. Surfaces can turn slick fast.",
  "Near-freezing rain. Slow steps, better traction.",
  "Cold drizzle outside. Keep feet warm and dry.",
  "Wet plus cold today. Extra caution on sidewalks.",
  "Chilly rain mode. Waterproof layers pay off.",
  "Cold rain and wind. Keep hands covered.",
  "Cold rain with an icy feel. Watch curbs.",
  "Rain is cold enough to bite. Bundle and hood up.",
  "Cold wet outside. Prioritize grip over speed."
};

static const char *const PHRASES_SNOW[] = {
  "Snow falling. Layer up and walk slow.",
  "Slippery world outside. Mind the curbs.",
  "Cold and snowy out. Drive slowly and leave room.",
  "Boots, gloves, patience. In that exact order.",
  "Snow stacking up. Boots are not optional.",
  "Slush is the new ice. Step lighter.",
  "Hot drink mandatory. It's a snow day rule.",
  "Sky is quiet, ground is loud. Wear layers.",
  "It's snowing. Roads may be slick, so take it slow."
};

static const char *const PHRASES_HOT[] = {
  "Hydrate or wilt. It is cooking out there.",
  "It is hot. The car will be hotter.",
  "Heat advisory active. Better to stay inside today.",
  "Sweat is the new accessory. Hydrate.",
  "Pavement is literally lava. Choose grass.",
  "Stay cool. Literally and figuratively.",
  "Heat wave conditions. Slow is fast today.",
  "The sun is intense. Find some shade.",
  "Air conditioning will earn its keep today. Stay cool."
};

static const char *const PHRASES_COLD[] = {
  "Toe-numbing cold. Cover your extremities.",
  "Layers are not a suggestion. Bundle up safely.",
  "It's cold. Your nose already knows the truth.",
  "Very cold out. Hat now, thank yourself later.",
  "Winter said prove it. Wear heavy layers.",
  "Bone-cold outside. Hot bath waiting at home.",
  "Wind chill bites. Two pairs of socks is wisdom.",
  "Frost on everything. Plan extra driving time.",
  "Keep hands warm and take your time outside."
};

static const char *const PHRASES_WIND[] = {
  "Hold your hat. Wind is in charge today.",
  "Blustery conditions. Tie down loose stuff.",
  "Hair plans? Canceled. Lean into the wind.",
  "Strong gusts today. Keep your balance outdoors.",
  "Door-slam day. Hold the handle tightly.",
  "Wind makes mild cold feel completely arctic.",
  "Trash bag flight risk. Secure all outdoor items.",
  "Watch the trees. They'll tell on the wind.",
  "Crosswinds bite. Drive slower on highways."
};

static const char *const PHRASES_HIGH_UV[] = {
  "Sunscreen isn't optional. Bright burn risk today.",
  "UV is very high today. Cover up well.",
  "Sun is judging. So is your sensitive skin.",
  "Strong sun today. Strong shade preference required.",
  "Reapply your sunscreen often today.",
  "UV index severe. Burns happen in 15 minutes.",
  "Hat over hood. Wide brim always wins here.",
  "Shade will be limited. Plan outdoor time carefully.",
  "Sunglasses help today. They're more than style."
};

static const char *const PHRASES_BAD_AIR[] = {
  "Poor air today. Indoor exercise is the better call.",
  "Lungs prefer indoors today. Keep windows fully shut.",
  "Air feels thick. Trust the AQI gauge.",
  "Mask up if sensitive. Today's a stay-in day.",
  "Outdoor workout gently postponed. Do not push it.",
  "If air bothers you, spending time indoors helps.",
  "Air quality is rough today. Check your filter.",
  "Eyes burning is your body's early warning.",
  "Smoke or smog? Yes. Move your plans inside."
};

static const char *const PHRASES_MUGGY[] = {
  "Very humid today. Expect a sticky walk outside.",
  "Sticky conditions. Shower-after-walking territory.",
  "Air is thick today. Move slow and hydrate.",
  "Humidity says hi. Very, very loudly.",
  "Towel in the bag. Always carry one today.",
  "Light, breathable clothes will feel better today.",
  "Humidity is high. Expect hair and clothes to cling.",
  "Tropical conditions. Without the actual beach.",
  "It feels heavier than the temperature suggests."
};

static const char *const PHRASES_PLEASANT_DAY[] = {
  "Just nice out. Go safely enjoy something.",
  "Boring weather. This is officially the good kind.",
  "Everything feels nicely balanced today. Enjoy it.",
  "If you head out, conditions are on your side.",
  "All systems fully go. Step boldly outside.",
  "Friendly sky above. Go be friendly right back.",
  "Picnic-grade conditions. Grab your basket and run.",
  "The kind of day weather apps totally brag about.",
  "No weather blockers in sight. Plans are a go."
};

// Night-safe pleasant set. Keep these neutral and avoid "go outside now"
// directives that can be wrong late at night.
static const char *const PHRASES_PLEASANT_NIGHT[] = {
  "Clear and quiet outside. Good night to wind down.",
  "Calm skies overhead. The evening looks settled.",
  "Night is steady and dry. Low drama weather.",
  "Quiet conditions. A restful evening fits well.",
  "Stable night weather. Nothing urgent outside.",
  "Skies are calm. A cozy plan wins tonight.",
  "Clear late hours. No weather chaos in sight.",
  "Evening air is settled. Keep plans low-stress.",
  "Night forecast is calm. Choose what feels comfortable."
};

// Daytime but cool-safe pleasant set. Encourages layers instead of implying
// it is universally ideal.
static const char *const PHRASES_PLEASANT_COOL[] = {
  "Clear but brisk. Bring a jacket if you head out.",
  "Dry and cool today. Layers make it nicer.",
  "Steady sky, cooler air. Light jacket weather.",
  "Calm conditions with a chill. Dress for comfort.",
  "Looks nice, feels cool. Add one extra layer.",
  "Quiet weather, cooler temps. Easy layer day.",
  "Sky is friendly, air is brisk. Plan accordingly.",
  "Good visibility, cool feel. Keep sleeves handy.",
  "Nothing severe, just cool. Jacket still smart."
};

static const char *const PHRASES_DATA_STALE[] = {
  "Forecast is aging out. Pull down to refresh.",
  "This snapshot may be stale. Grab a fresh update.",
  "Data is older than ideal. Refresh before planning.",
  "Weather feed may be behind. Quick refresh recommended.",
  "Advisory confidence is reduced. Update for accuracy.",
  "Conditions may have shifted. Refresh for an update.",
  "This weather update is old. Refresh for the latest.",
  "This reading may be stale. Pull to refresh.",
  "Forecast is getting old. Refresh for confidence."
};

#define POOL(arr) (arr), (sizeof(arr) / sizeof((arr)[0]))

static const AdviceTierDef TIERS[ADV_TIER_COUNT] = {
  [ADV_STORM]     = { "STORM",     POOL(PHRASES_STORM) },
  [ADV_RAIN_SOON] = { "RAIN SOON", POOL(PHRASES_RAIN_SOON) },
  [ADV_RAIN_NOW]  = { "RAINY",     POOL(PHRASES_RAIN_NOW) },
  [ADV_SNOW]      = { "SNOW",      POOL(PHRASES_SNOW) },
  [ADV_HOT]       = { "HOT",       POOL(PHRASES_HOT) },
  [ADV_COLD]      = { "COLD",      POOL(PHRASES_COLD) },
  [ADV_WIND]      = { "WINDY",     POOL(PHRASES_WIND) },
  [ADV_HIGH_UV]   = { "HIGH UV",   POOL(PHRASES_HIGH_UV) },
  [ADV_BAD_AIR]   = { "BAD AIR",   POOL(PHRASES_BAD_AIR) },
  [ADV_MUGGY]     = { "MUGGY",     POOL(PHRASES_MUGGY) },
  [ADV_DATA_STALE]= { "CHECK",     POOL(PHRASES_DATA_STALE) },
  [ADV_PLEASANT]  = { "NICE",      POOL(PHRASES_PLEASANT_DAY) },
};

#undef POOL

// One-shot audit: on first draw of this card we measure every phrase
// against the actual body rect on the live platform and log a warning
// if any wraps tall enough to clip. Authoring guideline is <= 55 chars
// (verified per-platform via the original probe pass on emery + gabbro;
// at 55 chars on emery the rendered block is ~72px tall vs a 110px
// body budget, leaving comfortable slack). Run is essentially free
// (one pass of ~330 layout calculations on first card draw).
static bool s_audited = false;

static bool prv_now_local_hour(int *hour_out) {
#if TW_ADVICE_MATRIX_CASE > 0
  if (s_debug_ctx.force_hour_set) {
    if (hour_out) *hour_out = s_debug_ctx.force_hour;
    return true;
  }
#endif

  time_t now = time(NULL);
  struct tm *lt = localtime(&now);
  if (!lt) return false;
  if (hour_out) *hour_out = lt->tm_hour;
  return true;
}

static bool prv_parse_12h_minutes(const char *s, int *minutes_out) {
  if (!s || !*s) return false;
  const char *p = s;

  int h = 0;
  int h_digits = 0;
  while (*p >= '0' && *p <= '9' && h_digits < 2) {
    h = h * 10 + (*p - '0');
    p++;
    h_digits++;
  }
  if (h_digits == 0 || *p != ':') return false;
  p++;

  if (!(p[0] >= '0' && p[0] <= '9' && p[1] >= '0' && p[1] <= '9')) return false;
  int m = (p[0] - '0') * 10 + (p[1] - '0');
  p += 2;

  if (*p == ' ') p++;
  if (!(*p == 'A' || *p == 'a' || *p == 'P' || *p == 'p')) return false;
  bool is_pm = (*p == 'P' || *p == 'p');
  p++;
  if (!(*p == 'M' || *p == 'm')) return false;

  if (h < 1 || h > 12 || m < 0 || m > 59) return false;
  int h24 = h % 12;
  if (is_pm) h24 += 12;
  if (minutes_out) *minutes_out = h24 * 60 + m;
  return true;
}

static bool prv_is_daytime(const WeatherData *d) {
#if TW_ADVICE_MATRIX_CASE > 0
  if (s_debug_ctx.force_daytime_set) {
    return s_debug_ctx.force_daytime;
  }
#endif

  time_t now = time(NULL);
  struct tm *lt = localtime(&now);
  if (!lt) return true;
  int now_min = lt->tm_hour * 60 + lt->tm_min;

  int sunrise_min = 0;
  int sunset_min = 0;
  if (prv_parse_12h_minutes(d->sunrise, &sunrise_min) &&
      prv_parse_12h_minutes(d->sunset, &sunset_min) &&
      sunrise_min < sunset_min) {
    return now_min >= sunrise_min && now_min < sunset_min;
  }

  // Fallback if sunrise/sunset are unavailable.
  return lt->tm_hour >= 7 && lt->tm_hour < 20;
}

static int prv_normalized_temp(const WeatherData *d) {
  int t = d->temp;
  if (t < -80) t = -80;
  if (t > 180) t = 180;
  return t;
}

static int prv_normalized_feels_like(const WeatherData *d) {
  int f = d->feels_like;
  if (f < -80 || f > 180) f = d->temp;
  if (f < -80) f = -80;
  if (f > 180) f = 180;
  return f;
}

static bool prv_is_chilly(const WeatherData *d) {
  // "Pleasant but cool" threshold intentionally sits above the hard cold
  // threshold so the cool-safe phrase pool can be exercised without
  // classifying as ADV_COLD.
  const int cool = (d->units == UNITS_METRIC) ? 13 : 55;
  return prv_normalized_feels_like(d) <= cool;
}

static bool prv_is_freezingish_rain(const WeatherData *d) {
  const int freezeish = (d->units == UNITS_METRIC) ? 1 : 34;
  return prv_normalized_feels_like(d) <= freezeish;
}

static bool prv_is_stale_data(const WeatherData *d) {
  if (!d->valid) return true;
  if (d->last_updated == 0) return false;

  time_t now = time(NULL);
  if (now <= 0) return false;
  if ((uint32_t)now <= d->last_updated) return false;
  return ((uint32_t)now - d->last_updated) > (3u * 60u * 60u);
}

static int prv_clamp_int(int v, int lo, int hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

static const char *const *prv_phrase_pool_for_tier(const WeatherData *d,
                                                    AdviceTier tier,
                                                    int *count_out) {
  if (tier == ADV_RAIN_NOW && prv_is_freezingish_rain(d)) {
    if (count_out) *count_out = (int)(sizeof(PHRASES_RAIN_COLD) /
                                      sizeof(PHRASES_RAIN_COLD[0]));
    return PHRASES_RAIN_COLD;
  }

  if (tier != ADV_PLEASANT) {
    const AdviceTierDef *def = &TIERS[tier];
    if (count_out) *count_out = def->phrase_count;
    return def->phrases;
  }

  if (!prv_is_daytime(d)) {
    if (count_out) *count_out = (int)(sizeof(PHRASES_PLEASANT_NIGHT) /
                                      sizeof(PHRASES_PLEASANT_NIGHT[0]));
    return PHRASES_PLEASANT_NIGHT;
  }

  if (prv_is_chilly(d)) {
    if (count_out) *count_out = (int)(sizeof(PHRASES_PLEASANT_COOL) /
                                      sizeof(PHRASES_PLEASANT_COOL[0]));
    return PHRASES_PLEASANT_COOL;
  }

  if (count_out) *count_out = (int)(sizeof(PHRASES_PLEASANT_DAY) /
                                    sizeof(PHRASES_PLEASANT_DAY[0]));
  return PHRASES_PLEASANT_DAY;
}

static void prv_audit_phrases(GRect body_r) {
  GFont f = ui_font_body();
  for (int t = 0; t < ADV_TIER_COUNT; ++t) {
    const AdviceTierDef *def = &TIERS[t];
    for (int i = 0; i < def->phrase_count; ++i) {
      GSize s = graphics_text_layout_get_content_size(
          def->phrases[i], f,
          GRect(0, 0, body_r.size.w, 500),
          GTextOverflowModeWordWrap, GTextAlignmentCenter);
      if (s.h > body_r.size.h) {
        APP_LOG(APP_LOG_LEVEL_WARNING,
                "[advice] OVERFLOW tier=%d idx=%d h=%d>max=%d: \"%s\"",
                t, i, s.h, body_r.size.h, def->phrases[i]);
      }
    }
  }

  const char *const *pleasant_sets[] = {
    PHRASES_PLEASANT_DAY,
    PHRASES_PLEASANT_NIGHT,
    PHRASES_PLEASANT_COOL,
    PHRASES_RAIN_COLD,
  };
  const int pleasant_counts[] = {
    (int)(sizeof(PHRASES_PLEASANT_DAY) / sizeof(PHRASES_PLEASANT_DAY[0])),
    (int)(sizeof(PHRASES_PLEASANT_NIGHT) / sizeof(PHRASES_PLEASANT_NIGHT[0])),
    (int)(sizeof(PHRASES_PLEASANT_COOL) / sizeof(PHRASES_PLEASANT_COOL[0])),
    (int)(sizeof(PHRASES_RAIN_COLD) / sizeof(PHRASES_RAIN_COLD[0])),
  };

  for (int s = 0; s < 4; ++s) {
    for (int i = 0; i < pleasant_counts[s]; ++i) {
      GSize sz = graphics_text_layout_get_content_size(
          pleasant_sets[s][i], f,
          GRect(0, 0, body_r.size.w, 500),
          GTextOverflowModeWordWrap, GTextAlignmentCenter);
      if (sz.h > body_r.size.h) {
        APP_LOG(APP_LOG_LEVEL_WARNING,
                "[advice] OVERFLOW pleasant_set=%d idx=%d h=%d>max=%d: \"%s\"",
                s, i, sz.h, body_r.size.h, pleasant_sets[s][i]);
      }
    }
  }
}

// Classify weather into a tier. Priority-ordered, first match wins.
//
// Conflict-priority rubric:
//   1) Confidence gate
//      - stale/invalid packet -> ADV_DATA_STALE
//   2) Immediate precip/severe hazard
//      - storm -> rain soon -> rain now -> snow
//      (Precip wins over ambient stress so we do not advise "pleasant/windy"
//       while active/near-term precipitation is the core risk.)
//   3) Thermal stress
//      - hot -> cold -> late-night cool fallback
//   4) Exposure stress
//      - high wind -> high UV (daytime only) -> bad air -> muggy
//   5) Fallback
//      - pleasant
static AdviceTier prv_classify(const WeatherData *d) {
  if (prv_is_stale_data(d)) return ADV_DATA_STALE;

  // Normalize once, compare everywhere against normalized values only.
  int temp = prv_normalized_temp(d);
  int feels_like = prv_normalized_feels_like(d);
  int wind_speed = prv_clamp_int(d->wind_speed, 0, 200);
  int humidity = prv_clamp_int(d->humidity, 0, 100);
  int aqi = prv_clamp_int(d->aqi, 0, 500);

  const bool metric = (d->units == UNITS_METRIC);
  int hot_thresh = metric ? 30 : 86;
  int chilly_thresh = metric ? 7 : 45;
  int muggy_temp_thresh = metric ? 21 : 70;
  int wind_alert_thresh = metric ? 32 : 20;       // 20 mph ~= 32 km/h
  int late_night_cool_thresh = metric ? 13 : 55;  // cool but not freezing

  bool daytime = prv_is_daytime(d);
  int hour = 12;
  bool have_hour = prv_now_local_hour(&hour);

  // Trigger map (documented booleans make tie-break behavior explicit).
  bool trig_storm = (d->condition == COND_STORM);
  bool trig_rain_soon = (d->rain_alert_min >= 0 && d->rain_alert_min <= 30);
  bool trig_rain_now = (d->condition == COND_RAIN);
  bool trig_snow = (d->condition == COND_SNOW);
  bool trig_hot = (feels_like >= hot_thresh);
  bool trig_cold = (feels_like <= chilly_thresh);
  bool trig_late_night_cool = (!daytime && have_hour &&
                               (hour >= 22 || hour <= 5) &&
                               feels_like <= late_night_cool_thresh);
  bool trig_wind = (wind_speed >= wind_alert_thresh);
  // High-UV trigger uses today's forecast peak (uv_max) rather than the
  // current UV. With current-UV semantics in `d->uv`, evaluating against
  // the live value would only surface this advice during the brief midday
  // peak window. Using uv_max ensures the advice shows on a high-UV day
  // any time the user checks during daylight.
  int uv_peak = prv_clamp_int(d->uv_max > 0 ? d->uv_max : d->uv, 0, 15);
  bool trig_high_uv = (daytime && uv_peak >= 8);
  bool trig_bad_air = (aqi >= 100);
  bool trig_muggy = (humidity >= 75 && temp >= muggy_temp_thresh);

  if (trig_storm) return ADV_STORM;
  if (trig_rain_soon) return ADV_RAIN_SOON;
  if (trig_rain_now) return ADV_RAIN_NOW;
  if (trig_snow) return ADV_SNOW;

  if (trig_hot) return ADV_HOT;
  if (trig_cold || trig_late_night_cool) return ADV_COLD;

  if (trig_wind) return ADV_WIND;
  if (trig_high_uv) return ADV_HIGH_UV;
  if (trig_bad_air) return ADV_BAD_AIR;
  if (trig_muggy) return ADV_MUGGY;
  return ADV_PLEASANT;
}

// Tier-appropriate header icon.
static void prv_draw_tier_icon(GContext *ctx, GPoint c, int size,
                               GColor color, AdviceTier tier) {
  switch (tier) {
    case ADV_STORM:     icon_draw_lightning(ctx, c, size, color); break;
    case ADV_RAIN_SOON:
    case ADV_RAIN_NOW:  icon_draw_cloud_rain(ctx, c, size, color, color); break;
    case ADV_SNOW:      icon_draw_snow(ctx, c, size, color); break;
    case ADV_HOT:
    case ADV_HIGH_UV:
    case ADV_PLEASANT:  icon_draw_sun(ctx, c, size, color); break;
    case ADV_COLD:      icon_draw_snow(ctx, c, size, color); break;
    case ADV_WIND:      icon_draw_wind(ctx, c, size, color); break;
    case ADV_BAD_AIR:   icon_draw_pulse(ctx, c, size, color); break;
    case ADV_MUGGY:     icon_draw_droplet(ctx, c, size, color); break;
    case ADV_DATA_STALE: icon_draw_clock(ctx, c, size, color); break;
    default:            icon_draw_sun(ctx, c, size, color); break;
  }
}

// Tier-appropriate accent color for the badge below the header.
static GColor prv_tier_color(AdviceTier tier) {
  switch (tier) {
    case ADV_STORM:
    case ADV_HOT:
    case ADV_HIGH_UV:    return theme_accent_orange();
    case ADV_RAIN_SOON:
    case ADV_RAIN_NOW:
    case ADV_SNOW:
    case ADV_COLD:
    case ADV_BAD_AIR:
    case ADV_MUGGY:      return theme_accent_blue();
    case ADV_DATA_STALE: return theme_fg();
    case ADV_PLEASANT:   return theme_accent_advice();  // echo the card's identity color
    case ADV_WIND:
    default:             return theme_fg();
  }
}

// Generates a short, data-driven headline for the tier that triggered.
// The headline is the "why" — the specific metric that caused this tier
// to win — displayed in accent color above the quip.
static void prv_generate_tier_headline(AdviceTier tier, const WeatherData *d,
                                       char *buf, size_t buf_size) {
  int temp = prv_normalized_temp(d);
  int feels_like = prv_normalized_feels_like(d);

  switch (tier) {
    case ADV_STORM:
      snprintf(buf, buf_size, "STORM OVERHEAD");
      break;
    case ADV_RAIN_SOON:
      snprintf(buf, buf_size, "RAIN IN %d MINS", d->rain_alert_min);
      break;
    case ADV_RAIN_NOW:
      if (prv_is_freezingish_rain(d)) {
        snprintf(buf, buf_size, "COLD RAIN / %d\xc2\xb0", temp);
      } else {
        snprintf(buf, buf_size, "RAINING / %d\xc2\xb0", temp);
      }
      break;
    case ADV_SNOW:
      snprintf(buf, buf_size, "SNOWING / %d\xc2\xb0", temp);
      break;
    case ADV_HOT:
      snprintf(buf, buf_size, "FEELS LIKE %d\xc2\xb0", feels_like);
      break;
    case ADV_COLD:
      snprintf(buf, buf_size, "FEELS LIKE %d\xc2\xb0", feels_like);
      break;
    case ADV_WIND:
      if (d->units == UNITS_METRIC) {
        snprintf(buf, buf_size, "%d KMH GUSTS", d->wind_speed);
      } else {
        snprintf(buf, buf_size, "%d MPH GUSTS", d->wind_speed);
      }
      break;
    case ADV_HIGH_UV:
      // Match the trigger: show today's peak so the advice subtitle
      // makes sense even when current UV has dropped off.
      snprintf(buf, buf_size, "UV INDEX: %d",
               d->uv_max > 0 ? d->uv_max : d->uv);
      break;
    case ADV_BAD_AIR:
      snprintf(buf, buf_size, "AQI: %d (POOR)", d->aqi);
      break;
    case ADV_MUGGY:
      snprintf(buf, buf_size, "%d%% HUMIDITY", d->humidity);
      break;
    case ADV_DATA_STALE:
      snprintf(buf, buf_size, "DATA MAY BE STALE");
      break;
    case ADV_PLEASANT:
    default:
      if (!prv_is_daytime(d)) {
        snprintf(buf, buf_size, "NIGHT / %d\xc2\xb0", temp);
      } else {
        snprintf(buf, buf_size, "%d\xc2\xb0 & CLEAR", temp);
      }
      break;
  }
}

void card_advice_draw(GContext *ctx, GRect bounds) {
  WeatherData *live = weather_data_get();
  WeatherData scenario;
  const WeatherData *d = prv_effective_weather(live, &scenario);
  int W = bounds.size.w;

  AdviceTier tier = prv_classify(d);
  const AdviceTierDef *def = &TIERS[tier];
  GColor accent = prv_tier_color(tier);

  // Header: distinct identity icon + color so this card never reads as
  // "Air Quality". Tap icon + mustard yellow are unique to Touch & Go.
  int header_y = UI_HEADER_Y;
  ui_draw_card_header_with_icon(ctx, bounds, "TOUCH & GO",
                                theme_fg(),
                                header_y, 18, icon_draw_tap);

  // Tier badge (icon + tier name) — small, right under the header.
  int badge_y = header_y + UI_HEADER_HEIGHT + PBL_IF_ROUND_ELSE(8, 4);
  GFont badge_font = ui_font_label();
  GSize bsize = graphics_text_layout_get_content_size(def->label, badge_font,
      GRect(0, 0, W, 20), GTextOverflowModeTrailingEllipsis,
      GTextAlignmentLeft);
  int badge_icon = 16;
  int badge_gap = 5;
  int badge_total = badge_icon + badge_gap + bsize.w;
  int badge_x = bounds.origin.x + (W - badge_total) / 2;
  prv_draw_tier_icon(ctx, GPoint(badge_x + badge_icon/2, badge_y + 9),
                     badge_icon, accent, tier);
  graphics_context_set_text_color(ctx, accent);
  graphics_draw_text(ctx, def->label, badge_font,
      GRect(badge_x + badge_icon + badge_gap, badge_y, bsize.w + 4, 20),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  // Body phrase. Pick stable-per-refresh: seed by last_updated so the
  // same line shows for the entire session and refreshes when weather
  // data refreshes. Fall back to time-bucketed minute if never refreshed.
  uint32_t seed = d->last_updated ? d->last_updated
                                  : (uint32_t)(time(NULL) / 60);
  int phrase_count = 0;
  const char *const *phrase_pool = prv_phrase_pool_for_tier(d, tier,
                                                            &phrase_count);
  if (phrase_count <= 0 || !phrase_pool) {
    phrase_pool = def->phrases;
    phrase_count = def->phrase_count;
  }
  int idx = (int)(seed % (uint32_t)phrase_count);
  const char *phrase = phrase_pool[idx];

  // Two-Tone layout:
  //   ┌──────────────────────────────────┐
  //   │  headline  ← accent color, 18pt  │  ~20px
  //   │  quip      ← fg color, 24pt      │  remaining
  //   └──────────────────────────────────┘
  // The headline is the triggering metric (e.g. "FEELS LIKE 97°") so the
  // quip punches in context. Both zones sit between the badge and the
  // auto-banner / page-indicator safe zone at the bottom.
  int body_top = badge_y + 26;
  int body_bot = bounds.origin.y + bounds.size.h
                 - PBL_IF_ROUND_ELSE(70, 56);  // banner + indicator zone
  int ox = bounds.origin.x;

  // --- Headline row (accent-colored, Gothic 18 Bold) ---
  int headline_h = 22;
  GRect headline_r = GRect(ox + UI_MARGIN_X, body_top,
                           W - 2 * UI_MARGIN_X, headline_h);
  char headline_buf[32];
  prv_generate_tier_headline(tier, d, headline_buf, sizeof(headline_buf));
  graphics_context_set_text_color(ctx, accent);
  graphics_draw_text(ctx, headline_buf,
      ui_font_header(),
      headline_r, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // --- Quip row (fg color, Gothic 24 Bold) ---
  int quip_top = body_top + headline_h + 4;
  int quip_h   = body_bot - quip_top;
  GRect quip_r = GRect(ox + UI_MARGIN_X, quip_top,
                       W - 2 * UI_MARGIN_X, quip_h);
  if (!s_audited) { prv_audit_phrases(quip_r); s_audited = true; }
  graphics_context_set_text_color(ctx, theme_fg());
  graphics_draw_text(ctx, phrase,
      ui_font_body(),
      quip_r, GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);

  // Standard auto status banner so this card stays consistent with peers.
  ui_draw_auto_banner(ctx, bounds, d->rain_alert_min, d->last_updated,
                      anim_get_frame());
}
