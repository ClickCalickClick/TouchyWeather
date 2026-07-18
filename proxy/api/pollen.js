// TouchyWeather Pollen Proxy — Vercel serverless function.
//
// Strategy (free-tier optimized):
//   1. Round coords to 0.1° (~11 km) so users in the same locality
//      share a cache key but the cell is small enough for accuracy
//      comparable to Open-Meteo's 11 km CAMS grid.
//   2. Always request 5 days from Google in one call (`days=5`) — same
//      quota cost as 1 day, but buys 5 days of data per cache miss.
//   3. Persist results in Vercel KV so the 5-day window survives CDN
//      edge evictions / cold starts. This gives a guaranteed ≤1
//      upstream call per cell per 5 days = ~6 calls/cell/month.
//   4. Per-day level = max(grass, tree, weed) UPI indexes so all three
//      categories contribute to the final score.
//   5. Stale-on-error: if Google fails but we have any prior KV data
//      for this cell, return that instead of a 502.
//
// Endpoint:  GET /api/pollen?lat=<f>&lon=<f>[&key=<auth-secret>]
//
// Response shape:
//   { level: 0..5, grass, tree, weed, date: 'YYYY-MM-DD', source }
//   { level: -1 } when region not covered (Google returns no data).
//
// Quota math: 5,000 Google free calls/mo ÷ (30 days / 5 days per call)
//   = 833 unique 11 km cells supported per month before quota exhaustion.
//
// KV is optional: if env vars are missing, the function falls back to
// CDN-only caching (still works, slightly less efficient).

const GOOGLE_ENDPOINT = 'https://pollen.googleapis.com/v1/forecast:lookup';
const FORECAST_DAYS = 5;
const CACHE_SECONDS = FORECAST_DAYS * 24 * 60 * 60; // 5d CDN cache
const KV_TTL_SECONDS = (FORECAST_DAYS + 1) * 24 * 60 * 60; // safety margin

// Lazy-load KV so missing env vars don't crash the function — the
// proxy still works as a CDN-only cache without persistent storage.
let _kv = null;
function getKv() {
  if (_kv !== null) return _kv;
  try {
    if (!process.env.KV_REST_API_URL && !process.env.KV_URL) {
      _kv = false;
      return false;
    }
    // eslint-disable-next-line global-require
    const { kv } = require('@vercel/kv');
    _kv = kv;
    return kv;
  } catch (e) {
    _kv = false;
    return false;
  }
}

// Round to nearest 0.1° (~11 km). Matches the CAMS European grid
// resolution Open-Meteo uses, giving comparable accuracy worldwide.
function roundCoord(v) {
  return Math.round(v * 10) / 10;
}

function pad2(n) { return n < 10 ? '0' + n : '' + n; }
function isoDate(y, m, d) { return y + '-' + pad2(m) + '-' + pad2(d); }
function todayIso() {
  const d = new Date();
  return isoDate(d.getUTCFullYear(), d.getUTCMonth() + 1, d.getUTCDate());
}

// Parse Google's dailyInfo into [{date, level, grass, tree, weed}].
// Level for each day = max of grass / tree / weed UPI values (0..5).
// Missing categories are treated as -1 (don't contribute).
function parseDailyInfo(dailyInfo) {
  const out = [];
  for (let i = 0; i < dailyInfo.length; i++) {
    const day = dailyInfo[i] || {};
    const date = day.date || {};
    const iso = (date.year && date.month && date.day)
      ? isoDate(date.year, date.month, date.day) : null;
    const types = day.pollenTypeInfo || [];
    let grass = -1, tree = -1, weed = -1;
    for (let t = 0; t < types.length; t++) {
      const info = types[t] && types[t].indexInfo;
      const val = info && typeof info.value === 'number' ? info.value : -1;
      if (types[t].code === 'GRASS') grass = val;
      else if (types[t].code === 'TREE') tree = val;
      else if (types[t].code === 'WEED') weed = val;
    }
    let level = -1;
    if (grass > level) level = grass;
    if (tree  > level) level = tree;
    if (weed  > level) level = weed;
    out.push({ date: iso, level, grass, tree, weed });
  }
  return out;
}

// Find the entry matching today's UTC date; fall back to first entry.
function pickToday(daily) {
  const today = todayIso();
  for (let i = 0; i < daily.length; i++) {
    if (daily[i].date === today) return daily[i];
  }
  return daily[0] || null;
}

