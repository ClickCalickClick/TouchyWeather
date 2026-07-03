# Overnight Autopilot — Decisions Log

Running log of judgment calls made while executing the roadmap autonomously
(2026-07-03, overnight). Each entry: **what** was chosen, **why**, and **how to
reverse**. Newest phase last. This is Jared's morning review sheet.

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
