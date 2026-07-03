# TouchyWeather — Development Roadmap: Master Report

> Analysis date: 2026-07-02. Author: engineering review of the current `release/1.11.1` codebase against the proposed 4-phase feature backlog.
> Rev 2 (same day): incorporated owner decisions — **full 7-platform support (new Phase 5)**, graceful degradation policy, detail modals on all five forecast cards, bottom-sheet visual style; added the gesture budget.

This document assesses the proposed roadmap against what the code actually does today, corrects a few assumptions in the backlog, re-estimates effort, and recommends a sequencing. Each phase has its own detailed doc (linked at the bottom) written so it can be researched further and implemented directly.

---

## 1. Executive summary

The backlog is directionally sound — every one of the six proposed features is worth building and none are blocked by a missing capability. But exploration of the codebase changes the plan in four material ways:

1. **Phase 3's effort estimates are inverted.** Conditional precipitation hiding (Task 3.2, labelled "medium") is nearly free: the card-hiding mechanism and the required forecast data already exist end-to-end. Big Mode (Task 3.1, also "medium") is the single most expensive item in the entire roadmap, because fonts and layout are hardcoded inside every card's draw function with no abstraction to hook into.
2. **Phase 4 depends on Phases 1–2.** The long-press SELECT gesture that Phase 4 wants to repurpose currently means "toggle theme." It must be freed by the Phase 1.2 / Phase 2 work before Phase 4 can use it.
3. **Phase 2's premise is milder than described,** and one of its stated benefits doesn't hold. The Settings card is pinned *last* in the carousel, not interleaved with weather cards, so it interrupts scrolling far less than the backlog implies. And a nav refactor by itself will not "enable older hardware" — platform support is real work, now scoped as **Phase 5**.
4. **There is hygiene work worth doing first** ("Phase 0"): a plaintext proxy secret is committed to this public repo, and the theme setting is redundantly persisted under two keys.
5. **(Rev 2) Non-touch support has one hard gap:** the *only* user-initiated weather refresh today is the touch pull gesture — on button-only models there would be no manual refresh at all. A button refresh path (Phase 5.1) should land early, in the Phase 1–2 window.

**Recommended sequencing:** Phase 0 (anytime) → Phase 1 (+5.1) → Phase 2 → Task 3.2 → Phase 4 → 3.1 Stage A → Phase 5 → 3.1 Big Mode.

---

## 2. Current-state architecture snapshot

Facts that every phase doc builds on. File:line references are into `src/c/` unless noted.

