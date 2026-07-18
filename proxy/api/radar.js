// TouchyWeather Radar Proxy — Vercel serverless function.
//
// Returns a 160x160 raw Pebble 8-bit pixel buffer (25,600 bytes) for the
// requested lat/lon, with a high-detail Carto Voyager basemap composited
// under the latest RainViewer radar frame.
//
// The basemap is rendered at BASE_ZOOM (high detail: roads, coastlines,
// labels) and the radar overlay (RainViewer caps at z=7 for 256px tiles)
// is fetched at RADAR_ZOOM and upsampled RADAR_SCALE× with cubic
// resampling to align pixel-for-pixel.
//
// Pebble 8-bit format per pixel: 0b11RRGGBB (R/G/B each 2 bits).
//
// Endpoint:  GET /api/radar?lat=<f>&lon=<f>[&format=base64][&debug=...]
//
// Auth: if the RADAR_SECRET environment variable is set, all requests must
// include a matching ?key= query parameter or they receive HTTP 401.
// Set RADAR_SECRET in the Vercel project dashboard (not in source).
//
// Response headers:
//   X-Radar-Time: unix seconds of the radar frame used
//   Cache-Control: public, max-age=300

const sharp = require('sharp');

const TILE_SIZE = 256;
const BASE_ZOOM = 9;     // high-detail basemap (~30 mi / 50 km wide window)
const RADAR_ZOOM = 7;    // RainViewer 256px max
const RADAR_SCALE = 1 << (BASE_ZOOM - RADAR_ZOOM); // 4

const OUT_W = 160;
const OUT_H = 160;

// Backgrounds for makeLayer. Basemap canvas opaque earthtone so missing
// tiles look neutral; radar canvas fully transparent so non-precip areas
// don't cover the basemap below.
const BASE_BG = { r: 220, g: 220, b: 220, alpha: 1 };
const TRANSPARENT_BG = { r: 0, g: 0, b: 0, alpha: 0 };

// Carto Voyager raster tiles (CC-BY OpenStreetMap).
// voyager_nolabels  — roads, water, terrain without any text labels.
// voyager_only_labels — just the city/road label overlay, transparent BG.
// Splitting them lets us sandwich the radar between map geometry and
// labels so text is always legible regardless of precipitation intensity.
function basemapUrl(z, x, y) {
  return `https://a.basemaps.cartocdn.com/rastertiles/voyager_nolabels/${z}/${x}/${y}.png`;
}
function labelsUrl(z, x, y) {
  return `https://a.basemaps.cartocdn.com/rastertiles/voyager_only_labels/${z}/${x}/${y}.png`;
}

// RainViewer radar tile. Color scheme 2 = Universal Blue, options 1_1 =
// smoothed + snow visible.
function radarTileUrl(host, path, z, x, y) {
  return `${host}${path}/256/${z}/${x}/${y}/2/1_1.png`;
}

// Web-Mercator lon/lat -> fractional tile XY at zoom z.
function lonLatToTileXY(lon, lat, z) {
  const n = Math.pow(2, z);
  const x = ((lon + 180) / 360) * n;
  const latRad = (lat * Math.PI) / 180;
  const y =
    ((1 - Math.log(Math.tan(latRad) + 1 / Math.cos(latRad)) / Math.PI) / 2) *
    n;
  return { x, y };
}

async function fetchPng(url) {
  const res = await fetch(url, {
    headers: { 'User-Agent': 'TouchyWeather/1.0 (+https://github.com)' },
  });
  if (!res.ok) throw new Error(`HTTP ${res.status} for ${url}`);
  const buf = Buffer.from(await res.arrayBuffer());
  return buf;
}

// RainViewer returns a small (~1.3 KB) "Zoom Level Not Supported"
// placeholder PNG with HTTP 200 for unsupported zoom/area combos.
// Treat any radar tile under this threshold as empty (transparent).
const RADAR_PLACEHOLDER_MAX_BYTES = 1500;

// Quantize an RGBA buffer (length = w*h*4) to Pebble 8-bit, returning a
// Buffer of length w*h. Each output byte = 0b11RRGGBB (R/G/B 2 bits each).
function quantizeToPebble8(rgba, w, h) {
  const out = Buffer.alloc(w * h);
  for (let i = 0, j = 0; j < out.length; i += 4, j++) {
    const r = rgba[i];
    const g = rgba[i + 1];
    const b = rgba[i + 2];
    out[j] = 0xc0 | ((r >> 6) << 4) | ((g >> 6) << 2) | (b >> 6);
  }
  return out;
}

