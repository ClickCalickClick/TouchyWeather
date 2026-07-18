# Phase 5 — Full Platform Expansion (7 targets)

**Goal:** Expand `targetPlatforms` from `["emery", "gabbro"]` to all seven models in `CLAUDE.md` — aplite, basalt, chalk, diorite, emery, flint, gabbro — with a **graceful degradation** policy: each platform ships with unsupported features auto-removed rather than blocking the whole platform.

**Decisions locked (2026-07-02):** full 7-platform scope; graceful degradation (per-platform feature carve-outs are acceptable; feature parity is *not* required to ship a platform).

---

## 1. Platform matrix — **verified (2026-07-03) against SDK 4.9.169's own platform table** (`sdk-core/pebble/common/tools/pebble_sdk_platform.py`)

| Platform | Model | Display | Color | Touch | App RAM (code+data+heap) | Binary cap | Notes |
|---|---|---|---|---|---|---|---|
| aplite | Pebble Classic | 144×168 rect | 1-bit B&W | No | **24 KB** | 64 KB | The hard constraint driver |
| basalt | Pebble Time | 144×168 rect | 64-color | No | 64 KB | 64 KB | Baseline color/non-touch |
| chalk | Pebble Time Round | **180×180 round** | 64-color | No | 64 KB | 64 KB | The *small* round — see below |
| diorite | Pebble 2 | 144×168 rect | 1-bit B&W | No | 64 KB | 64 KB | B&W with decent RAM |
| emery | Pebble Time 2 | 200×228 rect | 64-color | **Yes** | 128 KB | 128 KB | Current target |
| flint | Pebble 2 Duo | 144×168 rect | 1-bit B&W | No | 64 KB | 64 KB | Currently-sold hardware |
| gabbro | Pebble Round 2 | **260×260 round** | 64-color | **Yes** | 128 KB | 128 KB | Current target |

> **Key memory fact (verified from the SDK linker template):** a Pebble app's code, data, *and* heap all share the single App RAM region — the binary is loaded into it. Our current binary is **~44 KB** (`build/emery/pebble-app.bin`). Consequences: on 64 KB platforms only ~15–20 KB is left for heap+stack *before any per-platform code trimming*; on aplite the current binary **cannot even load** (44 KB > 24 KB total).

> **Correction (Rev 3):** gabbro is **260×260**, not 180×180 — the current `PBL_IF_ROUND_ELSE` values are tuned for gabbro's 260×260, so **chalk (180×180 round) is a genuinely new, smaller screen class**, not "the round layout we already have."

Three capability axes drive all the work: **touch vs buttons**, **color vs B&W**, **memory class** (24 KB / 64 KB / 128 KB). Screen classes for the 5.2 layout pass (and Phase 3.1 Stage A) are **four**: small-rect 144×168 (aplite, basalt, diorite, flint — 4 of 7 platforms), small-round 180×180 (chalk), large-rect 200×228 (emery), large-round 260×260 (gabbro).

---

## 1b. User-facing summary (per-platform, end state after all phases)

- **emery / gabbro** — everything: touch + buttons, radar, modals, all cards, Big Mode.
- **basalt / chalk** — everything except touch gestures (all actions have button equivalents) and *likely* radar (64 KB App RAM vs 25.6 KB buffer — measure at Phase 5 start). Chalk gets its own small-round layout class.
- **diorite / flint** — as basalt, plus: no radar (1-bit screen), accent colors replaced by line-pattern differentiation, themes = B/W inversion.
- **aplite** — not shipped (24 KB App RAM < ~44 KB binary); revisit only if the Phase 5 diet lands surprisingly small.

**Framing rule: no platform loses a weather *feature* — non-touch loses touch *gestures* (button equivalents exist for all), B&W additionally loses radar and color accents.**

## 2. Feature degradation matrix

