// TouchyWeather PKJS - Open-Meteo fetcher
// No API key required. Fetches forecast + air-quality and posts AppMessage.

var Clay = require('@rebble/clay');
var clayConfig = require('./config');

// Runs inside the Clay config page (serialized with toSource — must be
// closure-free, ES5). Greys out everything that only applies while
// PhoneManagesCards is on: the per-card toggles, the drag-order list and
// HideSettingsCard. This mirrors comm.c's gate — without the grey-out, gated
// values a user changed while the gate was off would silently snap back to
// watch truth on the next open (the seed injection), which reads as a bug.
// HideSettingsCard is additionally forced OFF while ungated (the on-watch
// fail-safe AND makes it ineffective anyway; showing it checked would lie).
var clayCustomFn = function(minified) {
  var page = this;
  page.on(page.EVENTS.AFTER_BUILD, function() {
    var gate = page.getItemByMessageKey('PhoneManagesCards');
    if (!gate) return;
    // Small-screen watches (D1): the on-watch card editor is compiled out
    // there and the C side forces PhoneManagesCards on, so the phone is the
    // only card manager. Pin + hide the gate and leave every gated item
    // enabled. Unknown/null platform falls through to the stock gating, so
    // the large-screen (emery/gabbro) behavior cannot change.
    var info = page.meta && page.meta.activeWatchInfo;
    var pf = (info && info.platform) || '';
    if (pf === 'basalt' || pf === 'chalk' || pf === 'diorite' || pf === 'flint') {
      gate.set(true);
      gate.hide();
      return;
    }
    var gated = [
      'HideSettingsCard', 'CardOrder',
      'CardEnabledHours', 'CardEnabledWeek', 'CardEnabledPrecip',
      'CardEnabledUV', 'CardEnabledAQ', 'CardEnabledSun', 'CardEnabledNight',
      'CardEnabledGolden', 'CardEnabledRadar', 'CardEnabledAdvice'
    ];
    function sync() {
      var on = gate.get();
      for (var i = 0; i < gated.length; i++) {
        var item = page.getItemByMessageKey(gated[i]);
        if (!item || !item.enable || !item.disable) continue;
        if (on) {
          item.enable();
        } else {
          if (gated[i] === 'HideSettingsCard') item.set(false);
          item.disable();
        }
      }
    }
    gate.on('change', sync);
    sync();
  });
};

var clay = new Clay(clayConfig, clayCustomFn, { autoHandleEvents: false });
// Custom drag-to-reorder card list (must be registered before generateUrl).
clay.registerComponent(require('./cardorder'));

var COND = {
  SUNNY: 0, PARTLY_CLOUDY: 1, CLOUDY: 2, RAIN: 3, SNOW: 4, STORM: 5, FOG: 6
};

// Map Open-Meteo WMO weather codes to our internal enum.
function mapWeatherCode(code) {
  if (code === 0) return COND.SUNNY;
  if (code === 1 || code === 2) return COND.PARTLY_CLOUDY;
  if (code === 3) return COND.CLOUDY;
  if (code >= 45 && code <= 48) return COND.FOG;
  if (code >= 51 && code <= 67) return COND.RAIN;
  if (code >= 71 && code <= 77) return COND.SNOW;
  if (code >= 80 && code <= 82) return COND.RAIN;
  if (code >= 85 && code <= 86) return COND.SNOW;
  if (code >= 95 && code <= 99) return COND.STORM;
  return COND.PARTLY_CLOUDY;
}

function degToCompass(deg) {
  var dirs = ['N','NE','E','SE','S','SW','W','NW'];
  return dirs[Math.round(deg / 45) % 8];
}

// Resolve the active time format. "12"/"24" are explicit overrides; "0"
// (default, "Match watch") follows the watch's system clock style, which the
// C side reports via the ClockIs24h AppMessage on each refresh request.
function use24h() {
  var tf = localStorage.getItem('timeFormat') || '0';
  if (tf === '1') return false;            // 12-hour
  if (tf === '2') return true;             // 24-hour
  return localStorage.getItem('clockIs24h') === '1';  // match watch
}

function fmtTime12(iso) {
  if (!iso) return '';
  var t = iso.split('T')[1] || '';
  var parts = t.split(':');
  var h = parseInt(parts[0], 10);
  var m = parts[1] || '00';
  if (use24h()) {
    return (h < 10 ? '0' + h : h) + ':' + m;
  }
  var ampm = h >= 12 ? 'PM' : 'AM';
  h = h % 12; if (h === 0) h = 12;
  return h + ':' + m + ' ' + ampm;
}

// Moon phase from Julian-date formula. Open-Meteo does not provide
// moon_phase / moonrise / moonset (verified 2026-05). Reference new
// moon: 2000-01-06 18:14 UTC = JD 2451550.1. Synodic month = 29.5305889.
// Returns { phase: 0..7 enum, illum: 0..100, name1, name2 }.
function computeMoonPhase(date) {
  var ms = date.getTime();
  var jd = ms / 86400000.0 + 2440587.5;
  var p = (jd - 2451550.1) / 29.530588853;
  p = p - Math.floor(p); // 0..1
  // Illumination via cosine of phase angle.
  var illum = Math.round((1 - Math.cos(p * 2 * Math.PI)) * 50);
  // Phase enum + names. Bands per standard astronomy convention.
  var phase, name1, name2;
  if      (p < 0.03)  { phase = 0; name1 = 'NEW';     name2 = 'MOON'; }
  else if (p < 0.22)  { phase = 1; name1 = 'WAXING';  name2 = 'CRESCENT'; }
  else if (p < 0.28)  { phase = 2; name1 = 'FIRST';   name2 = 'QUARTER'; }
  else if (p < 0.47)  { phase = 3; name1 = 'WAXING';  name2 = 'GIBBOUS'; }
  else if (p < 0.53)  { phase = 4; name1 = 'FULL';    name2 = 'MOON'; }
  else if (p < 0.72)  { phase = 5; name1 = 'WANING';  name2 = 'GIBBOUS'; }
  else if (p < 0.78)  { phase = 6; name1 = 'LAST';    name2 = 'QUARTER'; }
  else if (p < 0.97)  { phase = 7; name1 = 'WANING';  name2 = 'CRESCENT'; }
  else                { phase = 0; name1 = 'NEW';     name2 = 'MOON'; }
  return { phase: phase, illum: illum, name1: name1, name2: name2 };
}

function getUnits() {
  return localStorage.getItem('units') === 'metric' ? 'metric' : 'imperial';
}

// Wind speed unit, INDEPENDENT of the measurement system. Requested on
// r/pebble by a Danish user: m/s is the everyday wind unit across Scandinavia
// while °C was never in question, so folding it into `units` would have been
// the wrong axis. Stored as the raw Clay choice; 'auto' is the default, which
// is what keeps this from changing anything for existing users.
function getWindUnitPref() {
  var w = localStorage.getItem('windUnit');
  return (w === 'mph' || w === 'kmh' || w === 'ms') ? w : 'auto';
}

// Resolve 'auto' against the measurement system. Deliberately done phone-side:
// the request has to name a concrete unit anyway (wind_speed_unit=), so
// sending the RESOLVED value means the watch never consults its units setting
// to label wind, and carries no AUTO case at all. See WindUnits in
// weather_data.h — that is a branch removed from the watch, not added.
function resolveWindUnit(units) {
  var w = getWindUnitPref();
  if (w !== 'auto') return w;
  return units === 'metric' ? 'kmh' : 'mph';
}

