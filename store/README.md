# Store upload kit — TouchyWeather

Everything needed for the Pebble appstore listing of the **watchapp**. The
watch face has its own kit in
[TouchyWeather - Watchface/store/](../../TouchyWeather%20-%20Watchface/store/).

| File | What it is |
|---|---|
| [FEATURES.md](FEATURES.md) | Full feature list, and how the app differs from the TouchyWeather Face |
| [LISTING.md](LISTING.md) | Ready-to-paste store copy — blurb, description, keywords |
| [screenshots/](screenshots/) | Per-platform PNGs at native resolution |

## Screenshots

One folder per platform, each PNG at the watch's exact native resolution — no
padding, no scaling, no emulator chrome.

| Platform | Model | Size | Radar |
|---|---|---|---|
| `basalt` | Pebble Time | 144×168 | — |
| `chalk` | Pebble Time Round | 180×180 | — |
| `diorite` | Pebble 2 | 144×168 | — |
| `emery` | Pebble Time 2 | 200×228 | ✅ |
| `flint` | Pebble 2 Duo | 144×168 | — |
| `gabbro` | Pebble Round 2 | 260×260 | ✅ |

### The shots

| File | Feature it sells |
|---|---|
| `00-whats-new.png` | The show-once "New on the horizon" screen after an update |
| `01-card-main.png` | Current conditions — the card you land on |
| `02-card-advice.png` | Touch & Go, the personality engine |
| `03-card-hours.png` | Six hours: time, condition, temp, wind, rainfall |
| `04-card-week.png` | Five-day forecast with rain probability |
| `05-card-precip.png` | Rain-probability bar chart, Now → +4h |
| `06-card-uv.png` | UV gauge with risk label and the day's peak |
| `07-card-aq.png` | Air-quality gauge with pollen |
| `08-card-sun.png` | Sunrise and sunset |
| `09-card-night.png` | Moon phase and illumination |
| `10-card-golden.png` | Blue hour and golden hour, both ends of the day |
| `11-card-radar.png` | Live radar, on-device *(emery / gabbro only)* |
| `12-card-settings.png` | On-watch card show/hide and reorder |
| `13-detail-hours.png` | Detail sheet: the temperature trend chart |
| `14-detail-uv.png` | Detail sheet: the hourly UV curve |
| `15-big-mode-main.png` | Big Mode — the accessibility layout |
| `16-big-mode-hours.png` | Big Mode on a forecast card: fewer, bigger rows |
| `17-dark-main.png` | Dark theme |

`11-card-radar.png` exists only for `emery` and `gabbro`. Radar needs ~51 KB of
peak heap to assemble a frame, so it is carved out of the carousel on every
other platform — the card genuinely is not there, and its absence from those
folders is correct.

### Known rendering problems these captures exposed

Both are in the app, not the capture — worth a look before you upload the
affected shots.

- **Touch & Go text is clipped by the status banner on every small screen.**
  On `basalt`, `chalk`, `diorite` and `flint`, `02-card-advice.png` shows the
  advice line running underneath the `UPDATED NOW` pill and being cut off
  mid-sentence ("Calm skies overhead. The…", "Skies are calm. A"). `emery` and
  `gabbro` render the same card cleanly. The advice body appears not to reserve
  the banner's height on the short layouts. As shipped, four of the six watches
  cut the personality engine's punchline in half — and it is the feature the
  listing leads on.
- **The Settings card's footer hint is drawn on top of the card list on the
  small screens.** `12-card-settings.png` on `basalt`, `chalk`, `diorite` and
  `flint` reads as garbled overlapping text (`SUN OYCLBOYE UP`). The hint sits
  at a fixed `H - 32` (`H - 58` on round) while the eleven card rows run past
  it, so they collide. This is not a capture artifact: the hint rotates through
  three strings every ~2.5 s off `anim_get_frame()` and freezes when the
  animation window closes, and it collides in every phase. `emery` and
  `gabbro` have the vertical room and render it cleanly.
- **Chart axis labels clip against the round bezel on `chalk`.**
  `13-detail-hours.png` shows the x-axis as `°M2 AM1 AM2 A` where the labels
  run past the chord.

The captures are honest — they are what the app draws. Fix the layouts and
re-shoot, or pick different screenshots for those platforms.

**On the radar shot:** it is a real RainViewer frame over San Francisco, which
matches the location every other card shows. San Francisco was dry when these
were taken, so the map renders with no precipitation over it. It is an honest
capture of the feature; if you want a frame with rain on it, re-shoot with
`CAPTURE_LAT` / `CAPTURE_LON` in `src/pkjs/index.js` pointed somewhere wet —
but then change `LocationName` to match, or the deck contradicts itself.

## How these were captured

These come from the emulator over the ordinary Pebble screenshot endpoint, via
a driver that holds **one libpebble2 connection per platform** and does
everything over it — install, app messages, button presses, screenshots.
Spawning `pebble <cmd>` per action is both slow and flaky: every invocation
reconnects, and the reconnects intermittently time out.

The forecast is served by a `CAPTURE_MODE` branch in `src/pkjs/index.js` that
replaces the live Open-Meteo fetch with one fixed payload, so all six platforms
show the same numbers on every card. **It ships `false`.** Only the data handed
to the watch changes — what is photographed is the real rendering. Radar is
deliberately *not* stubbed: it keeps its real RainViewer pipeline, and the
capture coordinates are seeded so it has somewhere to centre.

Things that cost real time, in case these need re-shooting:

- **BACK exits a watchapp.** This is the big difference from the face driver.
  Pressing BACK on the carousel drops you to the launcher, and there is no
  relaunch command in the tool — so the driver never presses BACK except to
  dismiss something known to be on top, tracks its carousel position, and
  reaches every card by walking forward with DOWN. Recovery is a reinstall.
- **A stale `qemu_spi_flash.bin` bricks the emulator silently.** QEMU keeps
  running and installs and app messages still report success, but its control
  channel is dead and *every* screenshot times out, forever. Deleting the flash
  image per platform is the fix — and it is also what makes the What's New
  screen (`00`) reproducible, since it is show-once.
- **pebble-tool's emulator registry goes stale.** `pb-emulator.json` caches the
  QEMU/pypkjs PIDs and only checks "is this PID running", so a recycled PID
  makes it connect to a port nothing is listening on and fail with "Connection
  refused" on every retry. Drop the platform's entry before booting.
- **Set the clock format *before* installing.** Installing relaunches the app,
  which reads the system clock style then; flipping it afterwards leaves the
  first shot in 24 h while the rest are 12 h.
- **Big app-message dicts lose their tail** — no error, no dropped-inbox log.
  Send in chunks of ~3 keys and repeat the whole config; every setter is
  idempotent.
- **Detail sheets slide in.** Grabbing the frame 2 s after the SELECT hold
  caught the sheet mid-animation with the card still visible above it. Allow
  ~4.5 s.
- **The radar card streams its frame in chunks** and needs roughly ten seconds
  on screen before it has a picture to photograph.
- **`pebble emu-steps` wedges the emulator outright** — the QEMU health-metric
  packet behind it kills the control channel the same way a stale flash image
  does. Reproducible with the stock CLI, so it is an SDK bug. Nothing here
  needs it, but do not add it.
- **pypkjs dies at random**, most often mid-install on the big screens. The
  watch keeps its persisted settings, so the driver tears the pair down and
  reconnects rather than losing the platform.