// Generic stitcher: lay out `placedTiles` (each {buf, left, top}) onto a
// (canvasW × canvasH) canvas with `bg` background, then extract a
// (cropW × cropH) window at integer (cropX, cropY).
async function stitchAndCrop(placedTiles, canvasW, canvasH, bg, cropX, cropY, cropW, cropH) {
  const composites = placedTiles
    .filter((t) => t.buf)
    .map((t) => ({ input: t.buf, left: t.left, top: t.top }));
  const canvas = await sharp({
    create: { width: canvasW, height: canvasH, channels: 4, background: bg },
  })
    .composite(composites)
    .png()
    .toBuffer();
  return await sharp(canvas)
    .extract({ left: cropX, top: cropY, width: cropW, height: cropH })
    .png()
    .toBuffer();
}

// Build the OUT_W × OUT_H basemap layer at BASE_ZOOM, centered on the
// user's exact lat/lon. Fetches a 3×3 tile window so the user's pixel is
// always at least TILE_SIZE pixels from any edge of the canvas.
async function makeBaseLayer(lon, lat, log) {
  const t = lonLatToTileXY(lon, lat, BASE_ZOOM);
  // 3×3 window: center tile = floor(t.x), floor(t.y).
  const cx = Math.floor(t.x);
  const cy = Math.floor(t.y);
  const txTL = cx - 1;
  const tyTL = cy - 1;
  const fracX = t.x - txTL; // 1..2
  const fracY = t.y - tyTL;

  const placed = [];
  await Promise.all((function () {
    const ps = [];
    for (let dy = 0; dy < 3; dy++) {
      for (let dx = 0; dx < 3; dx++) {
        const xi = txTL + dx;
        const yi = tyTL + dy;
        ps.push(
          fetchPng(basemapUrl(BASE_ZOOM, xi, yi))
            .then((b) => {
              placed.push({ buf: b, left: dx * TILE_SIZE, top: dy * TILE_SIZE });
              if (log) log('base ok', { xi, yi, len: b.length });
            })
            .catch((e) => {
              placed.push({ buf: null, left: dx * TILE_SIZE, top: dy * TILE_SIZE });
              if (log) log('base FAIL', { xi, yi, err: String(e && e.message || e) });
            }),
        );
      }
    }
    return ps;
  })());

  const userPxX = Math.round(TILE_SIZE * fracX);
  const userPxY = Math.round(TILE_SIZE * fracY);
  const cropX = Math.max(0, Math.min(3 * TILE_SIZE - OUT_W, userPxX - OUT_W / 2));
  const cropY = Math.max(0, Math.min(3 * TILE_SIZE - OUT_H, userPxY - OUT_H / 2));
  return stitchAndCrop(placed, 3 * TILE_SIZE, 3 * TILE_SIZE, BASE_BG,
                       cropX, cropY, OUT_W, OUT_H);
}

// Build the OUT_W × OUT_H label overlay (transparent background) aligned
// to the same window as makeBaseLayer. Composited on top of everything so
// city/road labels remain legible even under heavy radar precipitation.
async function makeLabelsLayer(lon, lat, log) {
  const t = lonLatToTileXY(lon, lat, BASE_ZOOM);
  const cx = Math.floor(t.x);
  const cy = Math.floor(t.y);
  const txTL = cx - 1;
  const tyTL = cy - 1;
  const fracX = t.x - txTL;
  const fracY = t.y - tyTL;

  const placed = [];
  await Promise.all((function () {
    const ps = [];
    for (let dy = 0; dy < 3; dy++) {
      for (let dx = 0; dx < 3; dx++) {
        const xi = txTL + dx;
        const yi = tyTL + dy;
        ps.push(
          fetchPng(labelsUrl(BASE_ZOOM, xi, yi))
            .then((b) => {
              placed.push({ buf: b, left: dx * TILE_SIZE, top: dy * TILE_SIZE });
              if (log) log('lbl ok', { xi, yi, len: b.length });
            })
            .catch((e) => {
              placed.push({ buf: null, left: dx * TILE_SIZE, top: dy * TILE_SIZE });
              if (log) log('lbl FAIL', { xi, yi, err: String(e && e.message || e) });
            }),
        );
      }
    }
    return ps;
  })());

  const userPxX = Math.round(TILE_SIZE * fracX);
  const userPxY = Math.round(TILE_SIZE * fracY);
  const cropX = Math.max(0, Math.min(3 * TILE_SIZE - OUT_W, userPxX - OUT_W / 2));
  const cropY = Math.max(0, Math.min(3 * TILE_SIZE - OUT_H, userPxY - OUT_H / 2));
  return stitchAndCrop(placed, 3 * TILE_SIZE, 3 * TILE_SIZE, TRANSPARENT_BG,
                       cropX, cropY, OUT_W, OUT_H);
}