// Our storage strings are exactly Open-Meteo's wind_speed_unit values, so the
// URL takes them verbatim. The watch does not — it gets this enum, which must
// stay in step with WindUnits in weather_data.h.
var WIND_UNIT_CODE = { mph: 0, kmh: 1, ms: 2 };

function getUseDewPoint() {
  return localStorage.getItem('useDewPoint') === '1';
}

function getShowLocation() {
  return localStorage.getItem('showLocation') === '1';
}

// Returns true when coordinates are within continental Europe + UK +
// Scandinavia. Open-Meteo CAMS pollen data is reliable inside this box;
// outside it we fall back to the Google Pollen proxy.
function isEurope(lat, lon) {
  return lat >= 35 && lat <= 72 && lon >= -25 && lon <= 45;
}

// Convert Open-Meteo pollen values (grains/m³) to the 0-5 UPI-style
// scale used by the Google Pollen API, using European Aeroallergen
// Network (EAN) category thresholds per pollen type. Returns -1 when
// all inputs are null (region not covered by CAMS).
//
// EAN bands (grains/m³) used here, normalized to the Google UPI 0..5
// scale (0=None, 1=Very Low, 2=Low, 3=Moderate, 4=High, 5=Very High):
//   Grass:   0 | 1-5   | 6-20  | 21-50  | 51-200 | >200
//   Tree:    0 | 1-15  | 16-50 | 51-100 | 101-300| >300  (birch/alder)
//   Weed:    0 | 1-5   | 6-15  | 16-50  | 51-200 | >200  (ragweed/mugwort)
function pollenGrainsToUpi(grass, birch, alder, ragweed, mugwort, olive) {
  function grassScale(v)   {
    if (v === null || v === undefined) return -1;
    if (v <= 0)   return 0; if (v <= 5)   return 1;
    if (v <= 20)  return 2; if (v <= 50)  return 3;
    if (v <= 200) return 4; return 5;
  }
  function treeScale(v) {
    if (v === null || v === undefined) return -1;
    if (v <= 0)   return 0; if (v <= 15)  return 1;
    if (v <= 50)  return 2; if (v <= 100) return 3;
    if (v <= 300) return 4; return 5;
  }
  function weedScale(v) {
    if (v === null || v === undefined) return -1;
    if (v <= 0)   return 0; if (v <= 5)   return 1;
    if (v <= 15)  return 2; if (v <= 50)  return 3;
    if (v <= 200) return 4; return 5;
  }
  var vals = [
    grassScale(grass), treeScale(birch),   treeScale(alder),
    weedScale(ragweed), weedScale(mugwort), treeScale(olive),
  ];
  var max = -1;
  for (var i = 0; i < vals.length; i++) {
    if (vals[i] > max) max = vals[i];
  }
  return max;
}

// Phase 11: Golden / Blue hour computation.
//
// Compact port of Vladimir Agafonkin's SunCalc (BSD-2). Computes sun
// times for arbitrary altitude angles. We use:
//   -6°    civil twilight (blue hour outer boundary)
//   -0.833° apparent sunrise/sunset (atmospheric refraction)
//    +6°   sun "high" boundary (golden hour upper edge)
//
// Morning chronology: blueHour.rise → sunrise.rise → goldenHour.rise
// Evening chronology: goldenHour.set → sunrise.set → blueHour.set
//
// We send four "milestone start" timestamps:
//   blue_am = blueHour.rise   (morning blue hour begins)
//   gold_am = sunrise.rise    (morning golden hour begins, sunrise)
//   gold_pm = goldenHour.set  (evening golden hour begins)
//   blue_pm = sunrise.set     (evening blue hour begins, sunset)
function computeGoldenHour(date, lat, lng, utcOffsetSec) {
  var rad = Math.PI / 180;
  var dayMs = 86400000;
  var J1970 = 2440588;
  var J2000 = 2451545;

  function toJulian(d) { return d.valueOf() / dayMs - 0.5 + J1970; }
  function fromJulian(j) { return new Date((j + 0.5 - J1970) * dayMs); }
  function toDays(d) { return toJulian(d) - J2000; }

  var e = rad * 23.4397;
  function declination(l, b) {
    return Math.asin(Math.sin(b) * Math.cos(e) +
                     Math.cos(b) * Math.sin(e) * Math.sin(l));
  }
  function solarMeanAnomaly(d) { return rad * (357.5291 + 0.98560028 * d); }
  function eclipticLongitude(M) {
    var C = rad * (1.9148 * Math.sin(M) + 0.02 * Math.sin(2 * M) +
                   0.0003 * Math.sin(3 * M));
    var P = rad * 102.9372;
    return M + C + P + Math.PI;
  }
  function julianCycle(d, lw) { return Math.round(d - 0.0009 - lw / (2 * Math.PI)); }
  function approxTransit(Ht, lw, n) { return 0.0009 + (Ht + lw) / (2 * Math.PI) + n; }
  function solarTransitJ(ds, M, L) {
    return J2000 + ds + 0.0053 * Math.sin(M) - 0.0069 * Math.sin(2 * L);
  }
  function hourAngle(h, phi, d) {
    return Math.acos((Math.sin(h) - Math.sin(phi) * Math.sin(d)) /
                     (Math.cos(phi) * Math.cos(d)));
  }
  function getSetJ(h, lw, phi, dec, n, M, L) {
    var w = hourAngle(h, phi, dec);
    var a = approxTransit(w, lw, n);
    return solarTransitJ(a, M, L);
  }

  var lw = rad * -lng;
  var phi = rad * lat;
  var d = toDays(date);
  var n = julianCycle(d, lw);
  var ds = approxTransit(0, lw, n);
  var M = solarMeanAnomaly(ds);
  var L = eclipticLongitude(M);
  var dec = declination(L, 0);
  var Jnoon = solarTransitJ(ds, M, L);

  function timesForAngle(angle) {
    var Jset = getSetJ(angle * rad, lw, phi, dec, n, M, L);
    var Jrise = Jnoon - (Jset - Jnoon);
    return { rise: fromJulian(Jrise), set: fromJulian(Jset) };
  }

  var sunrise = timesForAngle(-0.833);
  var blue = timesForAngle(-6);
  var gold = timesForAngle(6);

  // hourAngle can return NaN at extreme latitudes / polar day-night.
  // Detect via isNaN on the resulting Date and bail out gracefully.
  //
  // Date math note: SunCalc's fromJulian() returns a UTC-anchored Date.
  // The PKJS runtime's host timezone is unpredictable (often UTC on the
  // phone bridge), so we must NOT use d.getHours()/getMinutes() — they
  // would return host-local time, not the user's local time. Instead we
  // shift by Open-Meteo's `utc_offset_seconds` (which is exact for the
  // forecast point's tz, including DST) and read getUTC* from the
  // shifted timestamp.
  function fmt(d) {
    if (!d || isNaN(d.getTime())) return '--:--';
    var shifted = new Date(d.getTime() + (utcOffsetSec || 0) * 1000);
    var h = shifted.getUTCHours();
    var m = shifted.getUTCMinutes();
    var mm = m < 10 ? '0' + m : m;
    if (use24h()) {
      return (h < 10 ? '0' + h : h) + ':' + mm;
    }
    var ampm = h >= 12 ? 'PM' : 'AM';
    h = h % 12; if (h === 0) h = 12;
    return h + ':' + mm + ' ' + ampm;
  }

  return {
    BlueAm: fmt(blue.rise),
    GoldAm: fmt(sunrise.rise),
    GoldPm: fmt(gold.set),
    BluePm: fmt(sunrise.set)
  };
}

