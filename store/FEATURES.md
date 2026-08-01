# TouchyWeather — features, and how it differs from the watch face

**TouchyWeather v2.0.0** (watchapp) · UUID `78a2b21a-d72e-4c5e-b2b0-3aea3e72196b`
Companion to **TouchyWeather Face** (v1.3.0), which lives in the
[TouchyWeather - Watchface](../../TouchyWeather%20-%20Watchface) repo.

The two ship from separate codebases that started as copies of each other.
`theme.c`, `ui.c`, `icons.c` and `weather_data.c` began verbatim and have since
diverged, so they look like siblings and behave nothing alike.

---

## The one-line version

> **The face is where the weather comes to you. The app is where you go to read
> it properly.**

The face is a clock with four readings on it and a forecast a wrist-flick away.
The app is a twelve-card deck you open on purpose — with charts, live radar, and
a personality engine that has opinions about your weather.

---

## What makes the app special

### 1. Touch-first, button-complete

TouchyWeather began as a touchscreen app for the Pebble Time 2 and Round 2:
swipe between cards, pull down to refresh, swipe up for detail. It now runs on
the whole modern lineup, and **every touch gesture has a button equivalent**.
Touch is an enhancement, never a requirement — nothing is behind a gesture a
Pebble 2 owner cannot perform.

| Gesture | Button equivalent |
|---|---|
| Swipe left / right | UP / DOWN |
| Pull down to refresh | SELECT on the Main card |
| Swipe up for detail | Hold SELECT |
| Tap (Settings cursor) | — (cursor advances on tap only) |
| Flick down to dismiss | BACK |

### 2. Twelve cards, and you decide which exist

| Card | What it shows |
|---|---|
| **Main** | Current temp, feels-like, high/low, wind + direction, humidity (or dew point), optional location, and a rain / last-updated banner |
| **Touch & Go** | The personality engine — see below |
| **6 Hours** | Time, condition, temperature, wind and expected rainfall, hour by hour |
| **Week Ahead** | Five-day forecast with highs, lows and rain probability |
| **Precipitation** | Bar chart of rain probability, Now → +4h |
| **UV** | Gauge with the current index, its risk label, and the day's peak |
| **Air Quality** | AQI gauge with a descriptive label, plus pollen where available |
| **Sun Cycle** | Sunrise and sunset |
| **Night Sky** | Moon phase name and illumination |
| **Golden Hour** | Blue-hour and golden-hour windows, morning and evening |
| **Radar** | Live precipitation radar streamed onto the watch |
| **Settings** | Show, hide and reorder cards on the watch itself |

Show, hide and **drag-reorder** them from the watch (hold UP/DOWN on the
Settings card) or from your phone. Main is locked first and Settings locked
last; everything between is yours. Card management is deliberately fail-safe:
the Settings card can only be hidden while phone-side management is on, so
control is always reachable from at least one place.

### 3. Detail sheets with real vector charts

Hold **SELECT** — or swipe up — on **6 Hours, Week, Precipitation, UV** or
**Air Quality** and a bottom sheet slides up with a deeper chart: the
temperature trend, the hourly UV curve, hourly rainfall amounts, one detailed
day at a time, or the pollutant breakdown (PM2.5, PM10, ozone, NO₂). SELECT
toggles a secondary overlay — the rain-probability line on the temperature
trend, for instance. BACK, or a downward flick, dismisses it.

### 4. Live radar, rendered on-device

The **Radar** card streams a real precipitation frame from RainViewer onto the
watch — pre-quantised to Pebble's palette by a small Vercel proxy, reassembled
on-device, with a crosshair on your exact location. SELECT forces a refresh.

It needs ~51 KB of peak heap to assemble a frame (a 25.6 KB staging buffer and a
25.6 KB GBitmap coexist), which only the 128 KB App RAM models can spare — so
on every other platform it is **carved out of the carousel** rather than
failing at runtime. Nothing crashes; the card simply is not there.

### 5. Touch & Go — the personality engine

Live conditions are classified into one of **fifteen tiers** — storm, rain soon,
rain now, cold rain, snow, hot, cold, wind, high UV, bad air, muggy, pleasant
(day / night / cool), and stale data — and a line is drawn from that tier's
pool. The tier badge and the reading that drove it are shown above, so you know
*why* it said that.

> "Electrical storm active. Don't be a conductor."
> "Hydrate or wilt. It is cooking out there."
> "Window's closing. Move fast."
> "Slush is the new ice. Step lighter."
> "Wet phone risk: high. Pocket it safely."

### 6. Big Mode

A genuine accessibility mode for reduced eyesight: much larger fonts,
high-contrast colours, and simplified cards showing fewer, bigger items. The
Main card drops to temperature and condition; forecast cards show fewer rows.
The full detail is still one SELECT-hold away. The normal look is
pixel-identical until you opt in.

