---
alwaysApply: true
---

## Project Overview

This is a Pebble smartwatch application written in C using the Pebble SDK.

## Supported Platforms

The app targets multiple Pebble watch models:
- aplite (Pebble classic)
- basalt (Pebble Time)
- chalk (Pebble Time Round)
- diorite (Pebble 2)
- emery (Pebble Time 2)
- flint (Pebble 2 Duo)
- gabbro (Pebble Round 2)

## Commands

```bash
# Build the app for all platforms
pebble build

# Clean build artifacts
pebble clean

# Install the app on specific emulator
pebble install --emulator basalt

# Screenshot the running emulator
pebble screenshot --emulator basalt --vnc --no-open screenshot.png
```

If you need more information on the `pebble` command or a sub-command, append `--help`.

### Headless Environments

If you're running in an environment without a window server (e.g., headless Linux, Docker, CI), you must add `--vnc` to **all commands that interact with the emulator**. This includes app installs, screenshots, button presses, and any `emu-*` commands:

```bash
pebble install --emulator basalt --vnc
pebble screenshot --emulator basalt --vnc --no-open screenshot.png
pebble emu-button --emulator basalt --vnc click select
```

The `--vnc` flag enables a VNC-based display backend that doesn't require X11.

## Project Structure

```
src/c/           - C source files for the watchface
src/pkjs/        - PebbleKitJS files (optional)
resources/       - Images, fonts, and other resources
build/           - Generated build output
```
## Configuration

By default, this project is initialized as a watchface. To make it an app, replace "watchface": true with "watchface": false in package.json.

## Architecture

The application follows the standard Pebble app architecture:

1. **Main Entry Point**: `src/c` - The `main()` function initializes the app and starts the event loop
2. **Window Management**: Single window app with text layer for displaying button press feedback
3. **Event Handling**: Button click handlers registered via `prv_click_config_provider` for UP, DOWN, and SELECT buttons

## SDK Documentation

The full Pebble SDK documentation is available at https://developer.repebble.com.

Main Categories:
- Tutorials - Step-by-step learning (C watchface tutorial in 5 parts, advanced topics)
- Developer Guides - Comprehensive reference organized by topic

Key Sections:
- App Resources - Images, fonts, vector graphics, 256 resource limit
- User Interfaces - Layer hierarchy, TextLayer, MenuLayer, round vs rectangular displays
- Events & Services - Buttons, accelerometer, compass, health data, background workers
- Communication - Bluetooth AppMessage, PebbleKit JS/Android/iOS integration
- Graphics & Animations - Drawing APIs, property animations, vector graphics
- Debugging - App logs, GDB, common errors and solutions
- Best Practices - Multi-platform support, battery conservation, modular architecture
- Design & Interaction - Glance-first design, one-click actions, platform guidelines
- App Store Publishing - Submission requirements, assets, analytics

Key Entry Points:
- https://developer.repebble.com/tutorials/watchface-tutorial/part1 - C development start
- https://developer.repebble.com/guides/events-and-services/buttons - Button handling
- https://developer.repebble.com/guides/user-interfaces/layers - UI foundations

## Development Best Practices

- Whenever making changes, run `pebble screenshot --emulator basalt --vnc --no-open screenshot.png` and view the screenshot to make sure it's what the user requested. If not, make more changes until it does what it's supposed to.

## Emulator Button Control

Control emulator buttons programmatically with `pebble emu-button`:

```bash
# Click a button (press and release)
pebble emu-button click select

# Long press (e.g., 2 seconds to exit app)
pebble emu-button click back --duration 2000

# Repeat clicks (e.g., scroll down 5 times)
pebble emu-button click down --repeat 5

# Faster repeat interval
pebble emu-button click up --repeat 3 --interval 100
```

**Actions:**
- `click` - Press then release (use `--duration` for long press)
- `push` - Hold button down (use `release` to let go)
- `release` - Release all buttons

**Buttons:** `back`, `up`, `select`, `down`

**Best Practices:**
- Use `click` for normal navigation and selection
- Use `click --duration 2000` for long press (e.g., back button to exit)
- Use `--repeat` to scroll through menus instead of multiple commands
- After making UI changes, take a screenshot to verify the result

## AI Interaction Guidelines

- When given an image of a watchface to replicate, describe the target watchface in precise detail. Note every visual element present, as well as size, alignment, font weight, spacing, and location.

## AI Code Review Guidelines

- Once you think you've fulfilled the user's request, ask yourself if you see any issues with the current screenshot, and if there are any differences between the screenshot and the reference image or the user's description. If so, fix them.
## Emulator Traps