// Last-known lat/lon (cached so the Radar card can re-fetch without
// re-running geolocation, and so a Radar request that arrives before
// any weather fetch still has somewhere to look).
var lastLat = null;
var lastLon = null;

function xhr(url, cb) {
  var req = new XMLHttpRequest();
  req.open('GET', url, true);
  req.timeout = 15000;
  req.onload = function() {
    if (req.status >= 200 && req.status < 300) {
      try { cb(null, JSON.parse(req.responseText)); }
      catch (e) { cb(e); }
    } else {
      cb(new Error('HTTP ' + req.status));
    }
  };
  req.onerror = function() { cb(new Error('xhr error')); };
  req.ontimeout = function() { cb(new Error('xhr timeout')); };
  req.send();
}

// Best-effort reverse geocode for a human-readable city name. Uses
// BigDataCloud's keyless client endpoint. Always invokes `done` with a
// string: the freshly resolved name, or the last cached name, or '' —
// never blocks or fails the weather fetch.
//
// Cached by ~1.1km coordinate cell (2 decimal places) for 24h — city names
// rarely change, and this was previously a network hit on EVERY refresh.
// Moving to a different cell misses the cache and refetches, so travel is
// handled by construction. A failed or empty resolve never refreshes the
// timestamp, so the next fetch retries.
var GEO_TTL_MS = 24 * 60 * 60 * 1000;

function reverseGeocode(lat, lon, done) {
  var cellKey = lat.toFixed(2) + ',' + lon.toFixed(2);
  var cachedName = localStorage.getItem('lastLocationName') || '';
  var cachedAt = parseInt(localStorage.getItem('geoCacheAt') || '0', 10);
  if (cachedName && cachedAt > 0 &&
      localStorage.getItem('geoCacheCoords') === cellKey &&
      Date.now() - cachedAt < GEO_TTL_MS) {
    console.log('geocode cache hit: ' + cachedName);
    done(cachedName);
    return;
  }
  var url = 'https://api.bigdatacloud.net/data/reverse-geocode-client' +
            '?latitude=' + lat + '&longitude=' + lon + '&localityLanguage=en';
  xhr(url, function(err, data) {
    if (err || !data) {
      done(localStorage.getItem('lastLocationName') || '');
      return;
    }
    var name = data.city || data.locality || data.principalSubdivision || '';
    if (name) {
      localStorage.setItem('lastLocationName', name);
      localStorage.setItem('geoCacheCoords', cellKey);
      localStorage.setItem('geoCacheAt', String(Date.now()));
    } else {
      name = localStorage.getItem('lastLocationName') || '';
    }
    done(name);
  });
}

// ---------------------------------------------------------------------------
// STORE-CAPTURE MODE — must be false in any shipped build.
//
// Replaces the live Open-Meteo fetch with one fixed payload so the store
// screenshots show the same forecast on every platform and every card. Nothing
// in src/c is involved: this only changes which numbers the watch is handed, so
// what gets photographed is the real rendering. With the live fetch running,
// its reply lands at an unpredictable moment and overwrites the capture data
// mid-run. Radar is deliberately NOT stubbed — it keeps its real RainViewer
// pipeline, and lastLat/lastLon are seeded here so it has somewhere to centre.
var CAPTURE_MODE = false;
var CAPTURE_LAT = 37.7749;
var CAPTURE_LON = -122.4194;

// NO-DATA MODE — must also be false in any shipped build.
//
// The opposite harness: answer the watch with NOTHING, so the app stays in the
// state it holds before its first reading ever lands. That state is otherwise
// impossible to photograph — the fetch replies about a second after launch, and
// a screenshot cannot be timed inside that window reliably. It is what a user
// sees on a first-ever install, and (until the cache key was pinned) after any
// update that orphaned their cache, so it is worth being able to reproduce on
// demand rather than by luck. Every card should show NO DATA YET / WAITING FOR
// PHONE; a card showing numbers here is fabricating them.
var CAPTURE_NO_DATA = false;

function capturePayload() {
  var now = new Date();
  var msg = {
    Temp: 72, FeelsLike: 75, High: 84, Low: 61, Condition: 1,
    Wind: 12, WindDir: 'NW', Humidity: 58, DewPoint: 55,
    UV: 7, UVMax: 9, AQI: 42, PM25: 9, PM10: 17, O3: 31, NO2: 12,
    PollenLevel: 2,
    Sunrise: '6:14 AM', Sunset: '7:45 PM',
    BlueAm: '5:45 AM', GoldAm: '6:14 AM',
    GoldPm: '7:05 PM', BluePm: '7:45 PM',
    LocationName: 'San Francisco',
    RainAlertMinutes: -1,
    Units: 0, UseDewPoint: 0, ShowLocation: 1,
    LastUpdated: Math.floor(Date.now() / 1000)
  };
  // Precipitation card: now -> +4h
  var pop = [10, 15, 35, 60, 55];
  for (var i = 0; i < 5; i++) { msg['Precip' + i] = pop[i]; }

  var temps = [74, 78, 81, 83, 80, 76];
  var conds = [1, 1, 2, 3, 3, 2];
  var pops = [10, 15, 35, 60, 55, 25];
  var winds = [11, 12, 14, 16, 15, 13];
  var wdirs = ['NW', 'NW', 'W', 'W', 'SW', 'SW'];
  var amts = [0, 0, 2, 6, 4, 1];   // tenths of an inch
  var uvs = [6, 7, 9, 8, 6, 4];
  for (var h = 1; h <= 6; h++) {
    var hr = (now.getHours() + h) % 24;
    var ampm = hr >= 12 ? 'PM' : 'AM';
    var h12 = hr % 12; if (h12 === 0) { h12 = 12; }
    msg['Hour' + h + 'Label'] = h12 + ' ' + ampm;
    msg['Hour' + h + 'Temp'] = temps[h - 1];
    msg['Hour' + h + 'Cond'] = conds[h - 1];
    msg['Hour' + h + 'Pop'] = pops[h - 1];
    msg['Hour' + h + 'Wind'] = winds[h - 1];
    msg['Hour' + h + 'WindDir'] = wdirs[h - 1];
    msg['Hour' + h + 'Precip'] = amts[h - 1];
    msg['Hour' + h + 'Uv'] = uvs[h - 1];
  }

  var names = ['SUN', 'MON', 'TUE', 'WED', 'THU', 'FRI', 'SAT'];
  var highs = [84, 79, 73, 81, 86];
  var lows = [61, 59, 57, 60, 63];
  var dconds = [1, 3, 3, 0, 0];
  var dpops = [20, 70, 65, 10, 5];
  for (var dI = 0; dI < 5; dI++) {
    msg['Day' + dI + 'Label'] = names[(now.getDay() + dI) % 7];
    msg['Day' + dI + 'High'] = highs[dI];
    msg['Day' + dI + 'Low'] = lows[dI];
    msg['Day' + dI + 'Cond'] = dconds[dI];
    msg['Day' + dI + 'Pop'] = dpops[dI];
  }
  msg.MoonPhase = 3;
  msg.MoonIllum = 72;
  msg.MoonName1 = 'WAXING';
  msg.MoonName2 = 'GIBBOUS';
  return msg;
}

