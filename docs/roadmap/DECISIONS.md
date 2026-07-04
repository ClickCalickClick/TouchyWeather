# Overnight Autopilot — Decisions Log

Running log of judgment calls made while executing the roadmap autonomously
(2026-07-03, overnight). Each entry: **what** was chosen, **why**, and **how to
reverse**. Newest phase last. This is Jared's morning review sheet.

---

## ☀️ MORNING SUMMARY (read this first)

Branch `feature/roadmap-phases`, **8 local commits** on top of the roadmap docs,
**never pushed**. Tree clean, `pebble build` green (emery + gabbro) at HEAD.
Every commit was screenshot-verified on the emery emulator.

**Done (in execution order):**
| Phase | What | Notes |
|---|---|---|
| 0 | Theme persistence consolidated onto key 1 | Secret cleanup **waived** per your instruction |
| 1.1 | Animation idle-timeout + `AnimationsEnabled` | ~8 s freeze, ticker stops when idle |
| 1.2 + 5.1 | `SelectTogglesTheme` + Main-card button refresh | SELECT-on-Main = refresh |
| 2.1 | **FALLBACK** — Settings stays terminal card | ⚠️ long-press BACK is undeliverable on this SDK; see 2.1 |
| 2.2 | Opt-in Clay card visibility (`PhoneManagesCards`) | default off, avoids wiping on-watch config |
| 3.2 | Opt-in auto-hide Precip+Radar when dry | hysteresis + fail-open guardrails |
| 4 | **PARTIAL** — detail modals for 6 Hours + Precipitation | Week/UV/AQI deferred |

**Biggest things to review (⚠️):**
1. **2.1 overrode your decision.** Long-press BACK for Settings can't be
   implemented on this SDK/emulator (long-click and raw-click on BACK never
   fire — verified 3 ways; only short-click works). I fell back to keeping
   Settings as the terminal card (the phase doc's sanctioned runner-up) after a
   `fable` consult. **Please verify on real hardware** whether raw BACK events
   arrive — if so, the raw-timer approach can be reinstated. Full chain in 2.1.
2. **Phase 4 is partial** (2 of 5 forecast modals). Week/UV/AQI deferred; UV+AQI
   need one batched `WeatherData` change + cache-key bump (107→108). See Phase 4.
3. Several features are **opt-in / default-off** (Clay card control, auto-hide) —
   deliberate, so nothing changes for existing users until they opt in.

**Not started (large, left for you / a future run):** Phase 3.1 Stage A (the
ui-metrics/font accessor refactor across all 12 cards), Phase 5 (7-platform
expansion — `targetPlatforms` still `["emery","gabbro"]`), Phase 3.1 Stage B
(Big Mode). These are the roadmap's heavy items; the master-report dependency
graph has Stage A before Phase 5 before Big Mode.

**User action items (can't be autopiloted):**
- **Radar-proxy key:** you said keep it embedded / don't rotate — done nothing, as
  instructed.
- **Emulator env:** I installed `libpng` via Homebrew to boot the emulator (E1)
  and left the emery emulator's persisted app-state dirty from testing (some
  cards user-disabled). Delete `~/Library/Application Support/Pebble SDK/4.9.169/
  emery/qemu_spi_flash.bin` for a clean slate, or just re-enable cards on-watch.
- **Before release:** add a "What's New" changelog line if you want to announce
  any of this; run the app on real hardware to confirm the SELECT/BACK behaviors
  (emulator input has quirks — see E2 + 2.1).
- Review this file, then decide what to push / PR.

---

## Environment / setup

### E1. Emulator libpng dependency (I installed `libpng` via Homebrew) — FYI
- **What:** After I killed a wedged emulator, a fresh `qemu-pebble` cold-boot
  failed with `libpng16.16.dylib` not found (`/opt/homebrew/opt/libpng/...`).
  libpng was not installed on the machine. I ran `brew install libpng` (1.6.58)
  to restore emulator boot, which fixed it.
- **Why:** The emulator that was running when this session started (booted by you
  earlier) was working; I killed it during a restart attempt and then couldn't
  cold-boot a new one without libpng. The screenshot verification gate (CLAUDE.md)
  needs a bootable emulator. `brew install libpng` is additive and reversible.
- **Reverse:** `brew uninstall libpng` if you don't want it. Harmless to keep;
  the Pebble emulator legitimately needs it.
- **Lesson applied:** stopped force-killing the emulator between tasks.

### E2. Emulator SELECT-toggles-theme is visually inert in the emery emulator (pre-existing, NOT a regression)
- **What:** Pressing SELECT on ordinary cards (which should flip light/dark theme)
  produced no visible theme change in the emery emulator. I verified this is
  **identical on the untouched baseline** (stashed my changes, rebuilt, retested)
  — so it is a pre-existing emulator input characteristic, not caused by the
  Phase 0 persistence refactor.
- **Why it matters:** It made theme-toggle screenshots unreliable as a Phase 0
  verification signal. I fell back to verifying build-green + clean launch/render
  + baseline A/B comparison instead.
- **Reverse / follow-up:** Worth confirming on real hardware that SELECT theme
  toggle still works (it should — the handler code is untouched). If it's broken
  on-device too, that's a separate pre-existing bug, unrelated to this branch.

---

## Phase 0 — Hygiene

### 0.1. Radar-proxy secret cleanup — WAIVED by user
- **What:** The roadmap's Phase 0 item #1 (rotate + move the plaintext
  `tw-radar-prod-…` proxy key out of `src/pkjs/index.js`) is **not being done.**
- **Why:** Jared explicitly instructed (2026-07-03, before sleeping): *"We can
  retain the leaked key as it is embedded in the package. Don't rotate any keys."*
  This overrides the brief's Phase 0 secret-cleanup task and the "rotate the
  radar-proxy shared secret" user action item.
- **Reverse:** If you later want it cleaned: rotate the key in Vercel and move it
  out of the committed client (build-time injection, or an unauthenticated
  rate-limited proxy). Nothing on this branch blocks that.

### 0.2. Redundant theme persistence — consolidated onto persist key 1 (theme.c)
- **What:** Theme was persisted under two keys: `1` (`theme.c`, the live
  read/write path) and `200` (`settings.c` `KEY_THEME`, read at load and written
  only by the dead `settings_save_theme`). Consolidated to **key 1 as the single
  source of truth**:
  - `theme.c` `theme_init()` now migrates a legacy key-200 value **only when key 1
    is absent** (key 1 is always freshest — `theme_set` writes it on every change),
    then **deletes key 200** so the two can never drift again.
  - Removed the key-200 read block from `settings.c` `settings_load()`.
  - Removed dead `settings_save_theme()` (impl + `settings.h` decl) and the
    `KEY_THEME` define.
- **Why:** Master report §4.2. The old load path could let a stale key-200 value
  override a fresher key-1 value; and two write paths risk drift.
- **Reverse:** Re-add `KEY_THEME 200`, restore `settings_save_theme`, and restore
  the key-200 read in `settings_load()`. But the consolidated form is strictly
  better — no reason to.
- **Migration safety:** Chose "key 1 wins when present, else adopt key 200" rather
  than "key 200 wins" to avoid reverting a user's most recent theme change on the
  upgrade launch. Legacy key 200 is deleted after migration.
- **Verification:** `pebble build` green (emery + gabbro); app launches and
  renders the Main card correctly on emery; A/B compared against baseline (no
  behavior difference). Screenshot verification of the theme *toggle* itself was
  limited by E2 above.

---

## Phase 1 — Quick Wins & Stabilization

### 1.1. Animation timeout + `AnimationsEnabled` toggle
- **What:** Decorative animation (hero icon, rotating status banner, settings
  footer hint) now freezes ~8 s after the last activity, and the 10 Hz ticker
  **stops entirely** when fully idle (re-arms on activity) instead of running
  forever. Added a persisted `AnimationsEnabled` master switch (default on) and
  an `anim_kick()` activity signal.
- **Design (freeze-by-not-dirtying, per the phase doc):**
  - `anim.c` tracks `s_deadline_frame`. `anim_kick()` pushes it to
    `s_frame + ANIM_TIMEOUT_FRAMES` and ensures the ticker is running.
  - `prv_tick` only dirties decorative content while decorative is *active*
    (setting on AND before the deadline). It keeps the ticker alive while
    decorative is active OR `refresh_sheet_is_active()`; otherwise it stops
    re-registering (zero idle cost).
  - Crucially, `s_frame` (read by the refresh-sheet spinner via
    `anim_get_frame()`) is **not** frozen — only the *dirtying* of decorative
    content stops. The sheet-active code path is byte-for-byte the old behavior,
    so the spinner is unaffected. This is why freezing the global frame value was
    rejected: it would kill the spinner rotation.
  - `anim_kick()` is called from: button handlers (select/up/down short + select
    long), touch Touchdown, weather-data arrival (`comm.c` `got_anything`), and
    the `AnimationsEnabled` decode.