| Feature | Touch (emery/gabbro) | Color non-touch (basalt/chalk) | B&W non-touch (aplite/diorite/flint) |
|---|---|---|---|
| Card navigation (UP/DOWN) | ✅ (+ swipe) | ✅ | ✅ |
| Manual weather refresh | pull-to-refresh + **new button path** | **new button path (5.1)** | **new button path (5.1)** |
| Radar card | ✅ | ⚠️ **likely compiled out** — napkin math: ~44 KB binary + statics + 25.6 KB buffer > 64 KB App RAM; only a per-platform build (which trims touch code) can prove otherwise | ❌ **compiled out** (B&W useless + can't fit the 25.6 KB buffer) |
| Pull-to-refresh sheet visuals | ✅ (gesture) | ✅ shown programmatically on button refresh | ✅ same |
| Animations (Phase 1.1) | ✅ | ✅ | ✅ (verify aplite CPU handles 10 Hz; timeout helps) |
| Theme light/dark | ✅ | ✅ | ✅ (bg/fg invert works in B&W) |
| Accent colors | ✅ | ✅ | **fallback pass required (5.3)** |
| Detail modals (Phase 4) | ✅ (+ touch long-press) | ✅ via SELECT-long | ✅ via SELECT-long (vector charts fit aplite) |
| Settings card / Clay | ✅ | ✅ | ✅ |
| Touch hit-regions (4.1 enhancement) | ✅ | — (compiled out, already gated) | — |

**Radar carve-out mechanics:** gate registration — skip `nav_register("Radar", …)` and the radar `ToggleId` handling under `#if defined(PBL_PLATFORM_APLITE) || defined(PBL_BW)` (exact macro set per SDK). The settings card must not list a card that doesn't exist on the platform; `s_toggle_to_card_idx` (`TouchWeather.c:27`) already maps ToggleId→card index, so an absent card maps to -1 and is skipped. Persist keys stay stable so settings survive if a user's data roams across platforms.

---

## 3. Work packages

### 5.1 Button-triggered manual refresh — **prerequisite, can land as early as Phase 1–2**
Today the *only* user-initiated refresh is the touch pull gesture (`refresh_sheet_on_touchdown` → `comm_request_refresh`, `refresh_sheet.c:453`); everything else is automatic (launch-if-stale `comm.c:395`, background wakeup). Non-touch users would have **no manual refresh at all**.

- **Proposed gesture:** SELECT-short on the **Main card** = refresh. This matches the existing idiom (SELECT on Radar = refresh radar) and becomes available once Task 1.2 frees SELECT from theme-toggling. See the gesture budget in the [master report](00_MASTER_REPORT.md#gesture-budget).
- Add `refresh_sheet_show_programmatic()` — enter the OPENING/LOADING state machine directly, skipping touch tracking, so button users get the same spinner/phrase feedback. The state machine (`refresh_sheet.h:9`) already separates TRACKING (touch-only) from OPENING/LOADING — the entry point is small.

### 5.2 Layout pass for 144×168 (small-rect) and 180×180 (chalk round) — **shares the Phase 3.1 "Stage A" metrics abstraction**
All layout constants are compile-time macros in `ui.h` branched only by `PBL_IF_ROUND_ELSE`, tuned for emery 200×228 and gabbro 260×260. On 144×168 the emery-tuned sizes will cramp or clip, and **chalk's 180×180 round gets gabbro's 260×260 values — badly oversized**.

- **Do Phase 3.1 Stage A first** (route fonts + metrics through `ui_*()` accessors): the same accessor layer then branches on *screen class* (small-rect 144×168 / small-round 180×180 / large-rect 200×228 / large-round 260×260) as well as scale (Normal/Big). One refactor serves both Big Mode and platform expansion — this is the single biggest synergy in the roadmap.
- Per-card sweep afterward is mostly value-tuning, verified by screenshot matrix.

### 5.3 B&W color fallback pass
No `PBL_COLOR`/`PBL_BW` branches exist anywhere today. On 1-bit displays the accents (ChromeYellow, VividCerulean, VividViolet) and grays dither unpredictably.

- Centralize in `theme.c` (colors are already functions — ideal): each accessor gets a `PBL_IF_COLOR_ELSE(color, bw_fallback)`. Accents → `theme_fg()`; `theme_muted()` → `GColorLightGray` dither or fg with thin strokes. Chart series that relied on color get differentiated by **pattern** (solid vs dotted line, filled vs hollow markers) — build this into the Phase 4 chart helpers from day one.
- Audit any direct `GColor…` literals in `cards/*.c` that bypass theme functions and route them through `theme.c`.

### 5.4 aplite memory budget — **measured evidence now says lean no-go**
- **(Rev 3) Hard fact:** aplite's 24 KB App RAM must hold code+data+heap, and the current binary alone is ~44 KB. Shipping aplite means cutting the loaded footprint by **more than half** — not a trim, a rewrite-scale diet.
- If attempted anyway, order of carve-outs: radar (–25.6 KB buffer + code), touch code (already `#if`-gated), update_notes window, phrase tables, night-sky rendering. Decide only after a real aplite build's size report.
- **Recommendation:** plan for six platforms; revisit aplite only if a trial build after all other Phase 5 work lands surprisingly small.

### 5.5 Build & test matrix
- `package.json` targetPlatforms → all 7; fix any SDK warnings per platform.
- Emulator screenshot matrix: every card × every platform × light/dark (script it — `pebble install --emulator X && pebble screenshot …` loop). This matrix becomes the regression harness for Phases 3–4 too.

---

## 4. Files to touch
- `package.json` — targetPlatforms.
- `theme.c` — B&W fallbacks (5.3).
- `ui.h`/`ui.c` + `cards/*.c` — metrics accessor layer + small-screen values (5.2, shared with Phase 3.1).
- `TouchWeather.c` — Main-card SELECT refresh (5.1); radar registration gating (§2).
- `refresh_sheet.c/.h` — programmatic show entry (5.1).
- `cards/settings.c` / `settings.c` — skip absent cards on B&W platforms.

## 5. New keys
None. No `WeatherData` changes → **no cache-key bump**. Persist keys unchanged across platforms.

## 6. Dependencies & risks
- **5.1 should land early** (Phase 1–2 window) — it also benefits touch users.
- **5.2 depends on Phase 3.1 Stage A** — sequence Stage A before this phase (see master report sequencing).
- **Risk:** aplite may be unshippable within reasonable effort even after carve-outs — treat aplite as *stretch*: ship the other six, keep aplite behind a measured go/no-go after 5.4's audit.
- **Risk:** flint/gabbro SDK specifics (exact defines, heap) are new — verify against current repebble SDK before relying on this doc's assumptions.

## 7. Open questions for research
- ~~Exact SDK platform defines for flint/gabbro and their heap sizes.~~ **Resolved (2026-07-03):** verified from installed SDK 4.9.169 — see §1 matrix. Defines: `PBL_PLATFORM_FLINT` (144×168, `PBL_BW`, `PBL_RECT`, no touch, 64 KB) and `PBL_PLATFORM_GABBRO` (260×260, `PBL_COLOR`, `PBL_ROUND`, `PBL_TOUCH`, 128 KB).
- Radar on basalt/chalk: napkin math says no (§2), but **measure with a real per-platform build** at Phase 5 start — trimming touch code and radar itself changes the numbers.
- Does the Rebble appstore serve per-platform binaries from one package (it should — confirm resource limits per platform)?
- aplite 10 Hz animation cost (CPU/battery) — moot unless the aplite go/no-go (§5.4) flips to go.
- Chalk layout: the 180×180 round class is new (current round values are gabbro-tuned 260×260) — needs its own pass in 5.2.

## 8. Verification
- `pebble build` succeeds for all 7; per-platform size report reviewed.
- Screenshot matrix (every card × platform × theme) reviewed for clipping/dither problems.
- On a B&W emulator (diorite): no radar card in rotation or settings list; charts legible; SELECT-on-Main refresh shows the sheet.
- On aplite emulator: app launches, all remaining cards render, no OOM in logs.