function fetchWeather(lat, lon) {
  if (CAPTURE_NO_DATA) {
    console.log('no-data mode: withholding payload');
    fetchDone();
    return;
  }
  if (CAPTURE_MODE) {
    // Seed the radar's coordinates so its real pipeline still has a centre.
    lastLat = CAPTURE_LAT;
    lastLon = CAPTURE_LON;
    Pebble.sendAppMessage(capturePayload(),
      function() { console.log('capture payload sent'); fetchDone(); },
      function(e) {
        console.log('capture send fail: ' + JSON.stringify(e));
        fetchDone();
      });
    return;
  }
  lastLat = lat;
  lastLon = lon;
  trackPing(lat, lon); // anonymous once-per-day active-user ping (fire-and-forget)
  var units = getUnits();
  var tempUnit = units === 'metric' ? 'celsius' : 'fahrenheit';
  var windUnit = resolveWindUnit(units);

  var fc = 'https://api.open-meteo.com/v1/forecast' +
    '?latitude=' + lat + '&longitude=' + lon +
    '&current=temperature_2m,apparent_temperature,relative_humidity_2m,dew_point_2m,weather_code,wind_speed_10m,wind_direction_10m,uv_index' +
    '&hourly=temperature_2m,weather_code,precipitation_probability,wind_speed_10m,wind_direction_10m,precipitation,uv_index' +
    '&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max,sunrise,sunset,uv_index_max' +
    '&temperature_unit=' + tempUnit +
    '&wind_speed_unit=' + windUnit +
    '&timezone=auto&forecast_days=5';

  var aq = 'https://air-quality-api.open-meteo.com/v1/air-quality' +
    '?latitude=' + lat + '&longitude=' + lon +
    // Always request pollen fields. CAMS covers Europe; outside that
    // region the fields return null and we fall back to Google.
    '&current=us_aqi,pm2_5,pm10,ozone,nitrogen_dioxide,grass_pollen,birch_pollen,alder_pollen,ragweed_pollen,mugwort_pollen,olive_pollen' +
    '&timezone=auto';

  // Resolve the city name first (best-effort), then fetch weather so the
  // name can ride along in the same AppMessage. reverseGeocode always
  // calls back with a string, so a geocode failure never blocks weather.
  reverseGeocode(lat, lon, function(locName) {
  xhr(fc, function(err, data) {
    if (err) { console.log('forecast err: ' + err.message); fetchDone(); return; }
    xhr(aq, function(_e2, aqd) {
      var msg = {};
      msg.LocationName = (locName || '').substring(0, 31);
      var gotPollenFromOpenMeteo = false;
      try {
        var cur = data.current || {};
        var daily = data.daily || {};
        var hourly = data.hourly || {};
        msg.Temp = Math.round(cur.temperature_2m);
        msg.FeelsLike = Math.round(cur.apparent_temperature);
        msg.Humidity = Math.round(cur.relative_humidity_2m);
        msg.DewPoint = Math.round(cur.dew_point_2m);
        msg.UseDewPoint = getUseDewPoint() ? 1 : 0;
        msg.ShowLocation = getShowLocation() ? 1 : 0;
        msg.Wind = Math.round(cur.wind_speed_10m);
        msg.WindDir = degToCompass(cur.wind_direction_10m || 0);
        msg.Condition = mapWeatherCode(cur.weather_code);
        if (daily.temperature_2m_max && daily.temperature_2m_max.length) {
          msg.High = Math.round(daily.temperature_2m_max[0]);
          msg.Low = Math.round(daily.temperature_2m_min[0]);
        }
        if (daily.sunrise && daily.sunrise.length) {
          msg.Sunrise = fmtTime12(daily.sunrise[0]);
          msg.Sunset = fmtTime12(daily.sunset[0]);
        }
        // UV semantics:
        //   msg.UV    — current UV (live gauge value)
        //   msg.UVMax — today's forecast peak (subtitle "PEAK n")
        // Fall back to daily peak if `current.uv_index` is missing on
        // older API responses, so we never regress to undefined.
        var dailyMax = (daily.uv_index_max && daily.uv_index_max.length)
                       ? daily.uv_index_max[0] : null;
        if (typeof cur.uv_index === 'number') {
          msg.UV = Math.round(cur.uv_index);
        } else if (dailyMax !== null) {
          msg.UV = Math.round(dailyMax);
        }
        if (dailyMax !== null) {
          msg.UVMax = Math.round(dailyMax);
        }
        var p = hourly.precipitation_probability || [];
        var times = hourly.time || [];
        // Find the index of the current hour in the hourly arrays so the
        // 5 bars truly represent "Now / +1h / +2h / +3h / +4h" instead of
        // starting at midnight. Open-Meteo returns local-tz timestamps
        // like "2026-05-06T14:00" when timezone=auto.
        var startIdx = 0;
        if (times.length) {
          // Open-Meteo (timezone=auto) returns hourly.time in the LOCATION's
          // timezone. cur.time is in that same tz, so key off it — using the
          // phone's local clock would miss the match under a LocationOverride
          // in a different timezone and silently start the bars at midnight.
          var nowKey;
          if (cur.time) {
            nowKey = cur.time.slice(0, 13) + ':00';  // "2026-05-06T14" + ":00"
          } else {
            var now = new Date();
            var pad = function(n) { return n < 10 ? '0' + n : '' + n; };
            nowKey = now.getFullYear() + '-' + pad(now.getMonth() + 1) +
                     '-' + pad(now.getDate()) + 'T' + pad(now.getHours()) + ':00';
          }
          for (var k = 0; k < times.length; k++) {
            if (times[k] === nowKey) { startIdx = k; break; }
          }
        }
        for (var i = 0; i < 5; i++) {
          msg['Precip' + i] = Math.round(p[startIdx + i] || 0);
        }
        // msg.RainAlertMinutes is computed below, once hPrcp (precip amount)
        // is available — the pill is driven by measurable amount, not POP,
        // so it agrees with the 6 Hours card's droplets.
        if (aqd && aqd.current) {
          msg.AQI = Math.round(aqd.current.us_aqi || 0);
          // Phase 4 AIR DETAIL breakdown — pollutant concentrations (µg/m³).
          // Open-Meteo names ozone/nitrogen_dioxide; we shorten to O3/NO2.
          msg.PM25 = Math.round(aqd.current.pm2_5 || 0);
          msg.PM10 = Math.round(aqd.current.pm10 || 0);
          msg.O3   = Math.round(aqd.current.ozone || 0);
          msg.NO2  = Math.round(aqd.current.nitrogen_dioxide || 0);
        }
        msg.Units = units === 'metric' ? 1 : 0;
        // Already resolved — never 'auto'. Sent alongside Units so the watch's
        // absent-key fallback (comm.c) and this value can never disagree about
        // which reading the numbers above are in.
        msg.WindUnits = WIND_UNIT_CODE[windUnit];
        msg.LastUpdated = Math.floor(Date.now() / 1000);

        // Pollen — hybrid strategy:
        //   Europe  → use Open-Meteo CAMS fields already in the AQ
        //             response (free, no quota, zero extra requests).
        //   Elsewhere → fetchPollen() calls the Google proxy after send.
        // `gotPollenFromOpenMeteo` is declared outside try so the
        // sendAppMessage callback can read it.
        // Phase 10A: Next 6 Hours (offsets +1h..+6h from current hour).
        var temps = hourly.temperature_2m || [];
        var codes = hourly.weather_code || [];
        var hWind  = hourly.wind_speed_10m || [];
        var hWdir  = hourly.wind_direction_10m || [];
        var hPrcp  = hourly.precipitation || [];
        var hUv    = hourly.uv_index || [];

        // Rain alert: first hour (now..+6h) with a *measurable* amount, using
        // the same metric the 6 Hours card uses to draw a droplet
        // (round(amount*10) > 0). This keeps the pill consistent with that
        // card; the Precipitation card stays the separate "chance" (POP) view.
        var alert = -1;
        for (var j = 0; j <= 6; j++) {
          var amt = Math.round((hPrcp[startIdx + j] || 0) * 10);
          if (amt > 0) { alert = j === 0 ? 15 : j * 60; break; }
        }
        msg.RainAlertMinutes = alert;

        for (var hi = 1; hi <= 6; hi++) {
          var idx = startIdx + hi;
          var hourLabel = '';
          if (times[idx]) {
            var hh = parseInt(times[idx].split('T')[1].split(':')[0], 10);
            if (use24h()) {
              hourLabel = String(hh);
            } else {
              var ampm = hh >= 12 ? 'PM' : 'AM';
              hh = hh % 12; if (hh === 0) hh = 12;
              hourLabel = hh + ' ' + ampm;
            }
          }
          msg['Hour' + hi + 'Label'] = hourLabel;
          msg['Hour' + hi + 'Temp']  = Math.round(temps[idx] || 0);
          msg['Hour' + hi + 'Cond']  = mapWeatherCode(codes[idx] || 0);
          msg['Hour' + hi + 'Pop']   = Math.round(p[idx] || 0);
          // Wind speed in selected unit (mph/kmh), rounded to integer.
          msg['Hour' + hi + 'Wind']    = Math.round(hWind[idx] || 0);
          msg['Hour' + hi + 'WindDir'] = degToCompass(hWdir[idx] || 0);
          // Precip amount as integer tenths of in/mm (avoids floats on watch).
          msg['Hour' + hi + 'Precip']  = Math.round((hPrcp[idx] || 0) * 10);
          // UV index (integer) for the UV modal's hourly curve.
          msg['Hour' + hi + 'Uv']      = Math.round(hUv[idx] || 0);
        }

        // Phase 10B: Week Ahead (today + next 4 days = 5 total).
        var dayCodes = daily.weather_code || [];
        var dayHigh  = daily.temperature_2m_max || [];
        var dayLow   = daily.temperature_2m_min || [];
        var dayPop   = daily.precipitation_probability_max || [];
        var dayTimes = daily.time || [];
        var dayNames = ['SUN','MON','TUE','WED','THU','FRI','SAT'];
        for (var di = 0; di < 5; di++) {
          var lbl = '';
          if (dayTimes[di]) {
            var dt = new Date(dayTimes[di] + 'T00:00');
            lbl = dayNames[dt.getDay()];
          }
          msg['Day' + di + 'Label'] = lbl;
          msg['Day' + di + 'High']  = Math.round(dayHigh[di] || 0);
          msg['Day' + di + 'Low']   = Math.round(dayLow[di]  || 0);
          msg['Day' + di + 'Cond']  = mapWeatherCode(dayCodes[di] || 0);
          msg['Day' + di + 'Pop']   = Math.round(dayPop[di]  || 0);
        }

        // Phase 7: Moon phase computed locally (Open-Meteo lacks this).
        var moon = computeMoonPhase(new Date());
        msg.MoonPhase = moon.phase;
        msg.MoonIllum = moon.illum;
        msg.MoonName1 = moon.name1;
        msg.MoonName2 = moon.name2;

        // Phase 11: Golden / Blue hour times computed locally.
        // utc_offset_seconds is exact for the forecast point's tz
        // (including DST), and avoids relying on the unpredictable
        // PKJS runtime timezone.
        var tzOffset = (typeof data.utc_offset_seconds === 'number')
                       ? data.utc_offset_seconds : 0;
        var gh = computeGoldenHour(new Date(), lat, lon, tzOffset);
        msg.BlueAm = gh.BlueAm;
        msg.GoldAm = gh.GoldAm;
        msg.GoldPm = gh.GoldPm;
        msg.BluePm = gh.BluePm;

        // Attempt Open-Meteo pollen (Europe only). If successful, skip
        // the Google proxy call entirely for this request.
        if (isEurope(lat, lon) && aqd && aqd.current) {
          var aqc = aqd.current;
          var euUpi = pollenGrainsToUpi(
            aqc.grass_pollen,  aqc.birch_pollen,  aqc.alder_pollen,
            aqc.ragweed_pollen, aqc.mugwort_pollen, aqc.olive_pollen
          );
          if (euUpi >= 0) {
            msg.PollenLevel = euUpi;
            gotPollenFromOpenMeteo = true;
          }
        }
      } catch (e) {
        console.log('parse err: ' + e.message);
        fetchDone();
        return;
      }
      Pebble.sendAppMessage(msg,
        function() {
          console.log('weather sent');
          // Successful round-trip: record freshness for the `ready` gate.
          localStorage.setItem('lastFetchAt', String(Date.now()));
          fetchDone();
          // Only call Google proxy for non-European locations.
          if (!gotPollenFromOpenMeteo) {
            fetchPollen(lat, lon);
          }
        },
        function(e) {
          console.log('send fail: ' + JSON.stringify(e));
          fetchDone();
        }
      );
    });
  });
  });
}

