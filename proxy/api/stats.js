// TouchyWeather private analytics dashboard — Vercel serverless function.
//
// Reads the aggregate sets written by track.js and reports unique active
// users (DAU / WAU / MAU / YAU), a trailing trend, simple retention, and a
// coarse location heatmap.
//
// PRIVACY / ACCESS: gated by STATS_SECRET — a SEPARATE admin-only key that is
// NEVER shipped to any device. (The RADAR_SECRET embedded in the watch app is
// extractable from the JS bundle and must not protect this read endpoint.)
// Without STATS_SECRET set, the endpoint refuses all access rather than
// exposing data, so a half-configured deploy can't leak numbers.
//
// Endpoint:
//   GET /api/stats?key=<STATS_SECRET>              -> HTML dashboard
//   GET /api/stats?key=<STATS_SECRET>&format=json  -> JSON for scripting

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

function pad2(n) { return n < 10 ? '0' + n : '' + n; }

function dayKey(d) {
  return `${d.getUTCFullYear()}-${pad2(d.getUTCMonth() + 1)}-${pad2(d.getUTCDate())}`;
}
function monthKey(d) {
  return `${d.getUTCFullYear()}-${pad2(d.getUTCMonth() + 1)}`;
}
function isoWeekKey(d) {
  const t = new Date(Date.UTC(d.getUTCFullYear(), d.getUTCMonth(), d.getUTCDate()));
  const dayNum = t.getUTCDay() || 7;
  t.setUTCDate(t.getUTCDate() + 4 - dayNum);
  const isoYear = t.getUTCFullYear();
  const yearStart = new Date(Date.UTC(isoYear, 0, 1));
  const week = Math.ceil((((t - yearStart) / 86400000) + 1) / 7);
  return `${isoYear}-W${pad2(week)}`;
}

function addDaysUTC(d, n) {
  return new Date(d.getTime() + n * 86400000);
}

async function scard(kv, key) {
  try { return (await kv.scard(key)) || 0; } catch (_) { return 0; }
}

module.exports = async (req, res) => {
  const secret = process.env.STATS_SECRET;
  // Fail closed: no admin secret configured => no access, ever.
  if (!secret || req.query.key !== secret) {
    res.setHeader('Cache-Control', 'no-store');
    res.status(401).send('unauthorized');
    return;
  }

  res.setHeader('Cache-Control', 'no-store');

  const kv = getKv();
  if (!kv) {
    res.status(500).json({ error: 'KV not configured' });
    return;
  }

  const now = new Date();

  // Headline numbers.
  const dau = await scard(kv, `users:day:${dayKey(now)}`);
  const wau = await scard(kv, `users:week:${isoWeekKey(now)}`);
  const mau = await scard(kv, `users:month:${monthKey(now)}`);
  const yau = await scard(kv, `users:year:${now.getUTCFullYear()}`);

  // Trailing 14-day DAU trend.
  const dailyTrend = [];
  for (let i = 13; i >= 0; i--) {
    const d = addDaysUTC(now, -i);
    const k = dayKey(d);
    dailyTrend.push({ label: k, value: await scard(kv, `users:day:${k}`) });
  }

  // Trailing 8-week WAU trend.
  const weeklyTrend = [];
  for (let i = 7; i >= 0; i--) {
    const d = addDaysUTC(now, -i * 7);
    const k = isoWeekKey(d);
    weeklyTrend.push({ label: k, value: await scard(kv, `users:week:${k}`) });
  }

  // Trailing 6-month MAU trend.
  const monthlyTrend = [];
  for (let i = 5; i >= 0; i--) {
    const d = new Date(Date.UTC(now.getUTCFullYear(), now.getUTCMonth() - i, 1));
    const k = monthKey(d);
    monthlyTrend.push({ label: k, value: await scard(kv, `users:month:${k}`) });
  }

  // Month-over-month retention: of this month's users, how many were also
  // active last month (returning) vs brand-new this month.
  const thisMonthKey = `users:month:${monthKey(now)}`;
  const lastMonthDate = new Date(Date.UTC(now.getUTCFullYear(), now.getUTCMonth() - 1, 1));
  const lastMonthKey = `users:month:${monthKey(lastMonthDate)}`;
  let returning = 0;
  try {
    // SINTERCARD (Redis 7+) avoids materializing the intersection.
    returning = (await kv.sintercard(2, [thisMonthKey, lastMonthKey])) || 0;
  } catch (_) {
    try {
      const inter = await kv.sinter(thisMonthKey, lastMonthKey);
      returning = Array.isArray(inter) ? inter.length : 0;
    } catch (__) { returning = 0; }
  }
  const newThisMonth = Math.max(0, mau - returning);

  // Coarse location heatmap for the current month.
  let geo = {};
  try { geo = (await kv.hgetall(`geo:${monthKey(now)}`)) || {}; } catch (_) { geo = {}; }
  const geoCells = Object.keys(geo).map(function (cell) {
    const parts = cell.split(',');
    return {
      lat: parseFloat(parts[0]),
      lon: parseFloat(parts[1]),
      count: parseInt(geo[cell], 10) || 0,
    };
  }).filter(function (c) { return isFinite(c.lat) && isFinite(c.lon); });

  const data = {
    asOf: now.toISOString(),
    headline: { dau, wau, mau, yau },
    retention: { mau, returning, newThisMonth },
    dailyTrend, weeklyTrend, monthlyTrend,
    geoCells,
  };

  if (req.query.format === 'json') {
    res.status(200).json(data);
    return;
  }

  res.setHeader('Content-Type', 'text/html; charset=utf-8');
  res.status(200).send(renderHtml(data));
};

