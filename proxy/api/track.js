// TouchyWeather anonymous analytics ingest — Vercel serverless function.
//
// Records a single active-user "ping" without storing anything identifiable:
//   1. The watch sends { id, lat, lon }. `id` is Pebble's anonymous account
//      token (no name/email/PII) or a random local fallback.
//   2. We SHA-256(id + ANALYTICS_SALT) one-way before it touches storage, so
//      the raw token is never persisted and hashes can't be precomputed.
//   3. The hash is added (SADD) to per-period Redis sets — day / ISO-week /
//      month / year. SCARD of each set = unique active users for that period
//      (DAU / WAU / MAU / YAU). Set math across periods gives retention.
//   4. A coarse ~11 km location cell is counted in a monthly geo hash for a
//      privacy-preserving heatmap (never linked to a user).
//
// Endpoint:  POST /api/track?key=<RADAR_SECRET>
//   body: { "id": "<token>", "lat": <num>, "lon": <num> }
// Response:  204 No Content (analytics is fire-and-forget on the client).
//
// KV is optional: without KV env vars the function no-ops gracefully, exactly
// like pollen.js, so a misconfigured deploy never errors the client.

const crypto = require('crypto');

// Lazy-load KV so missing env vars don't crash the function.
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

// TTLs (seconds): old buckets self-expire so storage stays bounded. Periods
// are kept long enough to compute trailing-window trends and retention.
const TTL_DAY = 45 * 24 * 60 * 60;        // ~45 days of daily sets
const TTL_WEEK = 400 * 24 * 60 * 60;      // ~13 months of weekly sets
const TTL_MONTH = 750 * 24 * 60 * 60;     // ~2 years of monthly sets
const TTL_YEAR = 5 * 365 * 24 * 60 * 60;  // ~5 years of yearly sets
const TTL_GEO = 400 * 24 * 60 * 60;       // ~13 months of monthly geo hashes

function pad2(n) { return n < 10 ? '0' + n : '' + n; }

// UTC period keys. ISO-week per ISO-8601 (weeks start Monday; week 1 contains
// the year's first Thursday) so WAU lines up with how calendars number weeks.
function periodKeys(d) {
  const y = d.getUTCFullYear();
  const m = d.getUTCMonth() + 1;
  const day = d.getUTCDate();

  // ISO week number + ISO week-year.
  const t = new Date(Date.UTC(y, d.getUTCMonth(), day));
  const dayNum = t.getUTCDay() || 7;           // Mon=1..Sun=7
  t.setUTCDate(t.getUTCDate() + 4 - dayNum);   // shift to the week's Thursday
  const isoYear = t.getUTCFullYear();
  const yearStart = new Date(Date.UTC(isoYear, 0, 1));
  const week = Math.ceil((((t - yearStart) / 86400000) + 1) / 7);

  return {
    day: `${y}-${pad2(m)}-${pad2(day)}`,
    week: `${isoYear}-W${pad2(week)}`,
    month: `${y}-${pad2(m)}`,
    year: `${y}`,
  };
}

function roundCoord(v) { return Math.round(v * 10) / 10; }

module.exports = async (req, res) => {
  const secret = process.env.RADAR_SECRET;
  if (secret && req.query.key !== secret) {
    res.status(401).send('unauthorized');
    return;
  }
  if (req.method !== 'POST') {
    res.status(405).send('method not allowed');
    return;
  }

  // Vercel parses JSON bodies automatically, but tolerate a raw string too.
  let body = req.body;
  if (typeof body === 'string') {
    try { body = JSON.parse(body); } catch (_) { body = null; }
  }
  body = body || {};

  const id = typeof body.id === 'string' ? body.id.trim() : '';
  if (!id) {
    // Nothing to count without an id; ack so the client doesn't retry forever.
    res.status(204).end();
    return;
  }

  const kv = getKv();
  if (!kv) {
    // No storage configured — accept and drop (matches pollen.js degradation).
    res.status(204).end();
    return;
  }

  const salt = process.env.ANALYTICS_SALT || '';
  const hash = crypto.createHash('sha256').update(id + salt).digest('hex');

  const now = new Date();
  const p = periodKeys(now);

  try {
    await Promise.all([
      kv.sadd(`users:day:${p.day}`, hash),
      kv.sadd(`users:week:${p.week}`, hash),
      kv.sadd(`users:month:${p.month}`, hash),
      kv.sadd(`users:year:${p.year}`, hash),
      kv.incr(`hits:day:${p.day}`), // total pings (volume) vs unique users
    ]);
    // Expirations are best-effort; a missed one only delays self-cleanup.
    await Promise.all([
      kv.expire(`users:day:${p.day}`, TTL_DAY),
      kv.expire(`users:week:${p.week}`, TTL_WEEK),
      kv.expire(`users:month:${p.month}`, TTL_MONTH),
      kv.expire(`users:year:${p.year}`, TTL_YEAR),
      kv.expire(`hits:day:${p.day}`, TTL_DAY),
    ]);

    // Coarse heatmap: count the ~11 km cell for this month. Only aggregate
    // per-cell counts are stored, never tied to a user/hash.
    const lat = parseFloat(body.lat);
    const lon = parseFloat(body.lon);
    if (isFinite(lat) && isFinite(lon) &&
        lat >= -90 && lat <= 90 && lon >= -180 && lon <= 180) {
      const cell = `${roundCoord(lat).toFixed(1)},${roundCoord(lon).toFixed(1)}`;
      await kv.hincrby(`geo:${p.month}`, cell, 1);
      await kv.expire(`geo:${p.month}`, TTL_GEO);
    }
  } catch (e) {
    // Never surface storage errors to the watch — analytics is best-effort.
    console.log('track kv error: ' + (e && e.message));
  }

  res.status(204).end();
};