// Fetch dedupe. Launch used to double-fetch: PKJS `ready` fired one chain
// AND the C side's 750ms LastUpdated sentinel fired another. The in-flight
// guard absorbs whichever arrives second; `lastFetchAt` (set only on a
// successful send) lets `ready` skip entirely when data is fresh, mirroring
// the C side's 15-minute open-refresh gate. The sentinel path stays ungated
// by freshness — it's the fail-safe for wiped watch storage, manual refresh
// and background wakeups (min bg interval 30min > gate, never starved).
var FETCH_FRESH_MS = 15 * 60 * 1000;      // matches comm.c threshold_secs
var FETCH_IN_FLIGHT_MS = 20000;           // xhr timeout is 15s; self-heals
var fetchStartedAt = 0;

function fetchDone() { fetchStartedAt = 0; }

function maybeInitialFetch() {
  var last = parseInt(localStorage.getItem('lastFetchAt') || '0', 10);
  var age = Date.now() - last;
  if (last > 0 && age < FETCH_FRESH_MS) {
    console.log('ready: last fetch ' + Math.round(age / 1000) +
                's ago, skipping');
    return;
  }
  locateAndFetch();
}

function locateAndFetch() {
  if (Date.now() - fetchStartedAt < FETCH_IN_FLIGHT_MS) {
    console.log('fetch already in flight, skipping');
    return;
  }
  fetchStartedAt = Date.now();
  if (CAPTURE_MODE) {
    // Skip geolocation too — the emulator's fix is slow and irrelevant here.
    fetchWeather(CAPTURE_LAT, CAPTURE_LON);
    return;
  }
  var override = localStorage.getItem('locationOverride');
  if (override) {
    var parts = override.split(',');
    if (parts.length === 2) {
      fetchWeather(parseFloat(parts[0]), parseFloat(parts[1]));
      return;
    }
  }
  navigator.geolocation.getCurrentPosition(
    function(pos) {
      fetchWeather(pos.coords.latitude, pos.coords.longitude);
    },
    function(err) {
      console.log('geo err: ' + err.message + ' — using fallback');
      fetchWeather(37.7749, -122.4194);
    },
    { timeout: 15000, maximumAge: 600000 }
  );
}

