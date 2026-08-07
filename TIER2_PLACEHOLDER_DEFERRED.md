# Deferred: Tier 2 — no fabricated readings on first-ever launch

**Status:** deferred by decision, 2026-08-07. Do this AFTER the Big Mode round (B)
and the watchface legibility round (C). Ask before starting.

## What the user sees today

`prv_init()` calls `weather_data_init_mock()` ([TouchWeather.c:391](src/c/TouchWeather.c#L391))
before anything else. That fills `WeatherData` with invented weather:

    temp 72°, feels 75°, San Francisco, sunny, wind 12 NW,
    "RAIN IN 15M", week 78/58 · 72/60 · 68/54 · 75/56 · 80/59

`comm_load_cache()` then overwrites all of it with the real last-known data
([TouchWeather.c:400](src/c/TouchWeather.c#L400)) — **before** the first
`window_stack_push()` at line 408. That ordering is Tier 1 of
`UNITS_FLASH_FIX_PLAN.md` and it is already shipped.

So there are three cases:

| Case | What is on screen |
|---|---|
| Returning user, cache present | Real cached data from the first frame. Mock never visible. |
| Background wakeup | Guarded — `comm_load_cache()` runs right after the mock ([TouchWeather.c:438-448](src/c/TouchWeather.c#L438-L448)) so `prv_save_cache()` can't persist the mock over real fields. |
| **First-ever launch, no cache** | **The full fictional forecast, until the first fetch lands.** Seconds normally; longer on poor connectivity or with the phone out of range. Nothing marks it as fake. |

Only the third row is the problem. Verified 2026-08-07 that the cache genuinely
persists (a relaunch logged `Data is 187 secs old (<900 threshold), skipping
fetch`), so rows 1 and 2 are sound.

## Why it was deferred

It is not the bug from the r/pebble 2.0 thread. pwnage777's "Week Ahead shows
imperial" retracted as user error, and the Big Mode repro is explained by Big
Mode dropping the wind row — the only "MPH"/"KMH" string in the app
([main_card.c:305](src/c/cards/main_card.c#L305), skipped by the Big Mode
early return at [line 102](src/c/cards/main_card.c#L102)). That is fixed in
round B.

Tier 2 is a real honesty problem with a different trigger, and it adds a field
to the persisted struct — which forces another `PERSIST_KEY_CACHE` bump. Keeping
that out of the Big Mode change keeps both diffs reviewable.

## The change when we do it

Per `UNITS_FLASH_FIX_PLAN.md` Tier 2 — 5 files, ~50 lines:

- `weather_data.h` — add a `DataState` enum (`NONE` / `CACHED` / `LIVE`) and a
  `state` field.
- `weather_data.c` — `weather_data_init_mock()` marks `DATA_STATE_NONE`.
- `comm.c` — mark cache loads `CACHED`, live payloads `LIVE`. **Bump
  `PERSIST_KEY_CACHE` 109 → 110** (the struct grows; an old blob misaligns).
- cards — render `--°` and blank fields while `state == DATA_STATE_NONE`.
- Decide per card what a placeholder looks like: the Main hero, the hi/lo row,
  the week rows, the status pill, and the location line all need an answer.

Open question to settle first: does `rain_alert_min` / the auto-hide evaluation
behave sanely at `DATA_STATE_NONE`, or does a placeholder state need its own
branch in `prv_evaluate_auto_hide()`?

## Verification when we do it

Fresh-install path is the whole point, so `pebble wipe` before every capture —
which also re-arms the What's New modal (see CLAUDE.md). Capture the first frame
after install on at least basalt and emery, and assert distinctness by md5 in
Python, not shell.
