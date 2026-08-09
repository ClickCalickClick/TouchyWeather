# Tier 2 Implementation: no fabricated weather, ever

Ships the goal of `UNITS_FLASH_FIX_PLAN.md` Tier 2 — the app never shows an
invented reading — but by a different route than the plan proposed. The
deviations are the interesting part; each one is recorded with the measurement
that forced it.

## What was actually wrong

`weather_data_init_mock()` filled `WeatherData` with a hardcoded IMPERIAL
forecast (72°, San Francisco, `RAIN IN 15M`, and a week of 78/58 · 72/60 ·
68/54 · 75/56 · 80/59) and set `valid = true`. Whenever no cache loaded, that
fiction reached the first draw.

`TIER2_PLACEHOLDER_DEFERRED.md` said the only exposed case was a first-ever
install. That was wrong, and the error mattered:

**`#define PERSIST_KEY_CACHE` was bumped on every `WeatherData` layout change** —
the comment above it logs six bumps, 104 → 109. A bump orphans the user's blob
(the old key is never deleted, and `persist_exists(new)` is false), so no cache
loads and the mock survives to the first draw **on every app update**, not only
at install. That is what makes the user report say *occasionally*.

It also explains why the report names the Week card specifically. Week prints
`days_high`/`days_low` with no unit label anywhere, so a metric user reading
78/58 has no way to see it as anything but Fahrenheit — and 78/58 is impossible
as Celsius. The Main card carries `MPH`/`KMH`, and round B gave Big Mode an
`F`/`C` letter; Week has neither.

## What shipped

**Placeholder state = `valid == false`. No new struct field, no key bump.**
The plan called for a `DataState` enum, which grows the struct and forces
`PERSIST_KEY_CACHE` 109 → 110 — i.e. the fix would have shipped one more
instance of its own bug. `valid` already carries the distinction, and three
call sites (`glance.c`, `refresh_sheet.c`, the status pill) already read
`!valid || last_updated == 0` as "no reading". Consequences that fall out free:

- `prv_save_cache()` already guards on `d->valid`, so an un-fetched struct can
  no longer be written over real fields on a background wakeup — the constraint
  the deferral doc raised is now structural rather than ordering-dependent.
- `prv_initial_refresh()` already fetches immediately when `!valid`.
- Touch & Go's existing `ADV_DATA_STALE` path stops being dead code.

**109 is pinned, with a layout guard instead of bumping** (`comm.c`). Accept a
blob only when `persist_get_size()` matches `sizeof(WeatherData)` and a
companion layout-version key matches; otherwise delete it and stay in the empty
state. An absent layout key grandfathers in as v1, so this commit does not
orphan the caches of users on 109 today. Adopted from the sibling watchface
repo, which pinned its key at 30 for a different reason (there, bumping aliases
a settings key; here it orphans data and leaks the 4 KB persist quota).

**One structural guard, in `nav.c`, not one per card.** `prv_draw_card()` swaps
in the panel for any card while `weather_data_has_reading()` is false. Settings
and Radar are exempt — neither renders WeatherData. The detail sheets keep their
own guard in `detail_modal.c` (different layer, one site for all five). This was
forced by RAM; see below. It is also the stronger design: a card added later
inherits the guard instead of needing someone to remember it.

**The mock survives behind `#ifdef TW_MOCK_DATA`** (defined nowhere) so layout
work can still use it. Guarded-out code emits nothing, so it cannot move the
locked binaries.

## The regression this work caused, and the budget nobody had written down

The first implementation put a guard in each of eleven cards and a nine-key
purge loop in `comm_load_cache()`. It built clean, the placeholder rendered
correctly on every card — and **the app could no longer receive weather at all
on basalt.**

`app_message_open(2048, 256)` returned **4096 = `APP_MSG_OUT_OF_MEMORY`**. With
no inbox open, the phone's payload is NAKed *and* the watch's own refresh
request goes nowhere, so both directions are dead and the app sits at "NO DATA
YET" forever. Pebble loads the whole app image into RAM, so **code size comes
straight out of the app heap**, and this app ships with roughly **400 bytes of
headroom** on the 144px platforms. The eleven guards cost ~430 bytes.

Two wrong diagnoses on the way, both worth recording:

1. *"The purge loop delays `app_message_open()`."* Moving it to the 750ms timer
   still failed 3/3 — that timer fires while the first payload is being
   delivered.
2. *"It is emulator flakiness."* It was not: 3/3 fail vs 3/3 clean on HEAD,
   re-tested after an hour.

The real signal was in the logs the whole time — `Heap Usage for App` reported
2680B on the broken build against 3248B on HEAD. Fix: consolidate to one guard
(`nav.c`), drop the `persist_exists` probe and the log string from the purge,
shorten the cache-rejected log. Final basalt binary is **62000 bytes vs HEAD's
62124 — 124 bytes smaller than what it replaces**, and `app_message_open()`
returns 0.

## Verification

All on the emulator, screenshots not reasoning.

- **No-data state** — new `CAPTURE_NO_DATA` harness in `src/pkjs/index.js` makes
  the phone withhold the payload, so the pre-first-reading state can be held
  indefinitely instead of caught in a ~1s window. `tools/capture_nodata.py`
  walks all 11 cards + 5 sheets. Every weather card and every sheet shows
  NO DATA YET / WAITING FOR PHONE on **basalt and emery**; Settings still
  renders its real content; the pill reads `UPDATED --`. Card shots distinct
  (asserted in Python) — the walk advanced.
- **Live data still arrives** — basalt, live Open-Meteo path, wiped emulator:
  `weather sent`, `amr=0`, and the Main card renders real weather (78°, FEELS
  76°, UPDATED NOW). This is the check the earlier build failed.
- **Locked pair unchanged with a warm cache** — HEAD and final walked
  back-to-back sharing one cached payload (the 15-minute freshness gate keeps
  the payload identical across both runs). **emery 12/15 and gabbro 13/15
  byte-identical.** All three differences explained and confirmed by eye:
  the rotating Touch & Go quip (seeded by `last_updated`), the `UPDATED NOW` vs
  `UPDATED 1M AGO` pill, and a What's New modal in the pre-install shot. An
  earlier run 40 minutes apart showed extra diffs that were purely hour labels —
  capture baselines close in time.
- **Auto-hide fails open at `valid == false`** — the open question from the
  deferral doc. Forced `s_auto_hide_precip` on, walked with no reading: the
  carousel is **byte-identical** to the auto-hide-off run, so Precipitation is
  not hidden. Measured, not reasoned from the branch.
- `pebble clean && pebble build` then `tools/lock_guard.py` exits 0.

**emery/gabbro lock DELIBERATELY re-baselined.** Removing fabricated weather has
to reach the locked pair too — they showed the same invented forecast. Approved
before the work started; the byte lock cannot prove this one, so the warm-cache
equivalence above is the substitute.

## Follow-ups not taken

- The `DataState` enum (plan Tier 3). Now safe to add behind the layout-version
  key, but it is a separate commit and it costs RAM this app does not have.
- Per-card headers in the empty state. The first implementation had them; they
  went with the consolidation. ~300 bytes for a state that lasts seconds.