// ----------------------------------------------------------------------
// Phase 12: Radar streaming.
// ----------------------------------------------------------------------

// PROTOCOL CONSTANT — the watch reassembles chunks at offsets of
// idx * RADAR_CHUNK_SIZE using its OWN copy of this number (radar.c:36).
// The two must match exactly. Only emery/gabbro ever receive chunks
// (TW_RADAR_SUPPORTED carves radar out elsewhere) and their inbox is 2048,
// so the small screens' 1536 inbox puts no pressure on this value.
// Shrinking this side alone to 1400 left every chunk 100 bytes short of
// its slot: late chunks landed past the buffer, were dropped uncounted,
// and radar stalled at 93%.
var RADAR_CHUNK_SIZE = 1500;
// Shared secret for the Vercel proxy (RADAR_SECRET env var), loaded from
// gitignored secrets.js (template: secrets.js.example) so the key never
// lands in the public repo. Kept URL-safe (no '#', '&', '@', etc.) so it can
// be embedded directly in a query string without encoding gymnastics. Prior
// values containing '#' were silently truncated by the HTTP client at the
// fragment marker, causing 401s. With an empty key the proxy endpoints just
// 401 and radar/pollen/analytics degrade gracefully.
var PROXY_KEY = require('./secrets').PROXY_KEY;
function proxyUrl(path) {
  var u = 'https://touchyweather-radar-proxy.vercel.app/api/' + path;
  return PROXY_KEY ? u + '?key=' + PROXY_KEY : u;
}
var RADAR_PROXY_URL = proxyUrl('radar');

// Pollen proxy shares the same Vercel project + RADAR_SECRET auth key
// as the radar endpoint, so the URL differs only in the /api path.
var POLLEN_PROXY_URL = proxyUrl('pollen');

// Anonymous analytics ping. Same Vercel project + RADAR_SECRET auth key as
// radar/pollen. The server hashes the id and stores only aggregate counts —
// nothing identifiable leaves the device. See proxy/api/track.js.
var TRACK_PROXY_URL = proxyUrl('track');

// Pollen is throttled to one proxy fetch per 6 hours per device.
// Between fetches the last known level is re-sent from localStorage so
// the watch always receives an up-to-date (or recently cached) value
// without burning Google API quota on every weather refresh.
var POLLEN_TTL_MS = 6 * 60 * 60 * 1000; // 6 hours

function fetchPollen(lat, lon) {
  var now = Date.now();
  var lastFetch = parseInt(localStorage.getItem('pollenFetchedAt') || '0', 10);
  var cachedLevel = parseInt(localStorage.getItem('pollenLevel') || '-1', 10);

  // If we have a recent cached value, send it immediately and skip the
  // proxy call entirely — saves both Vercel bandwidth and Google quota.
  if (now - lastFetch < POLLEN_TTL_MS && lastFetch > 0) {
    console.log('pollen cached: ' + cachedLevel);
    Pebble.sendAppMessage({ PollenLevel: cachedLevel },
      function() {},
      function(e) { console.log('pollen (cached) send fail: ' + JSON.stringify(e)); }
    );
    return;
  }

  var sep = POLLEN_PROXY_URL.indexOf('?') >= 0 ? '&' : '?';
  var url = POLLEN_PROXY_URL + sep + 'lat=' + lat + '&lon=' + lon;
  xhr(url, function(err, data) {
    if (err) {
      console.log('pollen err: ' + err.message);
      // On failure, still send the last known cached value so the watch
      // isn't stuck waiting. Don't update the timestamp so we retry sooner.
      if (lastFetch > 0) {
        Pebble.sendAppMessage({ PollenLevel: cachedLevel }, function() {}, function() {});
      }
      return;
    }
    var level = (data && typeof data.level === 'number') ? data.level : -1;
    localStorage.setItem('pollenLevel', String(level));
    localStorage.setItem('pollenFetchedAt', String(now));
    Pebble.sendAppMessage({ PollenLevel: level },
      function() { console.log('pollen sent: ' + level); },
      function(e) { console.log('pollen send fail: ' + JSON.stringify(e)); }
    );
  });
}

// ----------------------------------------------------------------------
// Anonymous active-user analytics.
//
// Identifies the user by Pebble's account token — a stable, per-user,
// per-app value containing no name/email/PII. When that's unavailable
// (user not signed in) we fall back to a random id persisted locally.
// The raw id is sent over HTTPS and hashed server-side; only aggregate
// DAU/WAU/MAU/YAU counts + a coarse (~11 km) location cell are stored.
// Throttled to one ping per UTC day so "daily active" is the natural unit.
// Fire-and-forget: failures never affect weather/radar/pollen.
// ----------------------------------------------------------------------

function utcDayStr() {
  var d = new Date();
  function p(n) { return n < 10 ? '0' + n : '' + n; }
  return d.getUTCFullYear() + '-' + p(d.getUTCMonth() + 1) + '-' + p(d.getUTCDate());
}

function getAnalyticsId() {
  // Prefer the anonymous Pebble account token (consistent across the same
  // user's watches). It can be '' when the user isn't signed in.
  var token = '';
  try { token = Pebble.getAccountToken() || ''; } catch (e) { token = ''; }
  if (token) return token;

  // Fallback: a locally persisted random id so an unsigned-in user still
  // counts as one stable device rather than a new user every launch.
  var anon = localStorage.getItem('anonId');
  if (!anon) {
    anon = 'anon-';
    for (var i = 0; i < 32; i++) {
      anon += Math.floor(Math.random() * 16).toString(16);
    }
    localStorage.setItem('anonId', anon);
  }
  return anon;
}

function trackPing(lat, lon) {
  var today = utcDayStr();
  if (localStorage.getItem('lastPingDay') === today) return; // already counted today

  var id = getAnalyticsId();
  // Round coords to 0.1° (~11 km) before they leave the device — analytics
  // never needs precise location, only a coarse heatmap cell.
  var rLat = Math.round(lat * 10) / 10;
  var rLon = Math.round(lon * 10) / 10;

  try {
    var req = new XMLHttpRequest();
    req.open('POST', TRACK_PROXY_URL, true);
    req.timeout = 15000;
    req.setRequestHeader('Content-Type', 'application/json');
    req.onload = function() {
      if (req.status >= 200 && req.status < 300) {
        // Mark the day done only on success so a failed ping retries next refresh.
        localStorage.setItem('lastPingDay', today);
        console.log('track sent');
      } else {
        console.log('track http ' + req.status);
      }
    };
    req.onerror = function() { console.log('track err'); };
    req.ontimeout = function() { console.log('track timeout'); };
    req.send(JSON.stringify({ id: id, lat: rLat, lon: rLon, variant: 'app' }));
  } catch (e) {
    console.log('track exception: ' + e.message);
  }
}