### Platforms & app type
- **Watchapp** (`package.json` → `"watchface": false`), capability `configurable`.
- **`targetPlatforms: ["emery", "gabbro"]`** today. The 7-platform list in `CLAUDE.md` is now the *decided target* (Phase 5). Both current targets have touchscreens (`PBL_TOUCH`); the other five are button-only, three are B&W. **(Rev 3, verified from SDK 4.9.169 + linker template):** gabbro is **260×260 round** (chalk's 180×180 round is a new, smaller class); App RAM holds code+data+heap *together*, so aplite's 24 KB is smaller than our current ~44 KB binary (aplite → lean no-go) and radar's 25.6 KB buffer likely doesn't fit the 64 KB platforms either. Details in the [Phase 5 matrix](PHASE_5_PLATFORM_EXPANSION.md).
- Touch code is cleanly gated (`#if ENABLE_TOUCH && defined(PBL_TOUCH)`, `TouchWeather.c:59-63`) — the app is *mostly* button-complete already. The gaps: manual refresh is touch-only, radar can't fit/render on B&W-low-mem, and zero `PBL_COLOR`/`PBL_BW` branches exist. Full analysis in [Phase 5](PHASE_5_PLATFORM_EXPANSION.md).

### Animation (`anim.c`)
- A single app-wide **10 Hz ticker** (`ANIM_PERIOD_MS 100`) that self-re-registers forever (`prv_tick` → `app_timer_register`). **No timeout, no auto-stop** except `anim_deinit()` at app exit.
- The ticker only triggers *redraws*; motion is computed by each card from a monotonic frame counter (`anim_get_frame()`).
- What actually moves: the **Main card hero icon** (`main_card.c:78` → `icons.c:434`), the shared **rain/updated banner** (`ui_draw_auto_banner`, ~4 s toggle) used by most cards, the **Settings footer hint** (~2.5 s), and the **refresh-sheet spinner** (every tick). `prv_tick` already does selective dirtying to save power (`anim.c:19-34`).
- **Implication for Task 1.1:** a naive "freeze all animation" breaks pull-to-refresh spinner feedback. The timeout must stop the hero-icon/banner redraws while leaving the refresh-sheet path alive.

### Buttons (`TouchWeather.c`, click provider at `:209`)
- SELECT short (`prv_select_click`, `:137`): **context-aware** — Radar → force refresh; Settings → toggle highlighted row; **everywhere else → toggle light/dark theme**.
- SELECT long, 600 ms (`prv_select_long`, `:162`): toggle theme everywhere except Settings.
- UP/DOWN short → `nav_prev` / `nav_next`. UP/DOWN long (500 ms) → reorder rows, Settings card only.
- **BACK is unhandled** → default single-press exit. **Decided (2026-07-03): long-press BACK** opens Settings (Phase 2.1); single-press exit timing untouched.
- Touch (swipe/tap) handled behind `ENABLE_TOUCH` (`:59-134`) for emery/gabbro.

### Gesture budget

Once all phases land, buttons are the whole UI on five of seven platforms. This table is the single source of truth for gesture allocation — SELECT-long was triple-booked before it existed:

| Gesture | Today | Target (after all phases) | Changed by |
|---|---|---|---|
| UP / DOWN short | Card nav | Card nav *(unchanged)* | — |
| UP / DOWN long | Reorder rows (Settings card only) | Same | — |
| SELECT short | Theme toggle (most cards); Radar refresh; Settings row-toggle | **Main: manual refresh** · Radar: refresh · Settings: row-toggle · in-modal: overlay toggle · elsewhere: theme toggle *iff* `SelectTogglesTheme` on | 1.2, 5.1, 4.2 |
| SELECT long | Theme toggle | **Open detail modal** (forecast cards) | 1.2 frees it, 4.1 claims it |
| BACK short | Exit app | Exit app; **dismiss modal/sheet first** if one is open | 4.2 |
| BACK long-press | — | **Open Settings card** (decided 2026-07-03; double-tap rejected — would slow single-BACK exit) | 2.1 |
| Touch (emery/gabbro only) | Swipe nav, pull-to-refresh, Settings tap | + long-press element → detail modal, drag-down dismiss sheet | 4.1/4.2 |

Rule: touch is always an *enhancement* — every action must have a button path (graceful-degradation policy, Phase 5).

### Navigation & cards (`nav.c`, `nav.h`)
- `Card` struct is `{ const char *name; CardDrawFn draw; }` — **no init/enter/exit lifecycle hooks**, just a draw callback.
- 12 cards registered in order (`TouchWeather.c:222-233`): Main(0), Touch&Go/advice(1), 6 Hours(2), Week(3), Precipitation(4), UV(5), Air Quality(6), Sun Cycle(7), Night Sky(8), Golden Hour(9), Radar(10), Settings(11).
- **Two independent concepts already exist:** `nav_set_enabled(idx, bool)` (visibility; disabled cards skipped and hidden from the page dots) and `nav_set_traversal(order, count)` (the user's saved visual order). Main is pinned first, Settings pinned last; both always enabled.
- **Conditional card hiding is already fully built** — Task 3.2 is mostly wiring, not new mechanism.

### Settings (`settings.c` / `settings.h`) and sync (`comm.c`)
- On-watch-only: per-card enable flags + visual order (persist keys `210..220`), plus non-persisted cursor.
- From phone via `@rebble/clay` (`config.js`), decoded in `comm.c:63` inbox: `Theme`, `TimeFormat`, `Units`, `UseDewPoint`, `ShowLocation`, `LoopNavigation`, `BackgroundUpdateInterval`, `LocationOverride`.
- **Gotcha:** Clay radiogroup/select values arrive as **C-strings**, so numeric settings are `atoi`'d (`comm.c:97-158`). New numeric Clay settings must follow this pattern.
- **Persist keys in use:** `1` (theme, in `theme.c`), `107` (weather cache), `200-202` (theme/loop/bg-interval), `210-220` (toggles + order). New keys must avoid these.
- **Cache gotcha:** the whole `WeatherData` blob is persisted under `PERSIST_KEY_CACHE 107`. **Any change to the `WeatherData` struct must bump this key** (`comm.c:34-47`) or old blobs misalign on upgrade.

### Theme (`theme.c` / `theme.h`)
- Two modes (`THEME_LIGHT=0`, `THEME_DARK=1`). Colors are **functions**, not a table: `theme_bg`, `theme_fg`, `theme_muted`, `theme_secondary`, plus theme-independent accents. Adding a high-contrast mode = extend the mode enum + branch each color function.

### Overlay / modal patterns (reusable for Phase 4)
- **`refresh_sheet.c`** — a layer overlay (not a Window) with a state machine, its own slide/timeout timers, and a global `refresh_sheet_is_active()` input lockout that every button handler checks. This is the template for Phase 4's floating modal.
- **`update_notes.c`** — a pushed modal Window with its own repaint loop. The alternative template.

### Fonts / layout (`ui.c` / `ui.h`)
- **No font or metrics abstraction.** Every card calls `fonts_get_system_font(FONT_KEY_…)` directly with hardcoded sizes; layout is compile-time macros (`UI_MARGIN_X`, `UI_HEADER_HEIGHT`, …) branched only by `PBL_IF_ROUND_ELSE`. This absence is the entire cost of Task 3.1.

### Forecast data available (`weather_data.h`)
- Near-term precip: `precip[5]` (now, +1h..+4h probability) and **`hours_pop[6]` (+1h..+6h probability)**, `hours_precip_x10[6]` (amount), and `rain_alert_min` (minutes to rain, -1 if none). **Task 3.2 needs no new phone-side plumbing.**
- Hourly: `hours_temp[6]`, `hours_cond[6]`, `hours_label[6][6]`, etc. — **6 points** available for Phase 4's temperature line chart.
- Weekly: `days_label/high/low/cond/pop` (5 days) — for Phase 4 day-detail modals.

---

## 3. Phase-by-phase assessment

| Phase / Task | Backlog effort | Re-estimate | Risk | Key dependency | Verdict |
|---|---|---|---|---|---|
| **1.1** Animation timeout + disable toggle | Low | **Low** | Must not kill refresh-sheet spinner | none | Build first — highest battery ROI |
| **1.2** Disable SELECT theme toggle | Low | **Low** | Decide what SELECT does when disabled | none | Build first; also unblocks Phase 4 |
| **2.1** Long-press BACK for settings *(decided)* | High | **Medium** | None to exit timing (long-press, not double-tap) | 1.2 pattern | Do; settings also editable in Clay (2.2) |
| **2.2** Migrate settings to Clay | High | **Medium–High** | Clay has no drag-reorder | 2.1 | Hybrid: toggles in Clay, reorder on watch |
| **3.1** Big Mode | Medium | **High** | Touches all 12 cards; needs font abstraction | font/metrics layer (Stage A) | Most expensive item — scope as own project |
| **3.2** Conditional precip UI | Medium | **Low** | False-negative (hide before storm) | reconcile w/ manual toggles | Cheap win — data + mechanism exist |
| **4.1/4.2** Long-press + bottom-sheet modals (5 forecast cards) | High | **High** | Overlay collision; UV/AQI need new data → cache bump | **1.2 + 2** free SELECT-long | Vector-only sheet reusing refresh-sheet pattern; build 6 Hours/Precip first |
| **5** Full 7-platform support *(new)* | — | **High** | aplite measured lean no-go (44 KB binary > 24 KB App RAM); B&W untested; chalk needs its own layout class | **5.1 early**; 5.2 needs 3.1 Stage A | Graceful degradation: radar compiled out on B&W/low-mem |

---

## 4. Phase 0 — hygiene (recommended before or alongside Phase 1)

Not in the original backlog; surfaced during review.

1. **Plaintext proxy secret in a public repo.** `src/pkjs/index.js` (~lines 537–546) hardcodes `key=tw-radar-prod-Xk7nQ2v9LpR4Mj8a` for the radar/pollen/track proxy endpoints — with a code comment reminding to remove it before publishing to a public repo. Under the repo's public + CC BY-NC status, this secret is exposed. Rotate it and move it out of the committed client (e.g. build-time injection or an unauthenticated-but-rate-limited proxy).
2. **Redundant theme persistence.** Theme is written to persist key `1` (`theme.c`) *and* key `200` (`settings.c`), and `settings_save_theme` appears to be uncalled. Consolidate to one source of truth to avoid drift.

These are independent of the feature work and can land anytime.

---

## 5. Recommended sequencing & dependencies

```
Phase 0 (hygiene) ─── independent, anytime
        │
Phase 1 ─ 1.1 Animation timeout ── highest battery ROI
        └ 1.2 SELECT toggle setting ──┐ (frees SELECT short+long)
        └ 5.1 Button refresh (pulled forward: SELECT on Main)
                                       │
Phase 2 ─ 2.1 Settings shortcut ───────┤
        └ 2.2 Clay migration ──────────┤ (establishes clean nav)
                                       │
Task 3.2 Conditional precip UI ────────┤ (cheap; data+mechanism exist)
                                       │
Phase 4 ─ 4.1 Long-press listener ─────┘ (requires freed SELECT-long + clean nav)
        └ 4.2 Bottom-sheet modals ──── 5 forecast cards; reuse refresh_sheet.c pattern
        │
Task 3.1 Stage A (ui metrics/font abstraction) ── shared prerequisite ─┐
        │                                                              │
Phase 5 Platform expansion (7 targets) ◄───────────────────────────────┤
        │                                                              │
Task 3.1 Stage B (Big Mode proper) ◄───────────────────────────────────┘
```

Rationale: Phase 1 delivers the biggest user-visible win (battery) cheaply and frees the gestures Phases 4–5 need; 5.1 (button refresh) rides along because it's tiny and required by every non-touch platform later. Phase 2 gives Phase 4 the clean navigation it assumes. Task 3.2 is pulled early because it's nearly free. **3.1 Stage A (the ui metrics abstraction) is the roadmap's key shared refactor** — it serves both Big Mode and the 144×168/round layout work, so it lands once, before Phase 5, and Big Mode's visual layer (Stage B) finishes last on a stable, multi-platform base.

---

## 6. Phase documents

- [Phase 1 — Quick Wins & Stabilization](PHASE_1_QUICK_WINS.md)
- [Phase 2 — Navigation Refactor & Clay Migration](PHASE_2_NAV_REFACTOR.md)
- [Phase 3 — Accessibility & Smart UI](PHASE_3_ACCESSIBILITY.md)
- [Phase 4 — Advanced Interaction & Data Density](PHASE_4_DEEP_DIVE_UI.md)
- [Phase 5 — Full Platform Expansion](PHASE_5_PLATFORM_EXPANSION.md)

Each phase doc follows the same skeleton: Goal · Current state (with file:line) · Proposed design · Files to touch · New keys (message + persist, with cache-bump warnings) · Dependencies & risks · Open questions for research · Verification (build + emulator screenshots per `CLAUDE.md`).