Each of these cost a real debugging run. They are about this project's toolchain, not about any
one feature, so they stay here permanently.

* **`pebble install` needs 12–14 SECONDS to relaunch and render.** At `sleep 2`–`sleep 5` the
  screenshot catches the *previous* state. Use `sleep 18`. This has produced whole capture sets of
  a stale card.
* **Assert distinctness on EVERY capture set** — `md5` the shots and check the unique count equals
  the shot count, including sets that "obviously" differ. It is the only check that has ever caught
  a stale-capture run. Write the assertion in Python, not shell: a shell `N=$(ls $SET | wc -l)`
  where `$SET` fails to word-split yields `N=0 unique=0`, which "passes" vacuously.
* **A guarded edit can compile to nothing.** `#if defined(UI_SCREEN_SMALL_RECT)` in a file that
  does not include `ui.h` is silently false — green build, no warning, no visible change. The only
  cheap proof a guarded edit compiled in is that `tools/lock_guard.py`'s hash for a platform it
  targets actually **moved**. Use the guard in both directions: locked pair unchanged, target
  platform changed.
* **The first `down` press after an install is usually swallowed** while the app settles. Verify
  which card you are on by looking at the screenshot, never by counting presses.
* **Theme toggles from card 1+, never card 0** — SELECT on the Main card is a manual refresh
  ([TouchWeather.c:240-243](src/c/TouchWeather.c#L240-L243)), not the theme toggle. Assert a flip by
  MEAN LUMINANCE, not by distinctness: a failed toggle still yields distinct shots.
* **There is a 15-minute data freshness gate.** Reinstalls log `skipping fetch` and re-render the
  persisted cache. Force a real fetch with SELECT on the Main card.
* **`pebble emu-set-time` moves the WATCH clock but not PKJS** — pypkjs `new Date()` follows the
  host. A forced watch clock against a cached payload fabricates defects that do not exist; this is
  what made defect #63 look real for five phases.
* **The emulator's weather is for a field in central Germany — not your location.** pypkjs 2.0.7
  implements `navigator.geolocation` by fetching the host's public IP from ipify and resolving it
  through a **bundled `GeoLiteCity.dat`** (MaxMind GeoLite City, discontinued 2018). A modern IP
  returns a country-level-only record, so it yields the country centroid — for DE that is
  **51.0, 9.0**. Confirmed 2026-08-01: the API at those coords matched a live capture exactly
  (71°F, feels 66, hi/lo 74/59, wind 7mph N). Consequences: every live-data screenshot in this repo
  shows German weather; a card can look empty (0% POP all day there) while your real location is
  rainy; and it is *not* the `37.7749,-122.4194` San Francisco fallback at `index.js`, which only
  fires when the lookup raises. **To capture real local data, set the Clay "Location override"
  (`lat,lon`, stored as `localStorage.locationOverride`) — the app reads it before geolocation.**
  Do not confuse this with `CAPTURE_MODE`, which serves a fully synthetic payload.
* **PKJS edits need `pebble build` to ship.** Editing `src/pkjs/*.js` alone does nothing.
* `pebble emu-button --repeat N` silently drops presses — use single `click` calls in a shell loop.
* `emu-button click back` EXITS the watchapp unless a modal is open.
* `pebble wipe` recovers a wedged emulator but resets persisted state, which RE-ARMS the What's New
  modal — the next launch is the notes screen, not card 0.
* **The same applies the first time a build runs on a platform you have not driven before.** An
  emulator for a fresh platform arms the notes screen, it SWALLOWS every early button press, and
  the `back` that follows exits the watchapp to the Timeline ("No events") — a whole 14-shot emery
  set came back as notes and Timeline. `update_notes_maybe_show()` persists the seen-version the
  instant it displays, so the fix is to **install twice**: the first launch disarms it, the second
  starts on card 0. Cheap, and it makes a capture run reproducible across platforms.
* **`sizeof(WeatherData)` is 480 bytes and `PERSIST_DATA_MAX_LENGTH` is 256 — the cache persists
  anyway.** `prv_save_cache()` writes the whole struct and does not check the return. That looks
  like a guaranteed silent-failure bug and is not one: verified 2026-08-07 by relaunching and
  reading `comm.c:683> Data is 187 secs old (<900 threshold), skipping fetch`, which can only come
  from a blob written by the previous run. Do not "fix" it, and do not re-derive it — the SDK
  header's 256 is documented for strings and is not what the firmware enforces here.
* **Code size is the RAM budget, and it has now bitten TWICE.** Pebble loads
  the whole app image into RAM, so every byte of code comes out of the app
  heap. Adding ~430 bytes (eleven copies of a one-line guard) made
  `app_message_open` return **4096 = `APP_MSG_OUT_OF_MEMORY`** the first time;
  PR #24's +368 bytes did it again (heap 3248B → 2880B, open needed ~2400B
  free, had 2324B). The failure does not look like memory: with no inbox open
  the phone's payload is NAKed *and* the watch's own refresh request goes
  nowhere, so the app silently never receives weather again — background and
  foreground both. Fix 2026-09-02: small screens open **(1536, 256)** via
  `prv_msg_open()` in comm.c, which checks the return, falls back to
  (1280, 160), and logs total failure; emery/gabbro keep (2048, 256). The
  measured weather payload is **1240B in 111 tuples** — the floor for any
  inbox shrink. `RADAR_CHUNK_SIZE` is a PROTOCOL constant duplicated in pkjs
  AND radar.c (the watch reassembles at `idx * RADAR_CHUNK_SIZE`): shrinking
  only the pkjs side stalled radar at 93% — late chunks land past the buffer
  and are dropped uncounted, so the transfer never completes. It stays 1500;
  only emery/gabbro (2048 inbox) ever receive chunks, so the small-screen
  inbox puts no pressure on it. Check `ls -l build/basalt/pebble-app.bin`
  against main after any
  addition; the `Heap Usage for App` line in `pebble logs` is the other tell
  (healthy is 2880B total with the 1536 open succeeding). Prefer one guard at
  a shared choke point over N copies.
* **Never bump `PERSIST_KEY_CACHE` (109). It is pinned.** Each of the six
  historical bumps orphaned the user's cache — the old key is never deleted, so
  `persist_exists(new)` is false, no cache loads, and whatever
  `weather_data_init_empty()` left reaches the first draw. That shipped an
  invented imperial forecast to metric users on every update (the "Week card
  switches to Fahrenheit" report) and leaks ~480 bytes of the 4 KB persist quota
  per bump. `prv_load_cache()` validates `persist_get_size()` against
  `sizeof(WeatherData)` plus a layout-version key; bump `CACHE_LAYOUT_VERSION`
  for a same-size layout change, never the key.
* **Nothing may render a WeatherData field while `weather_data_has_reading()` is
  false** — the struct is zeroed then, and zero is a *claim*: AQI 0 is "GOOD",
  UV 0 is "LOW", `moon_phase` 0 is NEW, condition 0 is SUNNY. `nav.c`'s
  `prv_draw_card()` enforces this for every card (Settings and Radar exempt);
  `detail_modal.c` does the same for all five sheets.
* **To photograph the pre-first-reading state, set `CAPTURE_NO_DATA = true`** in
  `src/pkjs/index.js` — the phone then withholds the payload and the state holds
  indefinitely instead of ending ~1s after launch. `tools/capture_nodata.py`
  walks it. Distinct from `CAPTURE_MODE`, which serves a fixed synthetic
  forecast for store shots.
* **PKJS is NOT listening at t=0 of a wakeup launch, and the send still returns
  `APP_MSG_OK`.** A wakeup launch is what STARTS the phone's JS app, so anything
  `comm_background_init()` sends arrives before an `appmessage` listener exists
  and is dropped on the phone — with no NAK, so `prv_outbox_failed`'s one retry
  never fires. Measured on basalt 2026-09-03: `app_message_outbox_send()`
  returned 0 while PKJS logged NEITHER of the two lines its handler can print,
  so `comm_request_refresh()` was a no-op on every background launch. The fetch
  then fell through to PKJS's own `ready` hook, which is freshness-gated at 15
  min (`FETCH_FRESH_MS`) — so a wakeup finding data newer than that did nothing
  at all and died at the 28 s timeout, which `prv_bg_timeout()` scored as a
  FAILURE and climbed the backoff with. Fixed by `prv_bg_request()`: three sends
  at 1 s / 6 s / 11 s, stopping the moment `s_is_background_mode` clears. The
  foreground 750 ms defer exists for the same reason and is equally unproven on
  hardware — foreground works because `ready` covers it, not because 750 ms is
  enough.
* **A closed app cannot see the phone reconnect, so out-of-range must NOT climb
  the backoff.** `connection_service_subscribe` only delivers while the app
  runs, so there is no reconnect hook to reset the ladder from — do not go
  looking for one. Persisting the fail count fixed a 5-minute retry churn but
  pinned any watch that had been out of range at the 4-hour cap, and that is
  what "reopen hours later, last updated 5 hours ago" was. The no-phone branch
  now HOLDS at the normal interval (`prv_schedule_wakeup_in`) and leaves the
  count alone; the backoff still owns real failures (timeout/drop), where the
  phone is reachable and retrying hard would cost something.
* **Testing background updates requires an in-code interval, and 600 s is the
  only good one.** Clay offers just 0/1800/3600 and `settings_load()` clamps to
  0 or 300..86400, so edit `s_bg_update_interval`'s default. It must exceed
  `FETCH_FRESH_MS` (900 s)… except that nothing under 900 s exercises the real
  path at all: at 300 s the PKJS `ready` gate always holds, which is a test
  artifact, not the bug. It must ALSO differ from backoff rung 1 (300 s), or the
  `BG: sched Ns` log cannot tell a hold from a failure. 600 s fails the first
  test deliberately and passes the second — use it to prove the sentinel path,
  and read `appmessage: LastUpdated sentinel` in the PKJS log as the pass.
* **A reinstall can leave the pending wakeup stale, so never reinstall while
  waiting for one.** `comm_init()` logs `BG: wid stale` when `wakeup_query()`
  no longer knows the persisted id, re-arms from that moment, and the wakeup you
  were waiting on never fires — a 12-minute run comes back with an empty log and
  looks like a broken fix. Arm the wakeup with the LAST install of the run, read
  `BG: sched <n>s wid=` to learn when it will fire, then only press buttons.
* **`pebble emu-bt-connection --connected no` WEDGES the emulator permanently —
  do not use it.** It severs the pypkjs link that `pebble logs` and every
  `emu-*` command ride on, so you lose your own observation channel the instant
  you create the condition you wanted to observe; `--connected yes` afterwards
  raises `libpebble2.exceptions.TimeoutError` like everything else, and only
  `pkill -9 -f qemu-pebble` + `pebble wipe` recovers. Cost: two dead 12-minute
  runs. To exercise `comm_background_init()`'s no-phone branch, temporarily
  invert the peek (`if (1) {  // TEMP TEST`) and run with the phone CONNECTED —
  the branch is what you are testing, not the radio, and logging survives.
* **Start the emulator with `pebble install`, and attach `pebble logs` SECOND.**
  `pebble logs` boots its OWN emulator when none is running; two `qemu-pebble`
  processes then share one `qemu_spi_flash.bin` on different ports and both
  wedge, after which every `emu-*` call raises
  `libpebble2.exceptions.TimeoutError`. `pebble kill` does not always clear
  them — `pkill -9 -f qemu-pebble` does. Cost: two dead 12-minute wakeup runs.
* Detail sheets open with a long press: `click select --duration 800`.
* No `timeout` command on macOS. No ImageMagick; PIL is available, and contact sheets are the
  fastest way to review a set.
* A `cd` inside a compound shell command persists to later commands. Use absolute paths.

Carousel order: Main(0) Touch&Go(1) 6 Hours(2) Week(3) Precipitation(4) UV(5) Air Quality(6)
Sun Cycle(7) Night Sky(8) Golden Hour(9) Settings(10). Radar is registered but runtime-disabled.

## Rendering Rules (earned, not theoretical)

* **Fill vs stroke decides WHETHER something dithers on 1-bit; muted vs secondary only decides
  which way a STROKE quantizes.** A fill that must not dither has to be a pure endpoint
  (`theme_fg()`). Getting this backwards shipped a "fixed" page-dot row still at 37% dither.
* **Dithering is a tool as often as a bug.** The Night Sky moon needs it as a THIRD tone — three
  regions must be distinct and 1-bit has two colours.
* **When a defect is in a duplicated helper, grep for the twin.** Three occurrences (#99, #43, #80).
  It is a rule, not an anecdote.
* **Reserve INK height, not the font's layout box** — but note a font's layout WIDTH exceeds its
  ink (side bearings), which is what ellipsized "Now" in a box its ink fit.
* **Measure the current build before changing code to match a defect report.** Ten register entries
  did not survive measurement (#16 #27 #29 #35 #47 #50 #63 #68 #77, plus #94's stated cause). A
  report written from old screenshots describes the app that was. **A deferral is not a diagnosis.**

## The emery/gabbro Lock

`emery` and `gabbro` are frozen. Any change must leave them byte-identical.

* Safe axes only: `UI_SCREEN_SMALL_RECT`, `UI_SCREEN_SMALL_ROUND`, `PBL_BW`.
* Never edit an `#else`/`LARGE_*` branch — add a branch **above** it and keep the large expression
  verbatim, character for character.
* Every build: `pebble clean && pebble build` (clean is mandatory), then `python3
  tools/lock_guard.py`, which must exit 0.
* `tools/lock_baseline.txt` was deliberately re-baselined 2026-07-31 for defect #93. It is correct —
  do not "restore" it.