// Build the OUT_W × OUT_H radar overlay aligned to the same lat/lon
// window as the basemap. Radar is sourced at RADAR_ZOOM and upscaled
// RADAR_SCALE× with cubic resampling.
async function makeRadarLayer(idx, frame, lon, lat, log) {
  // User's pixel position at BASE_ZOOM, then projected to RADAR_ZOOM.
  const baseT = lonLatToTileXY(lon, lat, BASE_ZOOM);
  const userBaseX = baseT.x * TILE_SIZE;
  const userBaseY = baseT.y * TILE_SIZE;
  const baseCropX0 = userBaseX - OUT_W / 2;
  const baseCropY0 = userBaseY - OUT_H / 2;
  // Equivalent crop window in RADAR_ZOOM pixel space.
  const radCropX0 = baseCropX0 / RADAR_SCALE;
  const radCropY0 = baseCropY0 / RADAR_SCALE;
  const radCropW = OUT_W / RADAR_SCALE; // 40
  const radCropH = OUT_H / RADAR_SCALE; // 40

  const txTL = Math.floor(radCropX0 / TILE_SIZE);
  const tyTL = Math.floor(radCropY0 / TILE_SIZE);
  const txBR = Math.floor((radCropX0 + radCropW) / TILE_SIZE);
  const tyBR = Math.floor((radCropY0 + radCropH) / TILE_SIZE);
  const tilesW = txBR - txTL + 1;
  const tilesH = tyBR - tyTL + 1;

  const placed = [];
  await Promise.all((function () {
    const ps = [];
    for (let dy = 0; dy < tilesH; dy++) {
      for (let dx = 0; dx < tilesW; dx++) {
        const xi = txTL + dx;
        const yi = tyTL + dy;
        ps.push(
          fetchPng(radarTileUrl(idx.host, frame.path, RADAR_ZOOM, xi, yi))
            .then((b) => {
              if (b.length <= RADAR_PLACEHOLDER_MAX_BYTES) {
                placed.push({ buf: null, left: dx * TILE_SIZE, top: dy * TILE_SIZE });
                if (log) log('rad placeholder', { xi, yi, len: b.length });
              } else {
                placed.push({ buf: b, left: dx * TILE_SIZE, top: dy * TILE_SIZE });
                if (log) log('rad ok', { xi, yi, len: b.length });
              }
            })
            .catch((e) => {
              placed.push({ buf: null, left: dx * TILE_SIZE, top: dy * TILE_SIZE });
              if (log) log('rad FAIL', { xi, yi, err: String(e && e.message || e) });
            }),
        );
      }
    }
    return ps;
  })());

  const stitchW = tilesW * TILE_SIZE;
  const stitchH = tilesH * TILE_SIZE;
  const localX = radCropX0 - txTL * TILE_SIZE;
  const localY = radCropY0 - tyTL * TILE_SIZE;
  // Integer-pixel box that covers the float window, with a 1px guard.
  const extL = Math.max(0, Math.floor(localX) - 1);
  const extT = Math.max(0, Math.floor(localY) - 1);
  let extW = Math.ceil(radCropW + (localX - extL)) + 1;
  let extH = Math.ceil(radCropH + (localY - extT)) + 1;
  if (extL + extW > stitchW) extW = stitchW - extL;
  if (extT + extH > stitchH) extH = stitchH - extT;

  const stitched = await stitchAndCrop(placed, stitchW, stitchH, TRANSPARENT_BG,
                                       extL, extT, extW, extH);
  // Upscale RADAR_SCALE× with cubic resampling.
  const upscaled = await sharp(stitched)
    .resize({ width: extW * RADAR_SCALE, height: extH * RADAR_SCALE, kernel: 'cubic' })
    .png()
    .toBuffer();

  // Final extract to OUT_W × OUT_H accounting for the sub-pixel guard.
  const subL = Math.max(0, Math.round((localX - extL) * RADAR_SCALE));
  const subT = Math.max(0, Math.round((localY - extT) * RADAR_SCALE));
  return await sharp(upscaled)
    .extract({ left: subL, top: subT, width: OUT_W, height: OUT_H })
    .png()
    .toBuffer();
}