// Hand-rolled base64 decoder for PKJS runtimes lacking `atob`.
function decodeBase64(s) {
  var chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
  var lookup = {};
  for (var i = 0; i < chars.length; i++) lookup[chars.charAt(i)] = i;
  var out = '';
  s = String(s).replace(/[^A-Za-z0-9+/]/g, '');
  for (var j = 0; j < s.length; j += 4) {
    var n = (lookup[s.charAt(j)] << 18) |
            (lookup[s.charAt(j + 1)] << 12) |
            ((lookup[s.charAt(j + 2)] || 0) << 6) |
            (lookup[s.charAt(j + 3)] || 0);
    out += String.fromCharCode((n >> 16) & 0xff);
    if (s.charAt(j + 2) !== '=' && s.charAt(j + 2) !== '')
      out += String.fromCharCode((n >> 8) & 0xff);
    if (s.charAt(j + 3) !== '=' && s.charAt(j + 3) !== '')
      out += String.fromCharCode(n & 0xff);
  }
  return out;
}

function getRadarProxyURL() {
  return RADAR_PROXY_URL;
}

function sendRadarStatus(status) {
  Pebble.sendAppMessage({ RadarStatus: status });
}

var RADAR_CHUNK_MAX_TRIES = 3;
function sendRadarChunk(chunkIdx, total, w, h, ts, byteArr, onAllDone) {
  var tries = 0;
  function send() {
    tries++;
    var msg = {
      RadarChunkIdx: chunkIdx,
      RadarChunkTotal: total,
      RadarChunkData: byteArr,
      RadarWidth: w,
      RadarHeight: h,
      RadarTimestamp: ts,
    };
    Pebble.sendAppMessage(msg, function() {
      if (chunkIdx + 1 >= total) {
        if (onAllDone) onAllDone();
      }
    }, function(e) {
      // Retry after a brief pause; transient drop is common when the watch is
      // busy redrawing. Cap the attempts so a persistently flaky link can't
      // loop forever draining battery — after the last try, report the error
      // status so the watch UI resolves out of its loading state.
      if (tries < RADAR_CHUNK_MAX_TRIES) {
        console.log('radar chunk ' + chunkIdx + ' failed, retry ' + tries);
        setTimeout(send, 1500);
      } else {
        console.log('radar chunk ' + chunkIdx + ' failed after ' + tries + ' tries, giving up');
        sendRadarStatus(3);
      }
    });
  }
  send();
}

function fetchRadar() {
  var proxy = getRadarProxyURL();
  if (!proxy) {
    console.log('radar: no RadarProxyURL configured');
    sendRadarStatus(3);
    return;
  }
  if (lastLat === null || lastLon === null) {
    // Trigger geolocation, then radar.
    navigator.geolocation.getCurrentPosition(function(pos) {
      lastLat = pos.coords.latitude;
      lastLon = pos.coords.longitude;
      fetchRadar();
    }, function() {
      sendRadarStatus(3);
    }, { timeout: 15000, maximumAge: 600000 });
    return;
  }

  var url = proxy + (proxy.indexOf('?') >= 0 ? '&' : '?') +
            'lat=' + lastLat + '&lon=' + lastLon + '&format=base64';
  console.log('radar: fetching ' + url);

  var x = new XMLHttpRequest();
  x.open('GET', url, true);
  x.timeout = 25000;
  x.onload = function() {
    if (x.status !== 200) {
      console.log('radar: proxy HTTP ' + x.status);
      sendRadarStatus(3);
      return;
    }
    var ts = parseInt(x.getResponseHeader('X-Radar-Time') || '0', 10) || 0;
    // Decode base64 → byte array. PKJS may lack atob; fall back to a
    // hand-rolled decoder.
    var b64 = (x.responseText || '').trim();
    var bin;
    try {
      bin = (typeof atob === 'function')
        ? atob(b64)
        : decodeBase64(b64);
    } catch (err) {
      console.log('radar: base64 decode err ' + err.message);
      sendRadarStatus(3);
      return;
    }
    var total_bytes = bin.length;
    var w = 160, h = 160;
    if (total_bytes !== w * h) {
      console.log('radar: unexpected payload size ' + total_bytes);
    }
    var totalChunks = Math.ceil(total_bytes / RADAR_CHUNK_SIZE);
    console.log('radar: ' + total_bytes + 'B -> ' + totalChunks + ' chunks');

    function sendChunk(i) {
      if (i >= totalChunks) return;
      var start = i * RADAR_CHUNK_SIZE;
      var end = Math.min(start + RADAR_CHUNK_SIZE, total_bytes);
      var slice = new Array(end - start);
      for (var k = 0; k < slice.length; k++) slice[k] = bin.charCodeAt(start + k);
      sendRadarChunk(i, totalChunks, w, h, ts, slice, function() {
        // all done
      });
      setTimeout(function() { sendChunk(i + 1); }, 80);
    }
    sendChunk(0);
  };
  x.onerror = function() {
    console.log('radar: xhr error');
    sendRadarStatus(3);
  };
  x.ontimeout = function() {
    console.log('radar: xhr timeout');
    sendRadarStatus(3);
  };
  x.send();
}

Pebble.addEventListener('ready', function() {
  console.log('TouchyWeather PKJS ready');
  // Re-sync the configured background interval to the watch. Persist storage is
  // wiped on reinstall/update, resetting the C side to 0 (disabled), but Clay's
  // own localStorage still holds the user's choice. Without this, background
  // updates stay off until the user manually re-saves Clay. Reading Clay's
  // 'clay-settings' store (not a new key) means this also recovers users who
  // already updated to the buggy build.
  var interval = null;
  try {
    var saved = JSON.parse(localStorage.getItem('clay-settings') || '{}');
    if (saved.BackgroundUpdateInterval !== undefined) {
      interval = parseInt(saved.BackgroundUpdateInterval, 10) || 0;
    }
  } catch (e) { /* corrupt store -> skip resync */ }

  if (interval !== null) {
    console.log('Re-syncing BackgroundUpdateInterval=' + interval + ' to watch');
    Pebble.sendAppMessage({ BackgroundUpdateInterval: interval },
      function() { maybeInitialFetch(); },
      function() { maybeInitialFetch(); });
  } else {
    maybeInitialFetch();
  }
});

// Watch→Clay seed: message keys the watch pushes to describe its current card
// config. Cached in localStorage and injected into Clay's persisted settings
// right before the config page opens, so Clay always shows the watch's TRUE
// state (on-watch toggles/reorders are no longer wiped by a Clay save).
var WATCH_CARD_STATE_KEYS = [
  'CardOrder', 'PhoneManagesCards', 'HideSettingsCard',
  'CardEnabledHours', 'CardEnabledWeek', 'CardEnabledPrecip', 'CardEnabledUV',
  'CardEnabledAQ', 'CardEnabledSun', 'CardEnabledNight', 'CardEnabledGolden',
  'CardEnabledRadar', 'CardEnabledAdvice'
];

// D1: the four small-screen platforms compile out the on-watch card editor
// and force PhoneManagesCards on. Mirror that phone-side so the cached seed
// can never open the Clay page with the card editor disabled.
function isSmallScreenWatch() {
  var info = null;
  try {
    info = Pebble.getActiveWatchInfo && Pebble.getActiveWatchInfo();
  } catch (e) { info = null; }
  var pf = (info && info.platform) || '';
  return pf === 'basalt' || pf === 'chalk' || pf === 'diorite' || pf === 'flint';
}

