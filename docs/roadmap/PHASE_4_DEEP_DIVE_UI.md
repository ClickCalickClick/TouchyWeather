# Phase 4 — Advanced Interaction & Data Density

**Goal:** Deep-dive weather metrics accessible from the forecast cards without cluttering them — long-press opens a **bottom-sheet detail modal** in the app's existing visual language.

**Decisions locked (2026-07-02):**
- **Scope:** all five forecast cards get a detail modal — 6 Hours, Week, Precipitation, UV, Air Quality. (Astronomy cards — Sun Cycle, Night Sky, Golden Hour — are possible later additions, out of v1 scope.)
- **Style:** bottom sheet sliding up from the bottom, mirroring the pull-to-refresh sheet's motion so it feels native to the app.
- **Platforms:** must work on all 7 targets (see [Phase 5](PHASE_5_PLATFORM_EXPANSION.md)) — buttons are the primary interaction; touch is an enhancement layer on emery/gabbro.

> **Hard dependency:** SELECT-long currently means "toggle theme" (`prv_select_long`, `TouchWeather.c:162`). **Task 1.2 and Phase 2 must free it first.** See the gesture budget in the [master report](00_MASTER_REPORT.md#gesture-budget).

---

## Task 4.1 — Contextual long-press listener

### Current state
- SELECT-long bound to theme toggle (`TouchWeather.c:162`); UP/DOWN-long used only on Settings card; BACK unhandled.
- Touch handlers exist behind `#if ENABLE_TOUCH && defined(PBL_TOUCH)` (`TouchWeather.c:63-134`) — emery/gabbro only.
- Cards are pure draw functions with no hit-regions.

### Proposed design — two input paths, one entry point
A single `detail_modal_open(card_idx, context)` entry, reached by:

1. **SELECT-long (all platforms, primary):** opens the detail modal for the current card with the default context (e.g. the full 6-hour chart, today's day detail). Cards without a modal (Main, astronomy, Radar, Settings) → no-op or gentle vibe pulse.
2. **Touch long-press (emery/gabbro, enhancement):** long-press on the card body opens the same modal. On 6 Hours and Week, a long-press on a *specific* hour block / day row passes that element as the context (chart scrolled to that hour / that day pre-selected). Requires each participating card to expose a tiny hit-test table (`GRect region → context id`) — add a single optional callback to the `Card` struct (`nav.h:8`), e.g. `int (*hit_test)(GPoint p)`, defaulting to NULL.

Input gating follows the established discipline: every handler early-returns on `refresh_sheet_is_active()` today; add `detail_modal_is_active()` to the same chain, and the modal must also refuse to open while the refresh sheet is active (and vice versa — one overlay at a time).

---

## Task 4.2 — Bottom-sheet detail modals

### Visual language (keep the app's aesthetic)

The sheet reuses the app's existing vocabulary — **no new colors, no new fonts**:

```
┌──────────────────┐
│ 6 HOURS       ☀  │  ← underlying card, dimmed/untouched, still visible above sheet
│ 2PM 3PM 4PM …    │
├══════════════════┤  ← sheet top edge: rounded corners + small grab notch (touch affordance)
│ ── TEMP TREND ── │  ← header: ui_draw_card_header_with_icon() style, GOTHIC_14, theme_muted rules
│       ╭──╮       │
│   ╭───╯  ╰─╮     │  ← chart line: theme_accent_orange (temp) / theme_accent_blue (precip)
│ ──╯        ╰─    │     gridlines: theme_muted, dotted via ui_draw_dotted_hline (ui.c:114)
│ 74°          65° │  ← key values: LECO numbers style, theme_fg
│ ┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈┈ │  ← dotted divider between sections (existing pattern)
│ POP  10 20 40 5 %│  ← secondary rows: GOTHIC_14/18, theme_secondary
│        ● ○       │  ← page dots when paged (reuse nav indicator style, nav.c:130)
└──────────────────┘
```

**Rules:**
- Surface = `theme_bg()`, text = `theme_fg()`/`theme_secondary()`, structure = `theme_muted()`, data series = the existing accents (`theme_accent_orange` for temperature, `theme_accent_blue` for water/precip, `theme_accent_advice` violet reserved for advice-flavored content). On B&W platforms accents fall back per the Phase 5 color policy.
- Sheet height ≈ 75–80% of screen; the top of the underlying card stays visible (dimmed on color platforms, plain on B&W) to communicate "overlay, not navigation."
- Slide-up/down animation copies the refresh sheet's mechanics (`refresh_sheet.c` — layer frame resize + 30 ms dedicated timer, ~250 ms duration). **All chart rendering is vector** (`graphics_draw_line`, filled circles) — no bitmaps, so it's cheap on RAM and works on aplite.
- Round screens (chalk/gabbro): inset content per `PBL_IF_ROUND_ELSE`, same as existing cards.

### Interaction map (what's "clickable")

| Input | In-modal behavior |
|---|---|
| UP / DOWN short | Scroll content; on **paged** modals (Week) switch page (prev/next day) |
| SELECT short | Toggle secondary overlay where defined (e.g. POP overlay on the temp chart); otherwise no-op |
| SELECT long | No-op (prevents accidental re-trigger) |
| BACK | Dismiss sheet (slide down). Modal consumes BACK; underlying nav untouched |
| Touch: drag down on notch / flick down | Dismiss (emery/gabbro) |
| Touch: swipe left/right | Paged modals: prev/next page |

While the sheet is open, all card navigation is locked out (same pattern as `refresh_sheet_is_active()`).

### Per-card modal content (v1)

| Card | Modal title | Content | Data | New plumbing? |
|---|---|---|---|---|
| **6 Hours** | TEMP TREND | Line chart of `hours_temp[0..5]` with hour labels (`hours_label`), min/max annotated in LECO style; SELECT toggles a POP overlay (`hours_pop`); below the fold: per-hour wind (`hours_wind`, `hours_wind_dir`) | All on watch | **None** |
| **Week** | *(day name)* | Paged, one page per day (5): big high/low, condition icon, POP; extended rows as data allows | `days_*` now; richer fields (daily wind max, precip sum, UV max) available from Open-Meteo `daily=` | Optional: new `Day*Wind/PrecipSum` keys → **WeatherData change → cache-key bump** |
| **Precipitation** | RAINFALL | Bar/line chart of hourly *amounts* `hours_precip_x10[0..5]` (the card shows only probability today); headline "rain in Nh" from `rain_alert_min`; POP row beneath | All on watch | **None** |
| **UV** | UV TODAY | Current `uv` vs `uv_max` gauge + hourly UV curve for the day | Curve needs Open-Meteo `hourly=uv_index` | **Yes:** new `HourN_Uv` keys + `WeatherData` fields → **cache-key bump** |
| **Air Quality** | AIR DETAIL | AQI headline + pollutant breakdown (PM2.5, PM10, O₃, NO₂) and pollen detail | Breakdown needs extra air-quality API fields | **Yes:** new keys + fields → **cache-key bump** |

**Chart honesty note:** the 6-hour series has exactly **6 points** — render as a segmented trend line with visible point markers, not a smoothed curve.

**Recommended build order within Phase 4:** 6 Hours → Precipitation (zero plumbing, prove the sheet) → Week (paged mechanics) → UV + AQI together (single coordinated `WeatherData` change and **one** cache-key bump, not two).

### Architecture

- New module `src/c/detail_modal.c/.h` — one reusable sheet, modeled line-for-line on `refresh_sheet.c`'s structure (state machine IDLE/OPENING/OPEN/CLOSING, layer-frame clipping, `detail_modal_is_active()`).
- Content renderers are small per-card draw functions (`prv_draw_hours_detail(GContext*, GRect, int scroll)`, …) registered alongside the card, or dispatched by card index — keep them in the corresponding `cards/*.c` files so card + detail live together.
- **Single-instance rule (memory):** one sheet layer, reused; never stack sheets or push Windows; no heap buffers beyond a small scroll state. Vector-only drawing keeps the aplite footprint near zero.

### Files to touch
- New: `src/c/detail_modal.c/.h`.
- `TouchWeather.c` — SELECT-long rewire (gated on 1.2 setting), input-gate additions, touch long-press dispatch, layer registration next to `refresh_sheet_init` (`:240`).
- `nav.h` / participating `cards/*.c` — optional `hit_test` callback + per-card detail renderers.
- For UV/AQI data: `src/pkjs/index.js` (fetch + pack), `package.json` (keys), `comm.c` (decode, **bump `PERSIST_KEY_CACHE`**), `weather_data.h` (fields).

### Dependencies & risks
- **Depends on 1.2 + Phase 2** (frees SELECT-long, settles gesture budget).
- **Risk — overlay collision:** refresh sheet vs detail sheet must be mutually exclusive; also verify behavior when weather data arrives *while* a modal is open (redraw the modal's data or defer).
- **Risk — cache bump churn:** batch the UV+AQI struct changes into one release to avoid invalidating users' cache twice.

### Open questions for research
- Week modal: paged-by-day (recommended above) vs one long scroll — validate on 144×168 where a page holds less.
- Does `hours_precip_x10` amount data render meaningfully when values are tiny (drizzle)? May need a "trace" label floor.
- Exact sheet height and notch size per screen size (144×168 / 180×180 round / 200×228).

### Verification
- Build + install on emery, gabbro, **and basalt** (small rect proves the layout floor).
- Per card: SELECT-long opens the right modal; UP/DOWN scrolls/pages; BACK dismisses; nav locked while open.
- Open modal during an in-flight refresh and vice versa → no overlay collision.
- Screenshot every modal on emery + gabbro + basalt per `CLAUDE.md`.