### 7. It keeps working while closed

Background auto-refresh fetches weather every 30 or 60 minutes while the app is
shut, so the deck is current when you open it. On exit — and after every
background fetch — the app publishes a **launcher glance**, so its launcher
entry shows the current temperature and condition without opening it. Weather is
cached on the watch, so the last reading is on screen before the first byte of a
new one arrives.

---

## Side by side

| | **TouchyWeather** (app) | **TouchyWeather Face** |
|---|---|---|
| What it is | Watchapp — you open it | Watch face — always on screen |
| Weather views | 12 cards | 4 peek pages + a dense overlay |
| Depth | Detail sheets with vector charts on 5 cards | The peek page itself |
| Input | Buttons **and** full touch (swipe, pull, tap, flick) | Accelerometer nudge only |
| Clock | None | Yes — the whole point |
| Complications | — | 4 user-assignable slots (2 lines, 2 pills) |
| Watch battery / step count | — | Both available as complications |
| Radar | **Live RainViewer radar** (colour, 128 KB models) | — |
| Personality | **Touch & Go — 15 tiers of one-liners** | — |
| Golden Hour, pollen, pollutant breakdown | Yes | — |
| Dedicated Precipitation / UV / Air Quality cards | Yes | Folded into Conditions |
| Accessibility | **Big Mode** — larger type, high contrast, simpler cards | Type grows as you switch rows off |
| Card management | On-watch show/hide/**reorder**, or from the phone | Four peek-page toggles |
| Refresh | Pull-to-refresh sheet + scheduled background wakeups | Rides the minute tick while on screen |
| Launcher glance | Current temp + condition in the launcher | — |
| Ambient behaviour | — | Rain auto-peek · night mode · Quick View reflow |

### Shared between them

Same Open-Meteo forecast and air-quality pipeline, same BigDataCloud reverse
geocoding, no API keys required. Same six platforms, same light/dark theme, same
icon set and accent colours. Both are configured from the phone with Clay, both
cache the last reading on the watch, both show a one-time "New on the horizon"
card generated straight from `CHANGELOG.md` at build time, and both send one
anonymous active-user ping per day (tagged `app` vs `face`), carrying no name,
no email, and coordinates rounded to ~11 km.

### What the face does that the app cannot

Tell you the time. It is a watch face: it is on your wrist all day, it holds
four readings you picked, and it needs no launching. If you want weather at a
glance rather than weather to read, that is the one to install.

---

## Platforms

All six modern Pebbles, no aplite. **Graceful degradation, not blocked
features:** non-touch models lose touch *gestures* (every one has a button
equivalent), and the 1-bit models additionally lose radar and colour accents.
No platform loses a weather *feature* it can physically render.

| Platform | Model | Display | Colour | Touch | Radar |
|---|---|---|---|---|---|
| `emery` | Pebble Time 2 | 200×228 rect | ✅ | ✅ | ✅ |
| `gabbro` | Pebble Round 2 | 260×260 round | ✅ | ✅ | ✅ |
| `basalt` | Pebble Time | 144×168 rect | ✅ | — | — |
| `chalk` | Pebble Time Round | 180×180 round | ✅ | — | — |
| `diorite` | Pebble 2 | 144×168 rect | 1-bit B&W | — | — |
| `flint` | Pebble 2 Duo | 144×168 rect | 1-bit B&W | — | — |

Layouts branch across four screen classes — small-rect, small-round, large-rect
and large-round — so each display gets geometry tuned for it. On the 1-bit
models colour accents become dither patterns, so charts and icons stay readable
without hue.

---

## Settings (phone, via Clay)

- **Appearance** — light / dark theme · time format (match watch / 12h / 24h) ·
  animations · Big Mode
- **Units** — imperial / metric · humidity or dew point
- **Navigation** — loop cards at the edges (off makes it a Quick Launch app you
  exit with the buttons) · SELECT switches theme
- **Background updates** — fetch while closed, every 30 or 60 minutes
- **Card visibility** — hand show/hide and ordering to the phone · auto-hide
  Rain & Radar when it's dry · hide the Settings card entirely
- **Location** — phone GPS, or pin a manual `lat,lon` · show the location name
  on the Main card

Card show/hide and **reorder** also live on the watch, on the Settings card.

---

## Where the data comes from

- **Forecast & air quality** — [Open-Meteo](https://open-meteo.com)
- **Radar** — [RainViewer](https://www.rainviewer.com), fetched and re-encoded
  for the watch by a small Vercel proxy in [proxy/](../proxy/)
- **Location name** — BigDataCloud reverse geocoding
- **Pollen** — Open-Meteo CAMS in Europe, a proxied Google Pollen lookup
  elsewhere