function cacheWatchCardState(p) {
  var state = {};
  try {
    state = JSON.parse(localStorage.getItem('watchCardState') || '{}');
  } catch (e) { state = {}; }
  for (var i = 0; i < WATCH_CARD_STATE_KEYS.length; i++) {
    var k = WATCH_CARD_STATE_KEYS[i];
    if (p[k] === undefined) continue;
    // CardOrder rides as a CSV string; everything else is a 0/1 flag that
    // Clay stores as a boolean (checkbox manipulator).
    state[k] = (k === 'CardOrder') ? String(p[k]) : !!p[k];
  }
  if (isSmallScreenWatch()) state.PhoneManagesCards = true;
  localStorage.setItem('watchCardState', JSON.stringify(state));
  console.log('watch card state cached: ' + JSON.stringify(state));
}

Pebble.addEventListener('appmessage', function(e) {
  var p = (e && e.payload) || {};
  if (p.RadarRequest) {
    console.log('appmessage: RadarRequest');
    fetchRadar();
    return;
  }
  // Watch card-state seed (identified by the CardOrder key, which only the
  // watch's state push carries watch→phone). Cache and stop — never fetch.
  if (p.CardOrder !== undefined) {
    cacheWatchCardState(p);
    return;
  }
  // The watch reports its system clock style with each refresh request so the
  // "Match watch" time format can follow it.
  if (p.ClockIs24h !== undefined) {
    localStorage.setItem('clockIs24h', p.ClockIs24h ? '1' : '0');
  }
  // Only fetch weather when explicitly requested via the LastUpdated sentinel.
  // The C side sends LastUpdated=1 for manual refresh or background wakeup.
  // Config messages (theme, toggles, etc.) should NOT trigger a fetch.
  if (p.LastUpdated !== undefined) {
    console.log('appmessage: LastUpdated sentinel, fetching weather');
    locateAndFetch();
  } else {
    console.log('appmessage: config message, no fetch');
  }
});

Pebble.addEventListener('showConfiguration', function() {
  // Inject the watch's cached card state into Clay's persisted settings so
  // the page opens showing the watch's true order/visibility. setSettings
  // writes the same 'clay-settings' store generateUrl() reads.
  try {
    var watchState = JSON.parse(localStorage.getItem('watchCardState') || 'null');
    if (watchState) {
      clay.setSettings(watchState);
      console.log('injected watch card state into Clay settings');
    }
  } catch (e) {
    console.log('watch card state inject skipped: ' + e.message);
  }
  Pebble.openURL(clay.generateUrl());
});

// The localStorage keys whose values are baked into the weather message
// phone-side (unit conversion, time formatting, flags that ride along and
// get cache-persisted on the watch only via a fetch). Only a change to one
// of these needs a refetch after a Clay save; theme/card/nav-only saves
// used to fire a full 3-call fetch for nothing.
function weatherRelevantSnapshot() {
  return [
    localStorage.getItem('units'),
    // Load-bearing: the wind unit is applied by the API, not by the watch, so
    // changing it without a refetch would relabel the SAME numbers — 12 km/h
    // would redraw as "12M/S", a gale. It also has to stay listed even though
    // 'auto' derives from units, because switching auto->ms changes the
    // request while `units` does not move.
    localStorage.getItem('windUnit'),
    localStorage.getItem('useDewPoint'),
    localStorage.getItem('showLocation'),
    localStorage.getItem('timeFormat'),
    localStorage.getItem('locationOverride')
  ].join('|');
}

Pebble.addEventListener('webviewclosed', function(e) {
  if (!e || !e.response) return;
  var beforeSave = weatherRelevantSnapshot();
  var dict = clay.getSettings(e.response, false);
  if (dict.Units !== undefined) {
    // Clay radiogroup values come back as strings ("0"/"1"), so coerce
    // before comparing. Prior versions used `=== 1` which always failed
    // and silently pinned the app to imperial.
    localStorage.setItem('units',
      parseInt(dict.Units.value, 10) === 1 ? 'metric' : 'imperial');
  }
  if (dict.WindSpeedUnit !== undefined) {
    // Clay select values come back as strings. Whitelist rather than store
    // verbatim: an unrecognised value would fall through getWindUnitPref() to
    // 'auto' on read anyway, but it would also reach the Open-Meteo URL from
    // here and fail the whole forecast request.
    var wsu = String(dict.WindSpeedUnit.value);
    localStorage.setItem('windUnit',
      (wsu === 'mph' || wsu === 'kmh' || wsu === 'ms') ? wsu : 'auto');
  }
  if (dict.UseDewPoint !== undefined) {
    localStorage.setItem('useDewPoint',
      dict.UseDewPoint.value ? '1' : '0');
  }
  if (dict.ShowLocation !== undefined) {
    localStorage.setItem('showLocation',
      dict.ShowLocation.value ? '1' : '0');
  }
  if (dict.TimeFormat !== undefined) {
    // "0" match watch, "1" 12-hour, "2" 24-hour. Stored as a string.
    localStorage.setItem('timeFormat',
      String(parseInt(dict.TimeFormat.value, 10) || 0));
  }
  if (dict.LocationOverride !== undefined && dict.LocationOverride.value) {
    localStorage.setItem('locationOverride', dict.LocationOverride.value);
  } else {
    localStorage.removeItem('locationOverride');
  }
  // Optimistically update the watch-state seed cache with what this save will
  // make the watch's card state, mirroring comm.c's gate logic exactly:
  // PhoneManagesCards + HideSettingsCard are stored on the watch
  // unconditionally; CardEnabled*/CardOrder apply only while the gate is ON.
  // Without this, reopening Clay before the watch pushes a fresh seed would
  // inject pre-save (stale) values over the settings just saved. The watch's
  // own re-push after applying (comm.c) remains the authoritative corrector.
  try {
    var wcs = JSON.parse(localStorage.getItem('watchCardState') || '{}');
    if (dict.PhoneManagesCards !== undefined) {
      wcs.PhoneManagesCards = !!dict.PhoneManagesCards.value;
    }
    // D1: on small-screen watches the gate is forced on watch-side; keep the
    // cache in agreement so the CardEnabled*/CardOrder writes below apply.
    if (isSmallScreenWatch()) wcs.PhoneManagesCards = true;
    if (dict.HideSettingsCard !== undefined) {
      wcs.HideSettingsCard = !!dict.HideSettingsCard.value;
    }
    if (wcs.PhoneManagesCards) {
      for (var wi = 0; wi < WATCH_CARD_STATE_KEYS.length; wi++) {
        var wk = WATCH_CARD_STATE_KEYS[wi];
        if (wk === 'PhoneManagesCards' || wk === 'HideSettingsCard') continue;
        if (dict[wk] === undefined) continue;
        wcs[wk] = (wk === 'CardOrder') ? String(dict[wk].value)
                                       : !!dict[wk].value;
      }
    }
    localStorage.setItem('watchCardState', JSON.stringify(wcs));
    console.log('watch card state cache updated from Clay save');
  } catch (err) {
    console.log('watch card state cache update skipped: ' + err.message);
  }
  // Refetch only when a weather-relevant setting actually changed. Not gated
  // by lastFetchAt — a units change needs freshly converted data regardless
  // of age. Kept in both send callbacks, as before.
  var needsFetch = (weatherRelevantSnapshot() !== beforeSave);
  function afterSave() {
    if (needsFetch) {
      locateAndFetch();
    } else {
      console.log('config saved, no weather-relevant change, skipping fetch');
    }
  }
  var msg = clay.getSettings(e.response);
  Pebble.sendAppMessage(msg, afterSave, afterSave);
});
