# TouchyWeather Vercel proxy

Serverless functions backing the watch app. Deployed to
`https://touchyweather-radar-proxy.vercel.app`.

## Endpoints

| Path | Method | Auth key | Purpose |
|------|--------|----------|---------|
| `/api/radar` | GET | `RADAR_SECRET` | Composited basemap + RainViewer radar, returned as Pebble pixels |
| `/api/pollen` | GET | `RADAR_SECRET` | Google Pollen forecast, cached per ~11 km cell |
| `/api/track` | POST | `RADAR_SECRET` | Anonymous active-user ping (writes aggregate counts) |
| `/api/stats` | GET | **`STATS_SECRET`** | Private analytics dashboard (HTML or `?format=json`) |

## Environment variables (set in the Vercel dashboard, never committed)

| Var | Used by | Notes |
|-----|---------|-------|
| `RADAR_SECRET` | radar, pollen, track | Shared key the **watch app sends**. It ships inside the watch JS bundle, so treat it as semi-public — it gates writes/reads of upstream APIs, not private data. |
| `GOOGLE_POLLEN_KEY` | pollen | Google Pollen API key. |
| `KV_REST_API_URL` / `KV_URL` (+ token vars) | pollen, track, stats | Vercel KV (Redis). Optional for pollen; **required** for analytics. Without it, `track` no-ops and `stats` returns 500. |
| `ANALYTICS_SALT` | track | Random secret mixed into the SHA-256 of each user id before storage, so stored hashes can't be precomputed/correlated. Set once and don't change (changing it makes returning users look new). |
| `STATS_SECRET` | stats | **Admin-only** key for the dashboard. MUST be different from `RADAR_SECRET` and MUST NOT be shipped to any device — it's the only thing keeping your numbers private. If unset, `/api/stats` refuses all access (fail-closed). |

## Analytics design (privacy)

`/api/track` receives `{ id, lat, lon }` where `id` is Pebble's anonymous
account token (no name/email/PII) or a random local fallback. It:

1. Hashes `SHA-256(id + ANALYTICS_SALT)` — the raw id is never stored.
2. `SADD`s the hash into per-period Redis sets: `users:day:<YYYY-MM-DD>`,
   `users:week:<ISO-week>`, `users:month:<YYYY-MM>`, `users:year:<YYYY>`.
   `SCARD` of each = unique active users (DAU/WAU/MAU/YAU); set intersections
   give retention.
3. Counts a coarse ~11 km location cell in `geo:<YYYY-MM>` for a heatmap —
   aggregate only, never linked to a user.

The watch pings at most once per UTC day (throttled client-side in
`src/pkjs/index.js`), so a ping ≈ a daily-active user.

## Viewing the numbers

Open in a browser (only you have `STATS_SECRET`):

```
https://touchyweather-radar-proxy.vercel.app/api/stats?key=<STATS_SECRET>
```

Append `&format=json` for machine-readable output.

## Local dev

```bash
cd proxy
vercel dev
# then, with KV + secrets configured in a local .env:
curl -s -X POST 'http://localhost:3000/api/track?key=<RADAR_SECRET>' \
  -H 'content-type: application/json' \
  -d '{"id":"test-user-1","lat":37.77,"lon":-122.41}' -i
```