module.exports = async (req, res) => {
  // Optional shared-secret auth. Set RADAR_SECRET (watch) and/or
  // RADAR_SECRET_MAC (Mac app, revocable independently — api-contract.md §5/§8)
  // in the Vercel dashboard; omit both to run without auth (local dev / forks).
  const allowedKeys = [process.env.RADAR_SECRET, process.env.RADAR_SECRET_MAC].filter(Boolean);
  if (allowedKeys.length && !allowedKeys.includes(req.query.key)) {
    res.status(401).send('unauthorized');
    return;
  }

  const debug = req.query.debug;
  const diag = { steps: [] };
  const log = (s, extra) => diag.steps.push(extra ? { s, ...extra } : { s });
  try {
    const lat = parseFloat(req.query.lat);
    const lon = parseFloat(req.query.lon);
    if (!isFinite(lat) || !isFinite(lon)) {
      res.status(400).send('lat/lon required');
      return;
    }
    if (lat < -90 || lat > 90 || lon < -180 || lon > 180) {
      res.status(400).send('lat/lon out of range');
      return;
    }
    log('parsed', { lat, lon });

    const idxRes = await fetch('https://api.rainviewer.com/public/weather-maps.json');
    if (!idxRes.ok) throw new Error(`rainviewer index ${idxRes.status}`);
    const idx = await idxRes.json();
    const past = (idx.radar && idx.radar.past) || [];
    if (!past.length) throw new Error('no radar frames');
    const frame = past[past.length - 1];
    log('rv frame', { time: frame.time });

    const [baseCroppedRaw, radCropped, labelsCropped] = await Promise.all([
      makeBaseLayer(lon, lat, log),
      makeRadarLayer(idx, frame, lon, lat, log),
      makeLabelsLayer(lon, lat, log),
    ]);
    // Darken the basemap so map geometry is visually recessed behind the
    // radar overlay and labels. brightness < 1.0 scales all channels down.
    // 0.72 ≈ 28% darker — enough to read as "background" on the small screen
    // without losing road/coastline detail.
    const baseCropped = await sharp(baseCroppedRaw)
      .modulate({ brightness: 0.72 })
      .png()
      .toBuffer();

    if (debug === '1') {
      res.setHeader('Content-Type', 'application/json');
      res.status(200).send(JSON.stringify({ frame, idxHost: idx.host, BASE_ZOOM, RADAR_ZOOM, diag }, null, 2));
      return;
    }
    if (debug === 'base') {
      res.setHeader('Content-Type', 'image/png');
      res.status(200).send(baseCropped);
      return;
    }
    if (debug === 'rad') {
      res.setHeader('Content-Type', 'image/png');
      res.status(200).send(radCropped);
      return;
    }
    if (debug === 'labels') {
      res.setHeader('Content-Type', 'image/png');
      res.status(200).send(labelsCropped);
      return;
    }
    if (debug === 'composed') {
      const out = await sharp(baseCropped)
        .composite([
          { input: radCropped, blend: 'over', opacity: 0.6 },
          { input: labelsCropped, blend: 'over' },
        ])
        .png()
        .toBuffer();
      res.setHeader('Content-Type', 'image/png');
      res.status(200).send(out);
      return;
    }

    const composedRgba = await sharp(baseCropped)
      .composite([
        { input: radCropped, blend: 'over', opacity: 0.6 },
        { input: labelsCropped, blend: 'over' },
      ])
      .ensureAlpha()
      .raw()
      .toBuffer({ resolveWithObject: true });

    const pebbleBytes = quantizeToPebble8(composedRgba.data, OUT_W, OUT_H);

    // PKJS XHR is unreliable for binary arraybuffer responses, so we
    // expose a base64 text mode for the watch path.
    if (req.query.format === 'base64') {
      res.setHeader('Content-Type', 'text/plain');
      res.setHeader('X-Radar-Time', String(frame.time));
      res.setHeader('Cache-Control', 'public, max-age=300');
      res.status(200).send(pebbleBytes.toString('base64'));
      return;
    }

    res.setHeader('Content-Type', 'application/octet-stream');
    res.setHeader('Content-Length', pebbleBytes.length);
    res.setHeader('X-Radar-Time', String(frame.time));
    res.setHeader('Cache-Control', 'public, max-age=300');
    res.status(200).send(pebbleBytes);
  } catch (e) {
    console.error('radar error:', e);
    if (debug) {
      res.setHeader('Content-Type', 'application/json');
      res.status(502).send(JSON.stringify({ error: String(e && e.message), stack: String(e && e.stack), diag }, null, 2));
      return;
    }
    res.status(502).send('radar error: ' + (e && e.message));
  }
};
