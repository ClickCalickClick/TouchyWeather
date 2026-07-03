# Phase 1 — Quick Wins & Stabilization

**Goal:** Kill the app-open battery drain and stop accidental theme toggles, with minimal architectural change. Both tasks are genuinely low-effort. Task 1.2 additionally frees the SELECT-long gesture that Phase 4 needs.

---

## Task 1.1 — Battery optimization (animation timeout)

### Problem
The 10 Hz animation ticker in `anim.c` runs continuously whenever the app is open and never stops, redrawing the animated hero icon (and rotating banners) indefinitely. On a smartwatch that is real, measurable drain if the app is left foregrounded.

### Current state
- `anim.c:7` `ANIM_PERIOD_MS 100` (10 Hz).
- `prv_tick` (`anim.c:12`) self-re-registers via `app_timer_register` every 100 ms — infinite loop, no timeout.
- `s_frame` monotonic counter (`anim.c:10`), read via `anim_get_frame()` (`anim.c:53`).
- Selective dirtying already present (`anim.c:19-34`): card 0 every tick; rain banner every 40 frames; Settings hint every 25 frames; refresh sheet every tick.
- Consumers of motion: `main_card.c:78` (hero icon), `ui_draw_auto_banner` (most cards), `refresh_sheet.c` spinner, `update_notes.c` sun.

### Proposed design
Add a **deadline / frame-budget** to the ticker so decorative animation freezes after a timeout, while keeping the ticker alive for cases that still need it.

1. Introduce `ANIM_TIMEOUT_FRAMES` (e.g. 80 frames = 8 s at 10 Hz; make it easy to tune between 5–10 s).
2. In `prv_tick`, track frames elapsed since the last "activity" event. Once past the deadline:
   - **Stop dirtying the hero icon and banners** (the decorative work).
   - **Keep re-registering the timer only while it still has a live consumer** — specifically while `refresh_sheet_is_active()` is true (spinner) or a nav transition is animating. When nothing needs it, stop re-registering entirely (fully idle → zero timer cost) and re-arm on the next activity.
3. **Reset the deadline on activity:** card change (`nav_next`/`nav_prev`), data arrival (`comm.c` update callback), button/touch input, and refresh-sheet open. Add a small `anim_kick()` (or `anim_reset_timeout()`) entry point that these call sites invoke.
4. **Settings toggle** (`AnimationsEnabled`, default on): when off, never run decorative animation at all — the hero icon renders a single static frame. Freeze immediately rather than after the timeout.

Design note: freezing the hero icon means `icon_draw_condition_animated` must be able to render a stable "resting" frame. Confirm each sub-animator (`icons.c:339-420`) looks acceptable at a frozen frame, or pin a chosen frame index when frozen.

### Files to touch
- `anim.c` / `anim.h` — deadline logic, `anim_kick()`, respect the new setting.
- `main_card.c` (and any card reading the frame) — render a static frame when animation is disabled/frozen.
- `settings.c` / `settings.h` — new persisted `AnimationsEnabled` + getter/setter.
- `comm.c` — decode `MESSAGE_KEY_AnimationsEnabled` from Clay (cstring→bool, per the `atoi` pattern at `comm.c:97+`); call `anim_kick()` on data arrival.
- `TouchWeather.c` — call `anim_kick()` on button/touch/nav events.
- `src/pkjs/config.js` — add the Clay toggle. `package.json` — add the message key.

### New keys
- **Message key:** `AnimationsEnabled` (add to `package.json` `messageKeys`).
- **Persist key:** next free int (e.g. `203`; keys `200-202` and `210-220` are taken). No `WeatherData` change → **no cache-key bump needed**.

### Dependencies & risks
- **Risk:** breaking the refresh-sheet spinner or nav slide by over-freezing. Mitigate by keeping the ticker alive while `refresh_sheet_is_active()` or a transition is running, and by verifying pull-to-refresh visually after the change.
- No dependency on other phases.

### Open questions for research
- Exact timeout value (5 vs 8 vs 10 s) — check any battery guidance in `plans/BACKGROUND_*`.
- When fully idle, is stopping the timer entirely safe, or should it fall back to a slow heartbeat? (Prefer full stop; re-arm on activity.)
- Does the rotating rain/updated banner need to keep toggling past the timeout for correctness, or is a frozen banner acceptable? (Likely freeze is fine.)

### Verification
- `pebble build && pebble install --emulator emery` (add `--vnc` if headless).
- Screenshot the Main card, wait past the timeout, screenshot again → hero icon should be frozen; battery-relevant redraws stopped.
- Trigger pull-to-refresh → spinner must still animate.
- Toggle `AnimationsEnabled` off in Clay → hero icon static from the start.

---

## Task 1.2 — SELECT button theme-toggle setting

### Problem
Users reflexively press SELECT and accidentally flip light/dark mode.

### Current state
- `prv_select_click` (`TouchWeather.c:137`) → theme toggle on all non-Radar, non-Settings cards.
- `prv_select_long` (`:162`) → theme toggle everywhere except Settings.

### Proposed design
Add a persisted `SelectTogglesTheme` setting (default **on**, to preserve current behavior). When **off**, SELECT-short and SELECT-long no longer toggle theme on ordinary cards.

**Decided (2026-07-02, see master-report gesture budget):** the freed gestures have assigned futures — SELECT-short on the **Main card** becomes manual refresh (Phase 5.1, which should ride along in this phase since it's tiny and every non-touch platform needs it), and SELECT-long becomes the detail-modal trigger (Phase 4). Within Phase 1 itself, implement the freed gestures as **no-ops**; the later phases claim them.

Radar (force-refresh) and Settings (row toggle) behavior are unaffected — they don't toggle theme.

> **Rides along: Task 5.1 (button-triggered refresh).** SELECT-short on Main → `refresh_sheet_show_programmatic()` → `comm_request_refresh()`. Details in [Phase 5 §3](PHASE_5_PLATFORM_EXPANSION.md). This is the only manual-refresh path non-touch platforms will ever have, and it benefits emery/gabbro users immediately.

### Files to touch
- `settings.c` / `settings.h` — `SelectTogglesTheme` persisted getter/setter.
- `TouchWeather.c` — gate the theme-toggle branches in `prv_select_click` / `prv_select_long`.
- `comm.c` — decode `MESSAGE_KEY_SelectTogglesTheme`.
- `config.js` + `package.json` — Clay toggle + message key.

### New keys
- **Message key:** `SelectTogglesTheme`.
- **Persist key:** next free int (e.g. `204`). No cache bump.

### Dependencies & risks
- **Unblocks Phase 4** by making SELECT-long available. Low risk; behavior change is opt-in.
- If chosen, keep the theme reachable somewhere (Clay `Theme` setting already exists) so disabling SELECT doesn't strand users in one mode.

### Open questions for research
- ~~Should this be a 3-way select rather than a bool?~~ **Resolved:** bool. The gesture roadmap is fixed in the master-report gesture budget; Phase 4 claims SELECT-long unconditionally, so no 3-way needed.

### Verification
- Build + install on emery.
- With setting on: SELECT toggles theme (unchanged). With setting off: SELECT is a no-op on ordinary cards; Radar refresh and Settings row-toggle still work; Clay `Theme` still switches modes.