- **Chosen values / judgment calls:**
  - **`ANIM_TIMEOUT_FRAMES = 80`** (~8 s at 10 Hz). Doc suggested 5–10 s; picked
    the middle. Reverse: edit the `#define` in `anim.c` (50–100 = 5–10 s).
  - **Frozen resting frame:** used whatever frame the icon last drew, not a pinned
    index. Verified the sun/cloud/bob animations render a clean full icon at an
    arbitrary frame (they're smooth periodic motions), so no pinned frame needed.
  - **Persist key `203`** for `AnimationsEnabled` (203 was free; 200–202 / 210–220
    taken). No `WeatherData` change → no cache bump.
  - **Clay decode** mirrors the sibling boolean toggles (`ShowLocation`,
    `LoopNavigation`) — int 0/1 — but also accepts a CSTRING form for robustness.
  - **`update_notes` modal:** its decorative sun now also freezes if the modal is
    left open past the timeout (it reads `anim_get_frame`, which stops advancing
    when idle). Judged acceptable — the modal is transient and this aligns with
    the battery goal. Not special-cased. Reverse: call `anim_kick()` from the
    update_notes repaint tick if you want its sun to always animate.
- **Verification (objective, via md5 of consecutive emulator frames):**
  - Animating right after activity → consecutive frames **differ**. ✅
  - After ~10 s idle → consecutive frames **byte-identical** (frozen). ✅
  - Button press after freeze → frames **differ again** (re-armed). ✅
  - Frozen hero renders as a clean, complete icon (screenshot). ✅
  - `AnimationsEnabled=off` (tested via a temporary default flip, then reverted):
    Main card **static from launch**, hero renders correctly. ✅
  - Refresh-sheet spinner: **not runtime-tested** — emulator touch (pull-to-
    refresh) isn't scriptable via `pebble emu-button`. Relied on the fact that the
    sheet-active path is unchanged from baseline. Worth a manual pull-to-refresh
    check on device/emulator. Programmatic refresh (Phase 5.1) will call
    `anim_kick()` on show so the spinner ticks even from an idle state.
    **Update:** now runtime-verified via Task 5.1 below — the programmatic sheet
    renders its spinner + phrase, confirming the spinner path works with the
    timeout in place.

### 1.2 + 5.1. `SelectTogglesTheme` setting + Main-card button refresh
- **What:**
  - New persisted `SelectTogglesTheme` (default on, key 204). When off, SELECT
    short/long no longer flips the theme on ordinary cards (no-op). When on,
    behavior is unchanged for ordinary cards.
  - **Task 5.1 rides along:** SELECT-short on the **Main** card now triggers a
    manual weather refresh via a new `refresh_sheet_show_programmatic()` — the
    only manual-refresh path button-only platforms will have.
  - Added `refresh_sheet_show_programmatic()` to `refresh_sheet.c`: enters the
    existing OPENING→LOADING state machine directly (no touch tracking), calls
    `comm_request_refresh()`, and `anim_kick()`s so the spinner rotates.
- **Gesture mapping now (per the master-report gesture budget):**
  - SELECT short: Radar→radar refresh · Settings→row toggle · **Main→weather
    refresh (unconditional)** · other cards→theme toggle *iff* SelectTogglesTheme.
  - SELECT long: Settings→no-op · other cards→theme toggle *iff* SelectTogglesTheme
    (Phase 4 will claim SELECT-long for the detail modal).
- **Judgment calls:**
  - **Main SELECT is unconditional refresh**, even when SelectTogglesTheme is on
    (matches the gesture budget: "Main: manual refresh"). This *changes* the old
    behavior where SELECT on Main toggled theme. Theme remains reachable via
    SELECT on other cards (when on) and Clay. Reverse: gate the Main-refresh
    branch behind `!settings_get_select_toggles_theme()` if you'd rather keep
    theme-toggle on Main when the setting is on — but that diverges from the
    documented budget.
  - **When SelectTogglesTheme is off, freed SELECT gestures are no-ops** (not
    repurposed yet) except Main-refresh — exactly as Phase 1.2 specifies; Phase 4
    claims SELECT-long later.
  - Persist key **204**; Clay decode mirrors the sibling toggle pattern.
- **Verification (emery emulator):**
  - SELECT on Main → app log shows `LastUpdated sentinel, fetching weather` +
    `weather sent`; rapid-capture caught the sheet in LOADING with the three-dot
    spinner and a status phrase ("Reading the wind…"); banner then reads
    "UPDATED NOW". ✅ (Also confirms SELECT events reach the app and the spinner
    animates under the Phase 1.1 timeout.)
  - SELECT on an ordinary forecast card (default on) → theme flips light↔dark
    (screenshots). ✅
  - SelectTogglesTheme off (temporary default flip, reverted) → SELECT on an
    ordinary card is a byte-identical no-op; Main refresh branch is independent
    of the setting (code) and unaffected. ✅

---

## Phase 2 — Navigation Refactor & Clay Migration

### 2.1. Long-press BACK for Settings — NOT IMPLEMENTED; fell back to Settings-as-terminal-card (⚠️ overrides a user decision — please review)
- **Decision:** Kept Settings as the last card in the up/down rotation (the
  existing/baseline behavior). Did **not** remove it from rotation and did
  **not** wire long-press BACK. This is the phase doc's sanctioned runner-up.
  Net code change for 2.1: **none** (this DECISIONS entry only).
- **Why — long-press BACK is undeliverable on this SDK 4.9.169 emulator (verified, full chain):**
  1. `window_long_click_subscribe(BUTTON_ID_BACK, 500, …)` — long handler
     **never fires**; holding BACK 800 ms just exits the app. The same
     push/hold/release fires `SELECT` long-click fine, so it's BACK-specific.
  2. `window_raw_click_subscribe(BUTTON_ID_BACK, down, up, …)` — raw down/up
     **also never fire**; a long BACK still exits. (Tried per the `fable`
     consult below, to bypass the click recognizer.)
  3. Only `window_single_click_subscribe(BUTTON_ID_BACK, …)` fires — on
     *release*, with no press event, so hold duration can't be measured.
  4. `window_single_repeating_click_subscribe` could detect a hold, but
     confirming short-vs-hold would delay the short-press exit — which the user
     explicitly forbade ("no added latency to single-BACK exit").
  ⇒ No way to implement long-press BACK here without an undelivered event or
     added exit latency. Firmware consumes BACK for app-exit before the app
     sees a long/raw event.
- **Escalation (tier-2):** This overrides the user's explicit 2026-07-03
  decision (long-press BACK; double-tap rejected), so I ran a synchronous
  `fable` consult. Tiebreak: try `window_raw_click_subscribe` first; if raw is
  also dead, take the fallback (Settings terminal card) and log the chain. Raw
  was dead → fallback taken. Fable also flagged: never ship an
  unverifiable-on-hardware gesture as the *only* route to Settings (lockout
  risk) — which is why I reverted the dead long-BACK code rather than leaving it
  in (in the emulator it left Settings unreachable).
- **Cost:** Phase 2.1's goal ("up/down scrolling is purely weather") is not
  achieved — Settings still appears last in the carousel (master report notes
  this is only mildly disruptive since it's pinned last, not interleaved).
  Phase 4's real dependency was the freed SELECT-long gesture (delivered in 1.2),
  not this — so **Phase 4 is not blocked**. Phase 2.2 (Clay visibility sync) is
  independent of rotation and proceeds.
- **For Jared — decide:**
  1. **Verify on real hardware.** The emulator may not emulate BACK long/raw
     even if hardware delivers it. If a real Pebble delivers raw BACK events,
     reinstate the raw-timer approach (I wrote then reverted it; see this
     commit's parent diff / git history): raw-down starts a 500 ms `app_timer`;
     timer-fires-while-held → open Settings; raw-up → if not consumed, exit.
     Plus `nav_set_enabled(11,false)`, drop Settings from
     `prv_sync_nav_traversal`, and skip the page indicator on a current-disabled
     card in `nav.c`.
  2. If long-BACK can't work on hardware either: keep terminal card (current),
     or revisit double-tap BACK (you rejected it, but it *is* deliverable). I did
     not re-litigate your double-tap rejection.

### 2.2. Clay card-visibility sync — opt-in master toggle; per-card keys (not EnabledMask)
- **What:** Added phone-side control of per-card visibility, gated behind a new
  opt-in master toggle `PhoneManagesCards` (default **off**). When on, incoming
  Clay `CardEnabled*` toggles (one per toggleable card) are applied to the
  on-watch enable flags (persist 210–219) and re-synced to nav. When off,
  on-watch card management is untouched. Card **reorder** stays on-watch (Clay
  has no drag-reorder), per the doc's hybrid recommendation.
- **Deviation 1 — opt-in master toggle instead of "Clay authoritative by default":**
  The `showConfiguration` handler opens Clay from its own localStorage (no seed
  from the watch), so a naive "Clay wins on save" would **silently wipe**
  carefully-curated on-watch card config the first time a user opens Clay for
  any reason and saves. Making it opt-in avoids that. Bonus: default-off means
  the new decode path is dormant, so shipping it — which can't be fully
  round-trip-tested in the emulator (Clay config is interactive) — cannot
  regress existing users. Reverse/upgrade path: implement a watch→Clay seed on
  `showConfiguration` (send current visibility to PKJS, inject into Clay's
  persisted settings before `generateUrl`) and then the master toggle can be
  retired / defaulted on. Noted: turning the master toggle ON adopts Clay's
  current toggle state (defaults all-on on first open), so first opt-in shows
  all cards — the user then curates from the phone. Documented in the toggle's
  Clay description.
- **Deviation 2 — per-card message keys instead of the `EnabledMask` bitmask:**
  The doc suggested reusing the unused `EnabledMask` key. But Clay toggles each
  need their own messageKey regardless, so a bitmask saves no Clay keys and adds
  fragile JS packing (build mask, strip raw keys from the outgoing message).
  Per-card keys (`CardEnabledHours`…`CardEnabledAdvice`) round-trip natively
  through Clay with **zero index.js changes** and decode with the same
  `dict_find` + `atoi`/int32 pattern as the other settings. `EnabledMask`
  remains unused. Reverse: switch to a single `EnabledMask` int + JS packing if
  the extra 10 keys ever matter (they don't today).
- **Key assignments:** persist **205 = `PhoneManagesCards`**. This shifts the
  phase docs' suggested 205 (`AutoHidePrecip`, Phase 3.2) → **206**, and
  `BigMode` (Phase 3.1) → **207**. New message keys: `PhoneManagesCards` + 10
  `CardEnabled*`. No `WeatherData` change → no cache bump.
- **Impl note:** `MESSAGE_KEY_*` are runtime symbols on this SDK, not constant
  expressions, so the key→ToggleId map is an **automatic** (non-static) local
  array in `comm.c` — a `static const` initializer fails to compile.
- **Verification:** Build green. `config.js` parses (node require). Watch-side
  apply path verified end-to-end via a temporary injection (set
  `PhoneManagesCards`+disable Radar, reverted): the Settings card showed Radar
  **unchecked**, and nav-prev from Settings landed on **Golden Hour, skipping
  the disabled Radar** — so `settings_set_enabled` → `prv_apply_card_visibility`
  → `nav_set_enabled` works. The **Clay phone→watch round-trip itself was not
  runtime-tested** (emulator `emu-app-config` is an interactive browser; no way
  to script toggle+save). The decode mirrors three shipped settings, and the
  feature is off by default. Worth a real-phone Clay save check before release.
- **Emulator note:** the temp test left the emery emulator's *persist* with
  Radar disabled + `PhoneManagesCards=true` (harmless; committed code defaults
  are off/enabled). Cold-boot the emulator to clear it if a later check needs
  Radar visible.

---

## Phase 3.2 — Conditional precipitation UI

### 3.2. Auto-hide Precip + Radar when dry (opt-in)
- **What:** New opt-in `AutoHidePrecip` (Clay toggle, persist **206**, default off).
  When on, the Precipitation and Radar cards are auto-hidden while no rain is
  expected soon, and reappear immediately when rain is forecast. Implemented as a
  runtime "auto-hidden" layer separate from the persisted user enable flags:
  **effective visibility = user_enabled AND NOT auto_hidden** (new
  `settings_get_effective_enabled`, now fed to nav by `prv_apply_card_visibility`).
- **Predicate (`comm.c prv_rain_expected`, tunable `#define`s at top of comm.c):**
  rain expected if `rain_alert_min >= 0` OR any `hours_pop[0..5] >= 40%`.
  Deliberately cautious (false-negative averse) — any signal keeps the cards
  shown. Dropped the amount (`hours_precip_x10`) secondary signal for simplicity;
  POP already covers it. Reverse/tune: `AUTO_HIDE_POP_THRESHOLD` (40).
- **Guardrails (all implemented):**
  1. **Fail open** on bad/stale data — `!valid` or `last_updated` older than
     `AUTO_HIDE_STALE_SECS` (**3 h**) → show. (Chose 3 h, not the launch-refresh
     15-min staleness, which is far too aggressive for a hide/show decision —
     data 20 min old is perfectly good for judging rain. Reverse: edit the define.)
  2. **Hysteresis** — hide only after `AUTO_HIDE_DRY_UPDATES` (**2**) consecutive
     dry evaluations; show immediately on rain (counter resets). Asymmetric:
     reappears fast, hides slowly.
  3. **Respects user toggles** — auto_hidden is a separate runtime flag; it never
     mutates the persisted user enable flag, and effective = user AND NOT auto,
     so a user-disabled card is never auto-shown.
  4. **Clay master switch** `AutoHidePrecip`, default off (opt-in).
- **Evaluation runs** after each data update (`comm.c` got_anything), on cache
  load at launch, and when the Clay toggle changes. Both precip and radar are
  hidden/shown together (one setting, per the doc's simpler option).
- **Verification (emery emulator, via temporary forced predicates + hysteresis=1,
  all reverted):**
  - Forced dry (+ real dry data) → **Rain card absent** from the rotation
    (Main→6 Hours→Week→UV, skipping Rain). ✅ (hide)
  - Forced rain → **Rain card reappears** (down-3 = Precipitation). ✅ (show)
  - Fail-open + guardrail-3 use the same `want_hidden=false` / `effective_enabled`
    paths just exercised — verified by construction, not separately screenshotted.
  - Note: the emery emulator's flash **persist survives qemu restarts**, so its
    Touch&Go + Radar cards are user-disabled from earlier-phase test taps — a red
    herring during nav checks, unrelated to auto-hide. The clean signal was the
    always-user-enabled Rain card.

---

## Phase 4 — Advanced Interaction & Data Density

### 4.1 + 4.2. Bottom-sheet detail modals — PARTIAL (6 Hours + Precipitation of 5 cards)
- **What shipped:** New `detail_modal.c/.h` — one reusable bottom sheet that
  slides up from the bottom (modeled on `refresh_sheet.c`: IDLE/OPENING/OPEN/
  CLOSING state machine, layer-frame slide, ease-out, 250 ms). All vector
  drawing. Two modals implemented (the doc's recommended "prove the sheet with
  the zero-plumbing cards first"):
  - **6 Hours → TEMP TREND:** segmented temperature line + point markers,
    min/max annotated (LECO), hour labels; **SELECT toggles a POP overlay**.
  - **Precipitation → RAINFALL:** hourly amount bars, "RAIN IN Nh"/"NO RAIN SOON"
    headline from `rain_alert_min`, POP row + hour labels.
- **Gesture wiring (per the budget):** SELECT-long on a forecast card with a
  detail opens its modal (claims the gesture there); on cards without one it
  still theme-toggles (gated). While open: UP/DOWN locked (consumed), SELECT
  toggles the overlay, **BACK dismisses**, touch drag-down dismisses. Mutually
  exclusive with the refresh sheet (both refuse while the other is active).
- **BACK single-click now subscribed** (`prv_back_click`): dismiss modal if open,
  else exit. Note this is *short* BACK, which **does** fire on this SDK (unlike
  the long/raw BACK that failed in 2.1), and on release with no added latency —
  so it delivers the gesture budget's "BACK dismisses modal/sheet first, else
  exit" without the 2.1 problem. Exit is re-implemented (`window_stack_pop_all`)
  since subscribing overrides the firmware default.
- **Deferred (remaining Phase 4 — clean stopping point):**
  - **Week** (paged-by-day modal), **UV** (UV TODAY gauge + hourly curve),
    **Air Quality** (pollutant breakdown). UV + AQI need new Open-Meteo fields →
    `WeatherData` additions + PKJS fetch/pack → **one coordinated
    `PERSIST_KEY_CACHE` bump (107→108)**, which is why they were left for a single
    batched change rather than started half-way.
  - Touch **long-press-on-element → contextual modal** (`hit_test` callback on
    the Card struct) — not added; SELECT-long + drag-down cover the interaction.
  - Underlying-card **dimming** while the sheet is open — skipped (the card top
    peeks plainly above the sheet, which already reads as "overlay"). Easy to add.
- **Judgment calls:**
  - Content renderers live **in `detail_modal.c`** (dispatched by a `DetailType`
    enum) rather than in each `cards/*.c` as the doc suggested — keeps the v1
    self-contained and reviewable. Moving them next to their cards later is
    mechanical. Reverse: split `prv_draw_hours`/`prv_draw_precip` into the card
    files with public headers.
  - Sheet height = **80%** of screen; the card's top stays visible.
- **Verification (emery emulator, screenshots):** 6 Hours modal opens on
  SELECT-long, renders the trend chart cleanly (fixed an initial hint/label
  overlap), SELECT toggles the POP overlay, UP/DOWN are locked, BACK dismisses,
  a second BACK exits. Precip modal renders (RAINFALL headline + POP + labels;
  bars empty only because the live data is dry). SELECT-long on UV correctly
  falls through to theme toggle. (Verified precip via a temporary card remap —
  reverted — because this emulator's precip card is user-disabled in persist
  from an earlier stray settings tap.)

---

## Phase 4 (cont.) — resumed 2026-07-03 (afternoon session)

Jared reviewed the overnight run, confirmed all 8 commits, and set this session
on autopilot to **finish the Phase 4 forecast modals + add a swipe-up gesture**,
then stop for review. Ordering (my call, he said "decide what's best"):
Week → UV+AQI (batched) → swipe-up. Scope stops after those; the Settings-in-Clay
opt-out is explicitly a **later** session (see the Settings-opt-out entry below).

### 4.3. Week detail modal — paged-by-day (3rd of 5 forecast modals)
- **What:** Added `DETAIL_WEEK`. SELECT-long on the **Week Ahead** card opens a
  bottom sheet with one page per day (5 days). Each page: day label in the header
  chrome (calendar icon), large condition icon, **HIGH / LOW** columns (LECO_28;
  HIGH tinted by condition per the Week card's palette, LOW in secondary), a
  centered droplet + POP% row, and a **page-dot indicator** (filled = current
  day). **UP/DOWN page** prev/next day (clamped, no wrap); BACK dismisses; other
  in-modal gestures unchanged. Zero new plumbing — uses existing `days_*` fields.
- **Judgment calls:**
  - **Paged-by-day, not one long scroll** — the phase doc's recommended default;
    fits the 5-day dataset cleanly and reuses the page-dot vocabulary.
  - **No wrap on UP/DOWN paging** (clamp at day 0 / day 4) — a paged modal with 5
    discrete pages reads more predictably clamped; matches the page dots.
  - **POP row is height-guarded** (`if (pop_y + 18 <= content_bottom)`) so it
    self-omits on short sheets (e.g. a 144×168 basalt where the 80% sheet is
    ~134px) rather than colliding with the dots. On emery/gabbro it always fits.
    Had to tighten the icon/column spacing after a first pass pushed POP below the
    fold on emery — now verified visible.
  - Renderer kept **in `detail_modal.c`** (dispatched by `DetailType`), consistent
    with the existing Hours/Precip renderers — not split into `cards/week.c`.
    Same reverse path noted in 4.1/4.2 applies.
- **Verification (emery emulator, screenshots):** SELECT-long on Week Ahead opens
  the sheet on FRI (HIGH 70 / LOW 50 / 0%); DOWN×2 pages to SUN (rain-cloud icon,
  HIGH 71 / LOW 58 / **30%**, 3rd dot filled) — condition icon, numbers, POP, and
  dots all update; card nav stays locked while paging; BACK returns to the Week
  Ahead card. ✅
- **Note (emulator input):** the emery emulator drops button events
  intermittently and its persist survives restarts (Touch & Go re-enabled itself
  between installs), so card indices shifted mid-test — drove nav one press at a
  time with screenshots to stay oriented. Not a code issue.

### 4.4. UV + Air Quality detail modals — batched WeatherData change (completes 5/5)
- **What shipped (one commit, one cache bump 107→108):**
  - **UV → "UV TODAY":** summary line `UV n  LABEL` (orange) + `PEAK m`
    subtitle, then an **hourly UV curve** (+1h..+6h) drawn as the same
    segmented trend line as the temp chart, with hour labels. Reuses `uv`,
    `uv_max`, and the new `hours_uv[6]`.
  - **Air Quality → "AIR DETAIL":** `AQI n  LABEL` headline in the AQI category
    color, then a **4-pollutant breakdown** (PM2.5 / PM10 / O3 / NO2) as
    labelled bars scaled to the largest value + numeric µg/m³, and a caption
    line that shows `POLLEN: <severity>` when covered else `ug/m3`.
- **Batched plumbing (the reason UV+AQI were deferred to do together):**
  - `weather_data.h`: added `uint8_t hours_uv[6]` + `int pm2_5/pm10/o3/no2`.
  - `PERSIST_KEY_CACHE` **107 → 108** (struct grew; an old blob would leave the
    new fields as garbage). Single bump for both cards, per the doc's warning
    not to invalidate the cache twice.
  - `package.json`: new message keys `PM25/PM10/O3/NO2` + `Hour1Uv..Hour6Uv`.
  - `comm.c`: decode the new keys (PM* near AQI; `hour_uv_keys[6]` in the
    existing hourly loop).
  - `src/pkjs/index.js`: added `uv_index` to the forecast `hourly=` list and
    `pm2_5,pm10,ozone,nitrogen_dioxide` to the air-quality `current=` list;
    pack `HourNUv` in the hourly loop and PM25/PM10/O3/NO2 after AQI. Open-Meteo
    names `ozone`/`nitrogen_dioxide` → shortened to O3/NO2 on the wire.
- **Judgment calls:**
  - **Hourly-UV window = the same +1h..+6h as the 6 Hours card** (6 new keys),
    not a full 24-point day curve (which would be 24 keys and a much bigger
    message). The gauge summary carries "now" and "peak"; the 6-point curve
    shows the near-term trend. Honest trade-off; at night the curve reads low,
    which is correct. Reverse/extend: widen to a day curve later if wanted.
  - **UV curve has no per-point value annotation** — a first pass annotated the
    peak point but it collided with the `PEAK n` line at the top of the chart;
    removed it since the summary already states current UV + peak. The curve is
    a shape indicator only.
  - **Pollutant bars scaled to the max of the four** (relative comparison), not
    to per-pollutant health thresholds — simplest honest "which dominates" read;
    the numbers give the absolute µg/m³. Bar fill uses the AQI **category
    color** so severity still reads at a glance.
  - **`ug/m3` written ASCII**, not `µg/m³` — the Pebble system fonts don't
    reliably carry `µ`/`³` glyphs (they do carry `°`). Avoids tofu boxes.
  - AQI category color helper **duplicated locally** in `detail_modal.c`
    (`prv_aqi_color`) rather than exported from `cards/air_quality.c`, to keep
    the modules decoupled — same pattern as the other self-contained renderers.
  - Mock data (`weather_data.c`) seeded with `uv_max=7`, a declining
    `hours_uv`, and sample pollutants so the modals render pre-refresh.
- **Verification (emery emulator, screenshots):** UV modal opens on SELECT-long
  from the UV card — summary `UV 3 MODERATE / PEAK 7`, clean 6-point curve with
  11–16 hour labels, no collision; BACK returns to the UV card. AQI modal opens
  from Air Quality — `AQI 38 GOOD` (green), PM2.5=6/PM10=8/O3=93/NO2=1 bars
  correctly proportioned (O3 full-width), `POLLEN: LOW`; BACK returns to the
  card. **Live PKJS data actually arrived in the emulator** (real AQI/O3/pollen
  values, not mock) — so the JS fetch+pack round-trip is confirmed end-to-end,
  not just syntax-checked. Build green (emery+gabbro), `node --check` clean.

### 4.5. Swipe-up opens the detail modal (touch models) — Jared's step-6 request
- **What:** On emery/gabbro, an **upward swipe** on a forecast card now opens
  that card's detail modal — a touch shortcut for SELECT-long. Mirrors the
  existing "swipe down = refresh" so the two vertical gestures are symmetric
  (Jared: *"we swipe down already for refresh, swipe up for extended data
  modals"*). No-op on cards without a modal. Drag-down still dismisses an open
  modal; horizontal swipe still navigates.
- **Where:** `TouchWeather.c touch_handler` Liftoff — a new branch
  `dy < 0 && ady > VSWIPE_THRESHOLD(30) && ady > adx` between the horizontal-swipe
  and tap branches (mutually exclusive with both: `adx>ady` vs `ady>adx`). Calls
  the already-wired `prv_detail_for_current()` → `detail_modal_open()`. A forward
  decl of `prv_detail_for_current` was added (it's defined below the `#if
  ENABLE_TOUCH` block).
- **Why it's safe by construction:** pull-DOWN is consumed by `refresh_sheet`
  earlier in Liftoff, and a swipe while a modal is already open is caught by the
  `detail_modal_is_active()` branch at the top of Liftoff (drag-down dismiss) —
  so an upward flick only reaches the new branch when no overlay is active. All
  the same guards the SELECT-long path uses.
- **Verification — build green (emery+gabbro); NOT runtime-tested.** The emulator
  has **no scriptable touchscreen swipe** (`pebble emu-button` is buttons only;
  `emu-tap` is the accelerometer; `emu-control` is interactive-GUI only) — the
  same gap prior sessions noted for pull-to-refresh and touch long-press. The
  branch reuses the proven horizontal-swipe structure and the SELECT-long-verified
  `detail_modal_open`, so risk is low, but **confirm the up-swipe on real
  emery/gabbro hardware** before release. Reverse: delete the branch + forward
  decl.

### Settings-in-Clay opt-out — DEFERRED to a later session (sequencing decided)
- **Context:** In the afternoon handoff Jared floated a new feature — Settings
  fully controllable in Clay, plus an **opt-in to remove the Settings card from
  the carousel** (touch models keep touch interaction; button models manage
  everything from Clay). This is *not* the reverted Phase 2.1 long-press-BACK
  idea (that stays dead — hold-BACK only exits on Pebble hardware, confirmed by
  Jared); it's a different, additive route.
- **Scope call:** Out of scope for this session (his stop-point was "Phase 4 +
  swipe-up"). Left for a future run — noted here so it isn't lost.
- **Sequencing verdict (Jared, this session):** **card reorder must land in Clay
  FIRST**, before the Settings card can be opt-out-hidden. Reason: reorder
  (UP/DOWN-long on the Settings card) is deliberately on-watch only and has no
  Clay equivalent (Phase 2.2 kept it on-watch — Clay has no drag-reorder). If the
  Settings card could be hidden today, a user would lose the *only* way to
  reorder cards. So the future run must:
  1. Add a card-order control to Clay (a numbered list / per-card order value,
     since Clay can't drag-reorder) that round-trips to the on-watch traversal —
     extends the Phase 2.2 `PhoneManagesCards` path.
  2. *Then* add the opt-in "hide Settings card" toggle (persist key: next free is
     **208** — 205 `PhoneManagesCards`, 206 `AutoHidePrecip`, 207 reserved for
     `BigMode`; see 2.2 key map).
- **Note:** this ties into the existing Phase 2.2 upgrade path (a watch→Clay
  visibility seed on `showConfiguration`) — doing that seed first would also let
  the `PhoneManagesCards` master toggle be retired. Worth bundling.

---

## ☀️ AFTERNOON SESSION SUMMARY (2026-07-03) — for Jared's review

Continued on `feature/roadmap-phases`. **3 new commits** on top of the overnight
8, all screenshot-verified on emery, tree clean, `pebble build` green
(emery+gabbro), **nothing pushed**.

**Done this session (Phase 4 forecast modals — now 5/5 — + a touch gesture):**
| Task | What | Verified |
|---|---|---|
| 4.3 | Week detail modal (paged-by-day, UP/DOWN page) | ✅ emulator (FRI→SUN, POP, dots, BACK) |
| 4.4 | UV + AQI modals, **batched** `WeatherData` + cache **107→108** | ✅ emulator; live PKJS data confirmed the fetch round-trip |
| 4.5 | Swipe-up opens the detail modal on touch models | ⚠️ build-only — no scriptable emulator swipe; **check on hardware** |

**Phase 4 is now complete** (all five forecast cards have a detail modal:
6 Hours, Precipitation, Week, UV, Air Quality).

**Things to review (⚠️):**
1. **Cache key bumped 107→108** (4.4). Existing users' weather cache invalidates
   once on upgrade — they'll see mock/last values until the next refresh. Normal
   for a struct change, but it's the reason to batch: only one bump for UV+AQI.
2. **Two paths not runtime-testable in the emulator, verified by build + review
   only:** (a) the swipe-up gesture (4.5 — no scriptable touch swipe), and (b)
   the *background/phone* PKJS path in the general case — though the foreground
   AQI fetch *did* return live data this session, which is a strong signal the JS
   is correct. Both want a real-hardware pass before release.
3. **Settings-in-Clay opt-out** was requested but is **deferred** with a decided
   ordering (reorder-into-Clay first) — see the entry just above.

**Next in the roadmap dependency order** (unchanged from the master report, now
that Phase 4 is done): **Phase 3.1 Stage A** — the ui-metrics/font accessor
refactor across all 12 cards — then **Phase 5** (7-platform expansion), then
**Big Mode (3.1 Stage B)**. Stage A is the heavy shared refactor and the
recommended next target.

**Before any release** (unchanged): bump version in CHANGELOG.md + package.json,
add a "What's New" line (also the discoverability hint for the nav/gesture
changes), do a hardware pass (SELECT/BACK behaviors, swipe-up, background
refresh), then push/publish. Version deliberately **not** bumped mid-roadmap.

---

## Phase 3.1 Stage A — ui font/metrics accessor refactor (2026-07-03, evening)

Resumed on `feature/roadmap-phases`. Jared confirmed Phase 4 complete and set
this session on **Phase 3.1 Stage A** — the shared font/metrics abstraction the
master-report graph puts before Phase 5 and before Big Mode (Stage B). Scope
agreed up front (AskUserQuestion): **fonts + layout metrics**, verify on **emery
+ gabbro**. Pure refactor, **no intended visual change**. **4 new commits**, tree
clean, `pebble build` green (emery+gabbro), **nothing pushed**.

Jared's one explicit worry going in: *"don't lose what we had — don't start a new
method of displaying fonts/positions and lose the current implementation."* The
whole design answers that: every accessor returns the **verbatim current value**;
the current look becomes the protected "Normal" path that Big Mode branches off
later, never something we migrate away from.

### 3.1A.1. Font-role accessors — one accessor per distinct font (`ui.h`/`ui.c`)
- **What:** Added `ui_font_header/body/title/label/caption/number()`. Migrated all
  12 cards + `ui.c` shared helpers off direct `fonts_get_system_font(FONT_KEY_*)`.
- **Design call — one accessor per *distinct font*, not per *semantic role*.** The
  cards use exactly 6 system fonts. Mapping each font to one accessor makes every
  migration a **pure 1:1 rename with zero per-call-site judgment**, so nothing can
  be semantically mis-collapsed (the failure mode Jared feared). Map: `18_BOLD→
  header`, `24_BOLD→body`, `28_BOLD→title`, `14_BOLD→label`, `14→caption`, `LECO_42
  →number`. (Role *names* are approximate — `header` also carries hi/lo values etc.
  — but that only matters to Stage B, which can split a font into two accessors
  then; today all callers of a given font get the identical GFont.) Reverse:
  inline each accessor back to its `fonts_get_system_font(...)`.
- **Accessors are zero-arg**, returning today's font. Big Mode/Phase 5 add the
  scale/screen-class `switch` *inside* the body — call sites never change. Chose
  this over baking a `UiScale`/screen-class enum into the signatures now, precisely
  because inventing new branching before Stage B's real needs exist is the "start a
  new method" risk; the minimal wrap preserves current output by construction.

### 3.1A.2. Layout-metric accessors — macro-alias, zero call-site churn
- **What:** The 3 shared layout constants now route through `ui_margin_x()` /
  `ui_header_y()` / `ui_header_height()` (values `round 20/rect 12`, `round 24/rect
  8`, `24`). The `UI_MARGIN_X` / `UI_HEADER_Y` / `UI_HEADER_HEIGHT` macros are
  **redefined as zero-cost aliases** to the functions.
- **Why macro-alias instead of migrating ~75 call sites:** the function is the
  single scaling point Big Mode needs; keeping the macro name means **no card file
  changed for metrics**, so pixel-identical is guaranteed *by construction* (macro
  expands to a function returning the identical value). Safe verified none of the
  ~25 uses each are in compile-time-const contexts (array sizes/case/static), so a
  function call is a valid substitution everywhere. Reverse: restore the macros to
  their literal `PBL_IF_ROUND_ELSE(...)` bodies and delete the functions.
- **Deliberately NOT migrated: per-card `PBL_IF_ROUND_ELSE(...)` layout literals**
  (e.g. main_card's `yshift`/`hero_shift`/`icon_y`). Those are **card-local layout
  logic, not shared metrics** — forcing them into global accessors would be
  over-abstraction and risk. Stage B/Phase 5 can introduce card-specific scaled
  layout where a card actually needs it. This keeps Stage A a bounded, shared-layer
  refactor.

### Verification method (and its honest limits)
- **Pixel-diff harness** (`scratchpad/pxdiff.py`, PIL): reports differing-pixel
  count + bounding box between a HEAD/committed baseline and the migrated build.
- **main_card (the layout-heaviest card) proven byte-identical on BOTH branches:**
  emery/rect (408→229px) and gabbro/round (229px) diffs are **confined entirely to
  the animated hero-icon bbox** — every glyph/value/coordinate outside it is
  byte-for-byte identical. Same-time stash diffs used to remove the two confounds
  (see below). The metrics change was isolated the same way → hero-only (229px).
- **The two confounds** that make naive md5 unreliable here, and how they were
  neutralized: (a) the **hero icon animates** (`anim_get_frame`) — non-deterministic
  frame, so it always shows in the diff; accepted as noise, bounded to its bbox.
  (b) the **"UPDATED NOW" banner is time-sensitive** — a stale baseline drifts to
  "UPDATED Xm ago"; neutralized by capturing baseline+after close in time via
  `git stash` (build HEAD/committed → cap → pop → build → cap).
- **The other 11 cards: build-green + a visual pass on emery covering all 6 font
  roles** (header, body, title=28B on Sun Cycle/Night Sky, label, caption=PEAK 7 /
  FETCHING RADAR, number=UV "2" / AQI "39") — every card rendered unchanged, no
  clipping/wrong fonts. They were **not** individually byte-diffed (see deviation).

### Deviations from the handoff plan (flagged for review)
1. **Batched the 11 remaining cards into ONE commit, not 11.** The handoff said
   "one card at a time, one commit per card." But the remaining migration is a
   *single deterministic scripted transform* (exact-string → exact-accessor map),
   already byte-proven on the hardest card. Eleven commits each claiming
   independent pixel-diff would **overstate** the per-card verification; one honest
   commit describing exactly how each was verified is truer. Accessor layer +
   main_card are their own commits (they proved the pattern). Net: 4 commits —
   (1) accessor layer + ui.c helpers, (2) main_card, (3) remaining 11 cards,
   (4) metrics accessors. Revert granularity lost is low (scripted-identical change).
2. **Did not byte-diff all 12 cards × 2 platforms.** The emery emulator is flaky
   (crashes to "Modules is not responding", exits to the system watchface on idle —
   it's a watchapp, `watchface:false` — and drops button events), so navigating 24
   card/platform captures for before/after diffs is impractical and low-value given
   the source-level guarantee. Verified the hardest card byte-exact on both
   branches + a full-role visual pass; relied on the mechanical 1:1 guarantee for
   the rest. **A hardware/emulator pass eyeballing each card at leisure is still
   worthwhile before release**, but no regression is expected.
3. **Emulator housekeeping:** wiped the emery persist (`qemu_spi_flash.bin`) to
   recover from the wedged state — this reset card enable flags to defaults
   (all-on) and re-triggered the one-shot update-notes modal on first launch
   (dismissed with BACK). Harmless; committed defaults unchanged.

### What this unblocks / next
- Stage A is the master-report's **shared prerequisite**. With it in, **Phase 5**
  (7-platform expansion) and **Big Mode / Stage B** can both scale by editing the
  accessor bodies (add `UiScale` + screen-class branching there) instead of
  touching call sites. The accessors are currently single-axis (return current
  value); adding the two axes is the first task of whichever of those phases runs
  next.

---

## Phase 5 — Platform expansion to 6 shippable watches (2026-07-03, evening)

Resumed on `feature/roadmap-phases`. Jared set this session on **Phase 5**, the
first consumer of Stage A. Two scope decisions up front (AskUserQuestion):
**(1) enable all 6 shippable platforms this session** (aplite excluded — RAM
no-go), and **(2) decide radar-on-64KB by measurement, not napkin math.**
**6 new commits**, tree clean, `pebble build` green for **all 6 platforms**,
**nothing pushed**. `targetPlatforms` went `["emery","gabbro"]` →
`["basalt","chalk","diorite","emery","flint","gabbro"]`.

The overriding constraint all session: **emery + gabbro must stay pixel-
identical** (they're already their own screen classes). Every change below is
compile-time gated so those two compile byte-for-byte unchanged — verified after
*every* commit by diffing the built `pebble-app.bin` code+data body against the
pre-Phase-5 HEAD (**0 differing bytes past the 168-byte app header** — the header
only differs in build timestamp/CRC). This is a stronger proof than the Stage A
screenshot diff (no hero-animation / banner-time confounds).

### ☀️ Phase 5 morning summary (what shipped)
| Platform | Class | Color | Result |
|---|---|---|---|
| basalt | small-rect 144×168 | color | ✅ hero font shrunk, radar carved out |
| chalk | **small-round 180×180** (new class) | color | ✅ banner re-tuned, radar carved out |
| diorite | small-rect 144×168 | **B&W** | ✅ 5.3 accent/banner fallback, radar carved out |
| flint | small-rect 144×168 | **B&W** | ✅ same as diorite |
| emery | large-rect 200×228 | color | ✅ **byte-identical** (unchanged) |
| gabbro | large-round 260×260 | color | ✅ **byte-identical** (unchanged) |

aplite: **not shipped** (24KB App RAM < ~51KB binary — unchanged from the doc).

### 5.0. Screen-class axis added to the Stage A accessors (commit 1)
- **What:** A 4-class compile-time axis (`UI_SCREEN_SMALL_RECT` / `_SMALL_ROUND` /
  `_LARGE_RECT` / `_LARGE_ROUND`) in `ui.h`, derived from the SDK's own
  `PBL_ROUND` + `PBL_DISPLAY_WIDTH` defines (verified present on every platform).
  The 3 shared metric accessors (`ui_margin_x/header_y/header_height`) became
  4-way tables. The two LARGE branches return the **verbatim** pre-Phase-5
  `PBL_IF_ROUND_ELSE` values; the two SMALL classes started **equal to their
  large sibling** (tuned later per platform with screenshot evidence, never up
  front — the "prove before you spread" discipline the handoff insisted on).
- **Why compile-time (not runtime):** matches the existing `PBL_IF_ROUND_ELSE`
  idiom, zero runtime cost, and lets emery/gabbro resolve to the identical
  constants → byte-identical. Reverse: collapse each table back to
  `PBL_IF_ROUND_ELSE`.

### 5.1. small-rect hero numeral font (commit 2, basalt)
- **What:** `ui_font_number()` (LECO_42, the temp/UV/AQI hero) → **LECO_36_BOLD_
  NUMBERS on small-rect**. On 144px the LECO_42 temp overflowed its box and
  clipped to "6…". One accessor change fixes temp + UV + AQI heroes at once.
  Large classes keep LECO_42 verbatim. Degree glyph verified present in LECO_36.
- **Reverse:** drop the `#if UI_SCREEN_SMALL_RECT` branch.

### 5.2. Radar carve-out on non-128KB platforms (commit 3) — **measured**
- **Measurement (the "decide by measurement" ask):** the radar buffer is
  `malloc`'d lazily (25.6KB staging + a 25.6KB GBitmap coexist, ~51KB peak).
  A real **basalt build reports 14,108 bytes free heap** (footprint 51,428/64KB).
  14KB < 25.6KB ⇒ radar physically cannot fit on any 64KB platform. Conclusive;
  not napkin math.
- **Mechanic chosen (low-risk):** rather than skip `nav_register("Radar",…)`
  (which the positional `IDX_*` index scheme would break — Settings is a hard-
  coded index 11), I reused the **Phase 3.2 nav-skip path**: a new
  `TW_RADAR_SUPPORTED` macro (defined only on emery/gabbro) makes
  `settings_get_effective_enabled(TOGGLE_RADAR)` return false on every other
  platform. Nav then skips radar exactly like a user-disabled card, so the card
  is **never navigated to → its buffer never allocates → no OOM.** `TOGGLE_RADAR`
  keeps its id + persist key on all platforms (settings survive if data roams).
- **Verified:** on basalt, UP×2 from Main lands on **Golden Hour** (Main→Settings
  →Golden Hour) — radar absent from the rotation.
- **⚠️ Known deferred (cosmetic):** the **Settings card still lists an inert
  "RADAR" row** on carved-out platforms (toggling it does nothing — effective
  stays off). Hiding it cleanly needs a shared settings cursor/reorder change
  (radar is `ToggleId` 8, *not* last, so a compile-time count reduction would
  excise the wrong card); judged too risky-for-cosmetic under the "keep emery/
  gabbro identical" constraint. Left as a follow-up.

### 5.3. small-round banner (commit 4, chalk)
- **What:** chalk is a genuinely new screen class — the round layout values were
  gabbro-260-tuned. The status-banner `pad_bottom` (35px) pushed the pill into
  mid-content on 180px (it covered the 17:00 row on 6 Hours). small-round now
  gets an 18px pad so the banner hugs the bottom, reclaiming a row. Large-round
  (gabbro) keeps 35px verbatim.
- **Reverse:** drop the `#if UI_SCREEN_SMALL_ROUND` branch in
  `ui_draw_status_banner`.

### 5.4. B&W color-fallback pass (commit 5, "5.3" in the phase doc)
- **What (the critical B&W fix):** on 1-bit the SDK auto-reduces the accents
  (ChromeYellow/VividCerulean/VividViolet) to **GColorWhite — invisible on the
  light theme's white bg.** On diorite the hi/lo temps, arrows, droplet and gauge
  fills all **vanished.** Accents now fall back to `theme_fg()` on `PBL_BW`
  (`PBL_IF_COLOR_ELSE`), always the opposite of the bg. Hue-based meaning is
  preserved by **shape** (up/down arrows, distinct icons/markers) per the doc.
- **Banner on B&W:** an accent pill collapses to fg (black text on it vanishes)
  and a muted pill dithers illegibly, so on `PBL_BW` the banner draws a **solid
  inverted pill** (fg bg + bg text) — crisp in both rain/updated modes.
- **Audit:** 47 accent call sites; the accents-as-*fill* uses (UV/precip/chart)
  paint on the bg, so fg-fill stays visible. The only accent-as-bg-under-text
  case is the banner, handled above.
- **Not changed:** `theme_muted` (tracks/dividers/inactive dots) still dithers —
  acceptable for thin chrome; `theme_secondary` still DarkGray (dithers but reads).
  Revisit only if a screenshot pass finds an illegible spot.
- **Reverse:** restore the three accent bodies to their bare `GColor…` and drop
  the `#if PBL_BW` banner branch.

### Verification (and its honest limits)
- **emery+gabbro byte-identical** after every commit (the primary safety net) —
  see above. basalt/chalk/diorite/flint each **screenshot-verified on their
  emulator**: main card + a header/gauge/list card render legibly, hero fonts fit,
  radar absent from rotation, B&W accents visible, banners crisp.
- **NOT a full 12-card × 6-platform matrix.** The emulator is flaky (the watchapp
  **idle-exits to the system watchface**, so navigation drops into the Timeline
  "No events" between shots — drove installs + fast button bursts to stay in-app).
  I verified ~8 basalt cards, main+6Hours on chalk, main+AQI on diorite, main on
  flint. **The remaining per-card small-screen fine-tuning is the 5.2 sweep** and
  is left as documented follow-up (see below).

### ⚠️ Things to review / flagged
1. **Pre-existing UV tofu (NOT this session's bug):** the UV hero draws a **tofu
   box** when live UV is negative/unknown — the LECO fonts lack a minus glyph.
   **Reproduced identically on the untouched emery LECO_42 path**, so it predates
   Phase 5 and is data-dependent (only shows on bad UV data). Worth a separate
   fix (clamp UV to ≥0, or special-case "—"). Flagged, not fixed.
2. **Radar "RADAR" row still shows in Settings** on the 4 carved-out platforms
   (inert). Cosmetic; see 5.2.
3. **5.2 per-card sweep is incomplete by design.** Shared-metric scaling is in
   (margins, header, hero font, banner), but individual cards tuned for 200/260
   still have minor small-screen crowding — e.g. chalk 6 Hours hides one row
   under the banner; basalt main has slight FEELS/hi-lo crowding. All **legible
   and shippable**, none clipped. A leisurely screenshot-matrix polish per card
   is the natural next increment.
4. **Everything still rides the unmerged branch** — Phase 4's hardware sign-off
   (SELECT/BACK, swipe-up, background refresh) still gates the merge, and Phase 5
   adds its own **hardware-pass items**: confirm radar-absence + B&W legibility on
   real diorite/flint, and that 64KB heap holds up in real use (detail modals /
   refresh sheet on 14KB free).

### What this unblocks / next
- **Big Mode (Stage B)** is now the last big roadmap item: it adds the **scale
  axis** (Normal/Big) inside the same accessor bodies that now carry the screen-
  class axis. The pattern is proven twice over (Stage A single-axis → Phase 5
  screen-class); Stage B is the third axis in the same place.
- Or, the **5.2 per-card polish sweep** (small-rect + small-round value tuning,
  card by card, screenshot matrix) if a tighter look is wanted before Big Mode.

## Phase 5.2 — Per-card layout sweep for the two small classes (2026-07-03, later)

Resumed on `feature/roadmap-phases`; Jared picked the **5.2 polish sweep** over
Big Mode (finish Phase 5, low-risk, de-risks the hardware review). **8 commits**,
tree clean, `pebble build` green for all 6, **nothing pushed**. Same overriding
constraint as Phase 5: **emery + gabbro stay byte-identical** — re-verified after
*every* commit with `cmp -l /tmp/ref-<p>.bin build/<p>/pebble-app.bin | awk '$1>168'`
== 0 (references snapshotted from the pre-5.2 HEAD build).

### Root cause the sweep addressed
The Phase 5 Stage-A scaffolding routed only the **shared** metrics (margin_x,
header_y, hero font, banner pad) through the screen-class axis. Every card's
**own** vertical layout still used `PBL_IF_ROUND_ELSE(round, rect)`, so the two
new small classes inherited the wrong sibling's absolutes: **chalk (small-round
180×180) got gabbro-260 values**, **basalt/diorite/flint (small-rect 144×168) got
emery-200 values**. On the shorter screens the fixed-from-top content and the
fixed-from-bottom status banner collided — content bled *under* the banner on
many cards. Fix pattern per card: give the two small classes their own
compressed anchors / smaller fonts / smaller gauge radius; keep the large
branches verbatim so emery/gabbro don't move.

### Commits (each verified on basalt + chalk, spot-checked on diorite B&W)
1. **nav page indicator (chalk).** `indicator_y` was a hardcoded `229` on every
   round display — a gabbro-260 absolute that falls *off* the 180px chalk screen
   (dots invisible). small-round now anchors `rb.size.h - 14` like rect.
2. **main card hero block.** icon→temp/hi-lo→FEELS positioned from the top, wind
   row from the bottom; on the small classes they collided (wind glyph rode into
   "FEELS"). New compressed anchors per small class; kept `temp_h ~44` so FEELS
   still clears the hi/lo column (low sits at `temp_y+46`). **Gotcha:** moving the
   `temp_drop` computation earlier changed gabbro's codegen by 12 bytes (it's a
   runtime value there; emery folds it to a constant) — kept it at its original
   post-`block_shift` position to stay byte-identical.
3. **6 Hours** — 6 rows in the 18px header font overflowed the banner; small
   classes use the 14px label font, `row_h 15`, tighter top offset.
4. **Week Ahead** — same, 5 day-rows, 14px label font + `row_h 16` (the existing
   top-clamp then floors the block).
5. **Sun Cycle** — 40px icon / 56px row pitch pushed the sunset under the banner;
   small classes use icon 36 / pitch 42.
6. **UV** + 7. **Air Quality** — the half-arc gauge used the gabbro-260 radius
   (72) on chalk; shrunk to 42/44, lifted the value box, tightened the
   label/PEAK (UV) and label/pollen (AQI) offsets; AQI pollen badge drops to the
   caption font on small.
8. **Night Sky** — 56/58px moon + title-font phase name buried the name words +
   "% LIT"; small classes use moon 40, header/label name fonts, tighter offsets.

### Codegen gotcha worth remembering (byte-identical discipline)
Adding **int** locals for the large branch is safe (they fold to the same
constants — emery/gabbro stayed 0). Introducing a **GFont variable** for the
large branch is *not* — storing the font pointer instead of calling
`ui_font_*()` inline at the draw site shifted both binaries (AQI, first attempt).
Rule: on the large/verbatim path, select fonts **inline via `#if`**, never via a
hoisted variable.

### Still open (not blockers)
1. **UV-value tofu** on unknown/negative UV — still reproduces (a solid block in
   the gauge); pre-existing, unrelated to layout, deferred. A clamp/"—" fix is a
   clean separate task (LECO lacks the minus glyph).
2. **update_notes window clips on chalk** (small-round) — the changelog splash
   truncates a line ("…ens working"); transient, shown once per version, not
   touched this sweep.
3. **Inert "RADAR" row in Settings** on carved-out platforms — still there
   (Phase 5 known item; needs the shared cursor/reorder change).
4. basalt Week precip `%` sits tight against the right edge on 144px — legible,
   left as-is.

### Next
- **Big Mode (Stage B)** — the last big roadmap item; add the Normal/Big **scale
  axis** inside the same accessor bodies that now carry the screen-class axis.
- The three small deferred fixes above (UV tofu clamp, chalk update_notes clip,
  Settings RADAR row) are each a tidy standalone commit if a finishing pass is
  wanted first. **→ Done in the Phase 5 finishing pass below.**

## Phase 5 finishing pass — the three deferred fixes (2026-07-03, later still)

Resumed on `feature/roadmap-phases`; Jared picked the **3 small deferred fixes**
over jumping straight to Big Mode, to make Phase 5 spotless first. **3 commits**,
one per fix, tree clean, `pebble build` green for all 6, **nothing pushed**. The
two layout-only fixes kept **emery + gabbro byte-identical** (verified with
`cmp -l ref-<p>.bin build/<p>/pebble-app.bin | awk '$1>168'` == 0 on both, refs
snapshotted from the pre-pass HEAD). The UV clamp is a genuine cross-platform
data guard, so it intentionally changes emery/gabbro **in the bad-data case only**.

### F1. UV clamp — `cards/uv.c` (NOT byte-identical, by design)
- **What:** The UV hero draws through the LECO numerals font, which has no minus
  glyph, so a negative/unknown live UV rendered "-1" with a tofu box, and the
  half-arc gauge swept backwards. UV index is physically 0+, so clamp the drawn
  value to `>=0` up front and feed the clamp to the gauge sweep, the hero
  numeral, and the qualitative label. Bad data now reads **"0 / LOW"** with an
  empty gauge.
- **Why not "—":** an em-dash is also tofu in a numbers-only font; 0 is the
  honest floor and matches the empty gauge. The detail-modal UV summary uses a
  Gothic font (has a minus), so it never had the tofu — scope was just the card.
- **Verification (emery):** forced `uv=-1` → renders "0 / LOW" (LECO's chunky
  zero, which reads as a block at emulator scale but is the real 0 glyph — I
  disambiguated by forcing `uv=6`, which rendered a clean "6 / HIGH" with the
  orange sweep, proving the render path and that the "-1" block was the clamped
  zero, not tofu). This fix touches all platforms' code intentionally.

### F2. update_notes fits on chalk — `update_notes.c` (emery/gabbro byte-identical)
- **What:** The splash's sun + headline + version-divider header was tuned for
  the tall large screens; on the short small classes it ate most of the height,
  so the note wrapped under the fold and — worst on **chalk (180 round)**, where
  the bottom curve also clips the body's left edge — the last visible line
  rendered half-cut mid-word ("…eps working").
- **Fix (small classes only):** shrink the sun (32→24), tighten the header's
  vertical rhythm, and reclaim right-side body width so the note wraps to fewer
  lines. On **small-round additionally** hold the scroll viewport 16px above the
  bottom so no line lands in the round clip band.
- **Byte-identical discipline (a re-learned lesson):** my first attempt hoisted
  the header offsets into shared `int` locals that the *large* path also used
  (`head_top = UI_HEADER_Y`, etc.) — storing the `ui_header_y()` result in a
  local shifted codegen and cascaded to **13k differing bytes** on both emery and
  gabbro. Fix: split with `#if` and keep the large branch the **verbatim inline
  expressions** (same rule as the 5.2 GFont gotcha — don't hoist a value the
  verbatim path uses). The round scroll inset is gated to `UI_SCREEN_SMALL_ROUND`
  (chalk), NOT `PBL_IF_ROUND_ELSE`, so gabbro (large-round) stays untouched.
  Re-verified 0/0.
- **Verification:** chalk shows 4 full clean lines with the rest scrolling in to
  "the app."; basalt (small-rect, also on the changed path) renders cleanly.

### F3. Hide the inert RADAR row in Settings — `settings.*` + card + TouchWeather.c (emery/gabbro byte-identical)
- **What:** On carved-out platforms radar is force-off but the Settings card
  still drew an inert RADAR checkbox. The 5.2-flagged risk was that radar is not
  last in the visual order, so a count reduction drops the wrong row.
- **Fix:** a radar-free **"visible" view** over the toggleable rows, used by the
  card draw, the SELECT-toggle handler, the cursor wrap, and the reorder — on
  carved-out platforms only. `settings_visible_count/id()` filter radar wherever
  it sits; `move_up/down` swap the moved visible card with its nearest *visible*
  neighbor in `s_visual_order`, stepping over the hidden radar slot so radar
  keeps its place. Underlying size-10 arrays, persist keys (`KEY_TOGGLE_BASE+8`),
  and the nav traversal (radar stays in the order, disabled) are unchanged, so
  settings still survive roaming.
- **Byte-identical mechanic:** the `SETTINGS_VIS_COUNT` / `SETTINGS_VIS_ID(i)`
  macros expand to the **exact original tokens** (`SETTINGS_TOGGLEABLE_COUNT` /
  `settings_visual_id(i)`) when `TW_RADAR_SUPPORTED`, and `cursor_advance` /
  `move_up` / `move_down` keep verbatim bodies under `#if` — so emery + gabbro
  are 0/0.
- **Verification:** basalt + chalk Settings now end at GOLDEN HR with no RADAR
  row or checkbox; emery/gabbro unmoved.

### Still open after this pass
- The rotating footer hint on the Settings card **overlaps the lower rows on the
  small screens** (basalt 144, chalk 180) — a pre-existing density issue,
  slightly *relieved* by dropping the radar row (10 rows → 9 toggle rows), not
  introduced here. Not fixed (out of scope for the radar removal); a small-class
  footer-position tune is a clean separate task if wanted.
- basalt Week precip `%` tight on the right edge (unchanged, legible).

### Next
- **Big Mode (Stage B)** is now the only big roadmap item left — Phase 5 is
  spotless. Add the Normal/Big **scale axis** inside the same accessor bodies
  that carry the screen-class axis; prove on one or two spots (emery/gabbro
  byte-identical with Big off, one small platform renders Big right) then spread.

## Phase 3.1 Stage B — Big Mode: mechanism + first two cards (2026-07-03, night)

Resumed on `feature/roadmap-phases`. Jared set this session on **Big Mode**, the
last big roadmap item. Three scope decisions up front (AskUserQuestion):
**(1)** the per-card Big-Mode *content* was designed by a **Fable subagent**
(planning only — see the plan captured below), **(2)** high contrast is an
**override flag** on the existing light/dark modes (not a third theme mode), and
**(3)** land the **mechanism + prove on 2 cards** (Main + 6 Hours), then STOP for
review before spreading to the other 10. **4 commits** (plumbing, mechanism,
Main, 6 Hours) + this entry, tree clean, `pebble build` green for all 6,
**nothing pushed**.

### The load-bearing difference from Phase 5: Big Mode is RUNTIME, not compile-time
The screen-class axis is resolved with `#if defined(UI_SCREEN_*)`, so emery/gabbro
compiled byte-for-byte identical and `cmp` past the 168-byte header was the safety
net. **Big Mode is a runtime toggle** (`settings_get_big_mode()` read on the draw
path), so a real branch exists in the binary and the `cmp` net **no longer
applies**. The invariant shifts to: **with Big Mode OFF, every platform renders
pixel-identical to pre-Stage-B.** That holds *by construction* — every accessor's
else-path and every card's pre-branch code is the verbatim prior code, and the
Big paths are `if (big) {…}` / early-returns — and was **confirmed by screenshot**
(emery Big-OFF Main+6 Hours returned the verbatim LECO/FEELS/wind and dense 6-row
layouts).

### B1. BigMode setting plumbing (commit 1) — no visual change
- Mirror of the `AnimationsEnabled` pattern end-to-end: `settings.c/.h` getter+
  setter on **persist key 207** (reserved for BigMode by the 2.2 key map),
  `comm.c` decode (int or CSTRING) that calls `s_update_cb()` for an immediate
  repaint, `BigMode` message key in `package.json`, and a Clay toggle in
  `config.js`. Default **off**. No `WeatherData` change → **no cache bump**.

### B2. Scale axis in the accessors + high-contrast override (commit 2) — the mechanism
- **`ui.c` — third axis in the accessor bodies.** Each `ui_font_*()` gains a
  `settings_get_big_mode()` branch bumping the role one tier (per the Fable map
  below), holding the two SMALL screen classes at the Normal tier for
  header/body where a bigger font wouldn't fit 144/180px. The status pill grows
  (`banner_h` 22→28, +20 wide) so the enlarged label fits.
- **Hero-numeral font — DEVIATION from Fable (chose `BITHAM_42_BOLD`, not
  `ROBOTO_BOLD_SUBSET_49`).** Fable specced ROBOTO_49 (49px, +7px) with a
  BITHAM fallback for negative temps. But ROBOTO_BOLD_SUBSET_49 is a *clock*
  subset (digits+colon) whose **degree glyph is unverified**, and the main-card
  temp is `"72°"`. `BITHAM_42_BOLD` is a **full font** — it carries both `°` and
  `-`, so no tofu and **no per-sign fallback needed** (dissolves Fable's whole
  minus-glyph special case). Cost: 42px vs 49px; the readability win in Big Mode
  comes mostly from the simplified layouts. **Verified live** on emery/gabbro/
  basalt that `"72°"` renders with a clean degree glyph. Reverse/revisit: swap
  the one line in `ui_font_number()`'s big branch if 49px is wanted (then
  reinstate a `< 0` fallback for the main temp).
- **`theme.c` high-contrast override (the flag, per scope choice 2).**
  `theme_secondary()` and the three accents collapse to full-contrast `theme_fg()`
  when Big Mode is on — same collapse the `PBL_BW` fallback already does, and the
  Big layouts carry meaning by **shape** (↑/↓ arrows, distinct icons). **Refinement
  of Fable's plan:** `theme_muted()` is deliberately **NOT** collapsed — it drives
  the inactive page-indicator dots, which must stay distinct from the fg active
  dot. A card that needs a hue to *be* the data (AQI category color) can opt back
  out locally when its Big layout lands.

### B3 + B4. Main + 6 Hours simplified Big layouts (commits 3, 4)
- **Main:** icon + huge centered temp + one `↑high ↓low` line; drops location/
  FEELS/wind+humidity. **6 Hours:** 3 rows sampling hours {0,2,4} (spans the full
  window), time + icon + temp only; drops the wind + precip columns and the odd
  hours (all still in the detail modal); header `6 HOURS`→`HOURLY`. Both are
  early-return branches keyed off `settings_get_big_mode()`, per-screen-class
  metrics, verbatim Normal path below.
- **Verified (emulator, temp default-flip to true then reverted):** Big-ON renders
  correctly on **emery (large-rect) + gabbro (large-round)** (phase-doc mandate)
  **and basalt (small-rect 144×168)** — no clipping, degree glyph clean, pill
  enlarged; the un-redesigned cards (e.g. Touch & Go) just show bigger fonts in
  their old layout, as expected mid-sweep. Big-OFF returned verbatim on emery.

### The Fable Big-Mode plan for the REMAINING 10 cards (spread from here)
Captured so the next session doesn't re-run Fable. Content policy: *maximize font/
object size, fewer elements, high contrast*. **Big-Mode `ui_font_*` map** (already
implemented in B2): header→24B(large)/18B(small), body→28B(large)/24B(small),
title→BITHAM_30_BLACK, label→18B, caption→14B, number→BITHAM_42_BOLD.
Per card (RETAIN → CUT, cut data lives in the detail modal unless noted):
- **Week** *(simplified — near-mechanical copy of 6 Hours)*: 3 days small / 5 large,
  `day + icon + ↑high ↓low`; cut POP% + days 4–5-on-small (→ modal). Arrows replace
  the hue-coded high/low.
- **Precipitation** *(simplified)*: 3 fat bars (Now/+2h/+4h) with % above + hour
  below; cut +1h/+3h (→ modal).
- **Golden Hour** *(simplified)*: **2 gold rows only** (AM/PM) at 28B. **CUT the two
  BLUE-hour rows entirely — NO modal fallback (data is gone).** ⚠️ product call.
- **UV** & **Air Quality** *(font-bump/trim, twins)*: **drop the arc gauge**, giant
  number (BITHAM_42_BOLD) + qualitative label (28B); PEAK/pollen line on large only.
  AQI keeps its **category color** (the one hue that IS data) — opt back out of the
  contrast collapse locally.
- **Sun Cycle** *(font-bump)*: both rows, BITHAM_30_BLACK times, bigger icons;
  SUNRISE/SUNSET word captions on large only.
- **Night Sky** *(trim)*: bigger moon + phase name (28B/24B); cut "% LIT"; **drop the
  status banner on this card** to fit. Moon's locked cream/navy palette is **exempt**
  from the contrast collapse (shadow direction = the data). ⚠️ banner drop.
- **Advice/Touch & Go** *(trim)*: promote the data headline to 28B, keep the quip
  (24B); cut the tier badge and **drop the status banner** for room; may need a
  short-phrase audit (~≤35 chars) on small classes. ⚠️ banner drop.
- **Radar** *(exempt face)*: bitmap unchanged, bigger crosshair + state text; Fable
  **recommends auto-hiding Radar from the Big-Mode rotation** (irreducible hue
  detail — antithesis of the policy; the visibility machinery exists). ⚠️ product call.
- **Settings** *(exempt)*: unchanged in Big Mode — 12 rows can't scale without a
  scrolling cursor (disproportionate lift for a rarely-visited management screen).

### ⚠️ Product-level cuts to CONFIRM WITH JARED before spreading
None touch Main/6 Hours, so they're deferred to the spread session, but they
remove data with no fallback and should be his call:
1. **Golden Hour** drops the two BLUE-hour rows entirely (no modal exists).
2. **Night Sky** and **Advice** drop the status banner (rain-alert reachability).
3. **Radar** hidden from the Big-Mode rotation (a visibility policy change).

### Still open / next
- **Spread Big Mode to the other 10 cards** per the Fable plan above — Week +
  Precipitation first (simplified, mechanical from 6 Hours), then the font-bump/
  trim cards, resolving the three ⚠️ product cuts with Jared first.
- Same carry-over discipline: prove each card Big-ON on emery + gabbro (+ a small
  class), confirm Big-OFF stays pixel-identical (screenshot, not `cmp`), one
  commit per card. Codegen gotcha still applies to the Normal path (inline fonts
  via `#if`, don't hoist a verbatim-path value into a shared local).
- **Everything still rides the unmerged branch** — Phase 4 + Phase 5 hardware
  sign-off still gates the merge; Big Mode adds its own hardware-pass item
  (legibility/contrast on a real low-vision pass, and the enlarged pill on device).

## Phase 3.1 Stage B — Big Mode spread to the remaining 9 cards (2026-07-03/04, night)

Continued on `feature/roadmap-phases`. After reviewing the mechanism + Main/6
Hours, **Jared rejected all three of Fable's product-level cuts** and had Fable
(the same planning subagent, context intact) revise the design; then the spread
was implemented. **9 more commits** (one per card, Stage B 5/N–13/N), tree clean,
`pebble build` green for all 6, **nothing pushed**. The three constraints Jared
set (now the durable rules):

1. **Golden Hour must keep all four milestones** (no blue-hour cut; no modal
   exists as a fallback).
2. **The status banner is mandatory on every weather card** — it can never be
   dropped to make room. (Settings is the one exemption Jared confirmed — it's a
   config screen with a rotating footer hint, not weather status; AskUserQuestion.)
3. **Radar stays in the Big-Mode rotation** (it's already a full-screen bitmap).

### Fable's revision (the design that was implemented)
- **Golden Hour → a 2×2 grid** (rows blue/gold × columns AM/PM) so all four times
  are visible at once — chosen over pagination (an accessibility mode shouldn't
  hide half the data behind a gesture). Band identity is carried by **shape**
  (filled□ = blue, outline□ = gold) so it survives the contrast collapse; the
  AM/PM column header lets the " AM"/" PM" suffix be stripped. This is the novel/
  highest-risk layout.
- **Night Sky + Advice → banner restored**, content re-fit above the (now taller)
  banner: Night Sky shrinks the moon on small classes and drops only "% LIT";
  Advice drops the tier badge, caps the headline to one line, and lets the quip
  wrap in the freed space.
- **Radar → banner ADDED.** Fable's re-read caught that **radar.c never drew a
  status banner at all** (only a RAINVIEWER footer) — so it's the single card that
  needed the banner *added*, done in Big Mode only (Normal radar unchanged).

### Per-card implementation notes (all early-return `if (big) {…}` branches)
- **Week** (5/N): 3 days small / 5 large, `day + ↑high ↓low` with arrows replacing
  the hue-coded high/low; `ui_font_header` (18B/24B) because two temps per row is
  too wide for 24B on 144px; icon dropped on small, POP% + days 4–5-on-small → modal.
- **Precipitation** (6/N): 3 fat bars (Now/+2h/+4h); +1h/+3h → modal.
- **UV** (7/N) + **Air Quality** (8/N): drop the arc gauge, giant `ui_font_number`
  + big `ui_font_body` label; PEAK/pollen on large only. **AQI keeps its EPA
  category color** on the number+label (opts out of the contrast collapse — the one
  hue that IS the data; note the MODERATE band routes through `theme_accent_orange`
  which still collapses to fg, an accepted minor degrade since the word shows).
- **Sun Cycle** (9/N): font-bump — both rows, bigger icons, times auto-scale to
  BITHAM_30_BLACK via `ui_font_title`. (Skipped Fable's large-only SUNRISE/SUNSET
  captions as a minor simplification.)
- **Night Sky** (10/N), **Golden Hour** (11/N), **Advice** (12/N), **Radar** (13/N):
  as in the revision above.

### ⚠️ Verification status — HONEST, please read
- **Build green on all 6 platforms** after every commit.
- **Big-OFF parity: guaranteed by construction** for all 9 — each is an early-return
  `if (settings_get_big_mode()) {…return;}` inserted *before* verbatim Normal code
  (radar uses `big ? new : orig` inline ternaries resolving to the exact originals).
  No Normal-path code was touched. (Main/6 Hours Big-OFF were screenshot-verified
  earlier; the same mechanical guarantee covers these.)
- **Big-ON per-card screenshots: NOT captured this session — BLOCKED by emulator
  failure.** Partway through the spread the emulator's **app-launch RPC died**: on
  every platform, `pebble install` reports success but the watch stays on "Install
  an app to continue" and phonesim screenshots time out. Confirmed **environmental,
  not a code regression** — reverting to Big-OFF (the shipped Normal code) fails to
  launch identically, and VNC framebuffer captures (which bypass the phonesim) also
  show the app never running. Earlier the same session, with the emulator healthy,
  Big-ON rendered correctly for **Main + 6 Hours on emery, gabbro AND basalt** — the
  9 spread cards reuse that identical, proven early-return pattern, but their
  individual layouts (positions/overflow) are **computed, not yet eyeballed.**
- **First things to eyeball once an emulator/device is healthy** (highest layout
  risk): (1) **Golden Hour** 2×2 grid fit on 144px (two times + shape per row is
  tight — the cluster clamps to margins), (2) **Advice** quip clipping on small-rect
  (~2-line box at 24B — the known limit; a short-phrase small-class pool is the
  fix), (3) **Week** two-temp rows on 144px, (4) **Radar** banner overlapping the
  bottom of the 160px bitmap on emery/gabbro (accepted, consistent with how the
  footer already overlays the image), (5) the UV/AQI giant number vertical centering.

### Still open / next
- **Visual pass on the 9 spread cards** (emery + gabbro + a small class) once the
  emulator is healthy or on hardware — tune the per-card anchors flagged above.
  Nothing is pushed, so amend/fixup commits are cheap.
- **Big Mode is now feature-complete across all 12 cards** (Settings intentionally
  exempt). Remaining before merge: the visual pass above, plus the standing Phase 4
  + Phase 5 hardware sign-off and a real low-vision legibility/contrast pass.

### ✅ Verification COMPLETED (2026-07-04, after Jared rebooted the machine)
The emulator's app-launch failure above was environmental — a machine reboot
cleared it. Big-ON was then screenshot-verified:
- **basalt (small-rect 144×168 — the tightest class): ALL 9 spread cards render
  correctly** — Advice (badge dropped, big 1-line headline, quip wraps, banner),
  Week (3 days `↑hi ↓lo`), Precipitation (3-bar frame + banner), UV ("4 / MODERATE",
  no gauge), Air Quality ("29 / GOOD" in **green** — category colour kept), Sun
  Cycle (big times + both rows), Night Sky (bigger moon + name, **banner present**),
  and **Golden Hour** — the novel 2×2 grid — with all four milestones visible
  (filled■ blue / outline□ gold × AM/PM), correct chronology, fits 144px.
- **gabbro (large-round): Main, Radar, Golden Hour** confirmed — Radar shows the
  **added banner** + bigger FETCHING text (the live bitmap/crosshair needs a phone
  feed, unavailable in-emulator), and Golden Hour shows the large-class BLUE/GOLD
  word labels beside the shapes.
- No layout defects found; none of the flagged risks materialised (Golden Hour
  fits 144px, the Advice quip reads fine). The one residual is the documented
  Advice small-rect quip length limit (long phrases can still clip — headline +
  banner always show); a short-phrase small-class pool remains the clean follow-up.
- Still pending (unchanged): the standing Phase 4 + Phase 5 hardware sign-off, a
  real low-vision legibility/contrast pass, and Radar's live bitmap+crosshair on a
  phone-connected build.

---

## Settings-in-Clay opt-out (2026-07-04) — the deferred feature, built in the decided order

Session start: entire roadmap feature-complete (Big Mode Stage B verified), tree
clean, nothing pushed. This session builds the deferred "Settings-in-Clay
opt-out" per Jared's baked decisions: (1) watch→Clay seed first, (2) Clay card
REORDER second (custom drag component), (3) HideSettingsCard opt-in last, gated
on PhoneManagesCards with an on-watch fail-safe. No WeatherData change → no
cache bump (still 108).

### SC.1. Watch→Clay card-state seed (the keystone)
- **What:** The watch now PUSHES its card order + per-card enable flags +
  PhoneManagesCards to PKJS (new `comm_send_card_state`): once ~1s after every
  foreground launch, and after every on-watch Settings change (row toggle,
  UP/DOWN-long reorder). PKJS caches the state in localStorage
  (`watchCardState`); `showConfiguration` injects it into Clay's persisted
  settings via the public `clay.setSettings()` (which writes the same
  `clay-settings` store `generateUrl()` reads) before opening the page. Clay
  therefore always opens showing the watch's TRUE current card state — a Clay
  save can no longer silently wipe on-watch card config. This closes the Phase
  2.2 "reverse/upgrade path" note.
- **Wire format:** order rides on a new `CardOrder` message key as a CSV of
  ToggleIds ("9,0,1,…" = the Settings-card visual order); enables reuse the ten
  `CardEnabled*` keys (0/1) so PKJS can inject them 1:1 into Clay's toggle
  values (as booleans — the checkbox manipulator stores booleans). `CardOrder`
  doubles as the phone→watch order key in SC.2, and its presence identifies a
  watch→phone state push in the PKJS `appmessage` handler (weather fetch is
  never triggered by it).
- **Delivery discipline:** the push is debounced 900ms so a chain of reorder
  long-presses coalesces into one message, and retries up to 5× (2s apart) if
  the outbox is busy (the launch push races the 750ms initial-refresh request).
  Deliberately best-effort — a lost push self-heals on the next change/launch.
  Background (wakeup) launches do NOT seed (keeps the 28s budget clean).
- **PhoneManagesCards NOT retired:** the 2.2 entry said the seed would allow
  retiring the master toggle; it is deliberately KEPT — SC.3 repurposes it as
  the required gate for hiding the Settings card (now load-bearing).
- **Verification (emulator, basalt):** PKJS log shows the launch seed
  (`watch card state cached: {"CardOrder":"9,0,…"`, all enables true), a fresh
  push after an on-watch row toggle (CardEnabledAdvice:false → true on revert),
  a fresh push after an on-watch reorder ("0,9,1,…" → restored "9,0,1,…"), and
  `injected watch card state into Clay settings` on `emu-app-config`. Emulator
  persist restored to defaults afterwards. Build green, all 6 platforms.

### SC.2. Card order in Clay — custom drag-reorder component
- **What:** New custom Clay component `cardorder` (src/pkjs/cardorder.js,
  registered via `clay.registerComponent`) — a real drag-to-reorder list per
  Jared's decision, not per-card number dropdowns. Rows show the 10 toggleable
  cards with a right-side ≡ grip; dragging the grip (touch or mouse) swaps rows
  live with a translateY follow. Value = CSV of ToggleIds in visual order
  (same format as the SC.1 seed). Config item sits in the Card Visibility
  section (messageKey `CardOrder`, default "9,0,1,2,3,4,5,6,7,8" = the watch's
  s_default_order). Self-contained: no external hosts; Clay inlines the
  component's template/style/JS into the data-URI config page.
- **toSource constraint (important for future components):** Clay serializes
  registered components into the page with `toSource()`, so every function in
  the component object must be closure-free — the ToggleId→label table is
  duplicated inside `set()` for that reason. ES5 only (old phone webviews).
  Also: `initialize()` runs BEFORE the first `set()` (ClayItem.initialize →
  clayItem.set(value) in clay-config.js `_addItems`), so the drag wiring uses
  event delegation on the (initially empty) list container.
- **Watch side:** new `settings_apply_order_csv` (settings.c) — strict parse
  (exactly 10 entries, digits only, no junk/trailing comma) + the existing
  `prv_is_valid_permutation`; persists KEY_CARD_ORDER only on change; returns
  true only on a valid AND different order. comm.c decodes `CardOrder` inside
  the PhoneManagesCards-gated block (order from Clay obeys the same opt-in as
  visibility). The visibility callback was widened to a combined
  `prv_cards_changed_from_phone` (TouchWeather.c) = apply enable flags + rebuild
  traversal — on-watch UP/DOWN-long reorder keeps working unchanged (both paths
  share s_visual_order).
- **Verification (log-based via SC.1's seed echo — the emulator degraded to the
  known fully-wedged state mid-session, so screenshots of the reordered
  Settings card were not obtainable; a machine reboot is the documented fix and
  was not performed autonomously):**
  - Valid injected order ("8,7,6,5,4,3,2,1,0,9" + PhoneManagesCards:1, temp
    PKJS injection since emu-app-config isn't scriptable): applied + persisted —
    the next launch seed echoed exactly that order with PhoneManagesCards:true,
    reproduced on emery AND diorite.
  - Invalid orders ("9,9,7,6,5,4,3,2,1,0" dupe; "junk,order"): both ACKed
    (no crash) and rejected — the relaunch seed still echoed the reversed order.
  - Traversal rebuild is the same prv_sync_nav_traversal that runs from persist
    every launch (screenshot-verified all roadmap long).
  - Emulator persists left polluted by the test were cleared with
    `pebble kill` + `pebble wipe` after the session's emulator work.
- **NOT verifiable in the emulator:** the drag interaction itself (config page
  is an interactive browser). A standalone browser harness is provided for a
  mouse test (see session summary), and the REAL-PHONE Clay save test remains
  REQUIRED before ship (added to the hardware sign-off list).