function esc(s) {
  return String(s).replace(/[&<>"]/g, function (c) {
    return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c];
  });
}

function bars(trend) {
  const max = trend.reduce(function (m, t) { return Math.max(m, t.value); }, 1);
  return trend.map(function (t) {
    const h = Math.round((t.value / max) * 80);
    return '<div class="bar" title="' + esc(t.label) + ': ' + t.value + '">' +
      '<div class="fill" style="height:' + h + 'px"></div>' +
      '<div class="bl">' + esc(t.label.slice(5)) + '</div>' +
      '<div class="bv">' + t.value + '</div></div>';
  }).join('');
}

function renderHtml(d) {
  const h = d.headline;
  // Geo cells are handed to a Leaflet map rendered client-side. Escape '<'
  // so the JSON can't break out of the inline <script>.
  const geoJson = JSON.stringify(d.geoCells).replace(/</g, '\\u003c');

  return '<!doctype html><html><head><meta charset="utf-8">' +
'<meta name="viewport" content="width=device-width,initial-scale=1">' +
'<meta name="robots" content="noindex,nofollow">' +
'<link rel="stylesheet" href="https://unpkg.com/leaflet@1.9.4/dist/leaflet.css">' +
'<script src="https://unpkg.com/leaflet@1.9.4/dist/leaflet.js"></scr' + 'ipt>' +
'<title>TouchyWeather Analytics</title><style>' +
'body{font:14px/1.5 -apple-system,Segoe UI,Roboto,sans-serif;margin:0;background:#0f1115;color:#e8eaed}' +
'.wrap{max-width:900px;margin:0 auto;padding:24px}' +
'h1{font-size:20px;margin:0 0 4px}.sub{color:#9aa0a6;font-size:12px;margin-bottom:24px}' +
'.cards{display:flex;gap:12px;flex-wrap:wrap;margin-bottom:28px}' +
'.card{flex:1;min-width:120px;background:#1a1d23;border:1px solid #2a2e36;border-radius:10px;padding:16px}' +
'.card .n{font-size:32px;font-weight:600}.card .l{color:#9aa0a6;font-size:12px;text-transform:uppercase;letter-spacing:.5px}' +
'h2{font-size:14px;color:#cdd1d6;margin:24px 0 10px;border-bottom:1px solid #2a2e36;padding-bottom:6px}' +
'.chart{display:flex;align-items:flex-end;gap:6px;height:120px;overflow-x:auto}' +
'.bar{display:flex;flex-direction:column;align-items:center;justify-content:flex-end;min-width:34px}' +
'.fill{width:22px;background:linear-gradient(#5b9dff,#3367d6);border-radius:3px 3px 0 0}' +
'.bl{font-size:9px;color:#8a8f96;margin-top:4px;white-space:nowrap}.bv{font-size:10px;color:#cdd1d6}' +
'.ret{display:flex;gap:12px}.ret .card{text-align:center}' +
'#map{width:100%;height:420px;border:1px solid #2a2e36;border-radius:10px;background:#11151c}' +
'.leaflet-popup-content{color:#111}' +
'</style></head><body><div class="wrap">' +
'<h1>TouchyWeather — Active Users</h1>' +
'<div class="sub">As of ' + esc(d.asOf) + ' (UTC). Private dashboard — anonymous, aggregate counts only.</div>' +
'<div class="cards">' +
'<div class="card"><div class="n">' + h.dau + '</div><div class="l">Daily (today)</div></div>' +
'<div class="card"><div class="n">' + h.wau + '</div><div class="l">Weekly</div></div>' +
'<div class="card"><div class="n">' + h.mau + '</div><div class="l">Monthly</div></div>' +
'<div class="card"><div class="n">' + h.yau + '</div><div class="l">Yearly</div></div>' +
'</div>' +
'<h2>Daily active users — last 14 days</h2><div class="chart">' + bars(d.dailyTrend) + '</div>' +
'<h2>Weekly active users — last 8 weeks</h2><div class="chart">' + bars(d.weeklyTrend) + '</div>' +
'<h2>Monthly active users — last 6 months</h2><div class="chart">' + bars(d.monthlyTrend) + '</div>' +
'<h2>This month: new vs returning</h2><div class="ret">' +
'<div class="card"><div class="n">' + d.retention.newThisMonth + '</div><div class="l">New</div></div>' +
'<div class="card"><div class="n">' + d.retention.returning + '</div><div class="l">Returning</div></div>' +
'<div class="card"><div class="n">' + d.retention.mau + '</div><div class="l">Total MAU</div></div>' +
'</div>' +
'<h2>Where users are this month (~11 km cells)</h2><div id="map"></div>' +
'<div class="sub" style="margin-top:8px">' + d.geoCells.length + ' distinct cells.</div>' +
'</div>' +
'<script>(function(){' +
'var cells=' + geoJson + ';' +
'if(!window.L){document.getElementById("map").innerHTML=' +
'"<div style=\\"padding:16px;color:#9aa0a6\\">Map library failed to load (need internet access).</div>";return;}' +
'var map=L.map("map",{worldCopyJump:true}).setView([20,0],1);' +
'L.tileLayer("https://{s}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}{r}.png",' +
'{attribution:"\\u00a9 OpenStreetMap, \\u00a9 CARTO",maxZoom:19,subdomains:"abcd"}).addTo(map);' +
'var max=cells.reduce(function(m,c){return Math.max(m,c.count);},1);' +
'var pts=[];' +
'cells.forEach(function(c){' +
'var r=6+14*(c.count/max);' +
'L.circleMarker([c.lat,c.lon],{radius:r,color:"#5b9dff",fillColor:"#5b9dff",fillOpacity:.5,weight:1})' +
'.bindPopup(c.lat.toFixed(1)+", "+c.lon.toFixed(1)+"<br>"+c.count+" user"+(c.count===1?"":"s")).addTo(map);' +
'pts.push([c.lat,c.lon]);' +
'});' +
'if(pts.length){map.fitBounds(pts,{maxZoom:8,padding:[40,40]});}' +
'})();</scr' + 'ipt>' +
'</body></html>';
}