async function fetchFromGoogle(apiKey, lat, lon) {
  const url = `${GOOGLE_ENDPOINT}?key=${encodeURIComponent(apiKey)}` +
    `&location.latitude=${lat}&location.longitude=${lon}` +
    `&days=${FORECAST_DAYS}`;
  const upstream = await fetch(url, {
    headers: { 'User-Agent': 'TouchyWeather/1.0' },
  });
  if (!upstream.ok) {
    if (upstream.status === 404) return { uncovered: true };
    const body = await upstream.text();
    const err = new Error(`google ${upstream.status}: ${body.slice(0, 200)}`);
    err.status = upstream.status;
    throw err;
  }
  const data = await upstream.json();
  const daily = parseDailyInfo((data && data.dailyInfo) || []);
  if (!daily.length) return { uncovered: true };
  return { daily, fetchedAt: Date.now() };
}

module.exports = async (req, res) => {
  // Accept the watch key (RADAR_SECRET) or the Mac key (RADAR_SECRET_MAC);
  // neither set ⇒ auth off (local dev / forks). See api-contract.md §5/§8.
  const allowedKeys = [process.env.RADAR_SECRET, process.env.RADAR_SECRET_MAC].filter(Boolean);
  if (allowedKeys.length && !allowedKeys.includes(req.query.key)) {
    res.status(401).send('unauthorized');
    return;
  }

  const apiKey = process.env.GOOGLE_POLLEN_KEY;
  if (!apiKey) {
    res.status(500).json({ error: 'GOOGLE_POLLEN_KEY not configured' });
    return;
  }

  const lat = parseFloat(req.query.lat);
  const lon = parseFloat(req.query.lon);
  if (!isFinite(lat) || !isFinite(lon)) {
    res.status(400).json({ error: 'lat/lon required' });
    return;
  }
  if (lat < -90 || lat > 90 || lon < -180 || lon > 180) {
    res.status(400).json({ error: 'lat/lon out of range' });
    return;
  }

  const rLat = roundCoord(lat);
  const rLon = roundCoord(lon);
  const cellKey = `pollen:${rLat.toFixed(1)},${rLon.toFixed(1)}`;

  const kv = getKv();
  let cached = null;
  if (kv) {
    try { cached = await kv.get(cellKey); } catch (_) { cached = null; }
  }

  // Cache valid if it covers today (i.e., today's date appears in the
  // stored daily array). Forecasts shift forward each day, so an entry
  // fetched on day N covers days N..N+4; on day N+5 it no longer
  // includes "today" and we refresh.
  let entry = cached;
  let source = 'kv-hit';
  const today = todayIso();
  const covers = entry && entry.daily &&
    entry.daily.some(function (d) { return d.date === today; });

  if (!covers) {
    try {
      const fresh = await fetchFromGoogle(apiKey, rLat, rLon);
      if (fresh.uncovered) {
        // Negative cache: store sentinel so we don't keep retrying
        // uncovered regions every request.
        if (kv) {
          try {
            await kv.set(cellKey,
              { uncovered: true, fetchedAt: Date.now() },
              { ex: KV_TTL_SECONDS });
          } catch (_) {}
        }
        res.setHeader('Cache-Control',
          `public, s-maxage=${CACHE_SECONDS}, stale-while-revalidate=3600, max-age=0`);
        res.setHeader('X-Pollen-Cell', `${rLat.toFixed(1)},${rLon.toFixed(1)}`);
        res.setHeader('X-Pollen-Source', 'google-uncovered');
        res.status(200).json({ level: -1 });
        return;
      }
      entry = fresh;
      source = 'google-fresh';
      if (kv) {
        try { await kv.set(cellKey, entry, { ex: KV_TTL_SECONDS }); } catch (_) {}
      }
    } catch (e) {
      // Stale-on-error: if we have *any* prior data, serve it instead
      // of failing. Better to show yesterday's pollen than no pollen.
      if (entry && entry.daily) {
        source = 'kv-stale-on-error';
      } else {
        res.status(502).json({ error: e.message || 'google fetch failed' });
        return;
      }
    }
  }

  if (entry && entry.uncovered) {
    res.setHeader('Cache-Control',
      `public, s-maxage=${CACHE_SECONDS}, stale-while-revalidate=3600, max-age=0`);
    res.setHeader('X-Pollen-Cell', `${rLat.toFixed(1)},${rLon.toFixed(1)}`);
    res.setHeader('X-Pollen-Source', 'kv-uncovered');
    res.status(200).json({ level: -1 });
    return;
  }

  const today_entry = pickToday(entry.daily);
  if (!today_entry) {
    res.status(200).json({ level: -1 });
    return;
  }

  res.setHeader('Cache-Control',
    `public, s-maxage=${CACHE_SECONDS}, stale-while-revalidate=3600, max-age=0`);
  res.setHeader('X-Pollen-Cell', `${rLat.toFixed(1)},${rLon.toFixed(1)}`);
  res.setHeader('X-Pollen-Source', source);
  res.status(200).json({
    level: today_entry.level,
    grass: today_entry.grass,
    tree:  today_entry.tree,
    weed:  today_entry.weed,
    date:  today_entry.date,
  });
};
