# Phase 2 — Navigation Refactor & Clay Migration

**Goal:** Separate settings from the weather carousel so up/down scrolling is purely weather, and make configuration less fiddly. This establishes the clean navigation Phase 4 assumes.

> **Correction to the backlog premise:** The Settings card is already pinned *last* in the carousel (`TouchWeather.c:222-233`, index 11), not interleaved with weather cards, so it interrupts scrolling less than the backlog describes. Also, this refactor by itself does **not** "enable older hardware" — that's now scoped properly as [Phase 5](PHASE_5_PLATFORM_EXPANSION.md). Frame the value as cleaner UX + a foundation for Phase 4.
>
> **Phase 5 consideration:** on the five button-only platforms, BACK is the *only* exit gesture — the exit-latency tradeoff below weighs even heavier there. Whatever is chosen must respect the master-report gesture budget.

---

## Task 2.1 — Access settings via double-tap BACK

### Current state
- Cards: `Card { name, draw }`, no lifecycle hooks (`nav.h:8`). Settings is card index 11, always enabled, always last (`nav.c` + `TouchWeather.c` traversal build at `:43`).
- **BACK is unhandled** → default single-press exits the app.
- Reorder/toggle of cards happens *inside* the Settings card via UP/DOWN-long and SELECT (`TouchWeather.c:177-207`).

### Proposed design
1. **Remove Settings from the up/down rotation.** In the traversal builder (`prv_sync_nav_traversal`, `TouchWeather.c:43`) stop appending Settings to the slot list; the weather cards then form a clean loop.
2. **Reach settings via long-press BACK** — **DECIDED by the user (2026-07-03)**, double-tap rejected. Use `window_long_click_subscribe(BUTTON_ID_BACK, 500, handler, NULL)` to jump to the Settings card. Single BACK keeps its default instant exit — `window_long_click_subscribe` does **not** delay the short-click path the way `window_multi_click_subscribe` would. Verify on-device/emulator that a long-press does not also fire the default exit on release (provide an empty `up_handler` if needed to swallow it).

**Also decided:** settings must be editable in **both locations** — the on-watch Settings card (reached by long-press BACK) *and* Clay on the phone. This makes Task 2.2's hybrid sync a hard requirement, not a nice-to-have: whatever is togglable on the watch must have a Clay counterpart and stay reconciled (see 2.2 precedence rules).

**Keep discoverability:** the Settings card leaves the *weather* rotation but must not become invisible — first-run hint or the existing Settings footer hint should teach the long-press.

### Files to touch
- `TouchWeather.c` — traversal builder; add BACK multi-click/long-click subscription in the click config provider (`:209`); handler to navigate to Settings.
- `nav.c` / `nav.h` — possibly a `nav_goto(idx)` helper if one doesn't exist for jumping directly to a card.

### New keys
- None required (pure on-watch nav change). No persist/message/cache changes.

### Dependencies & risks
- ~~The exit-latency tradeoff~~ **settled: long-press BACK** (user decision, 2026-07-03).
- Interacts with Phase 4: freeing BACK gestures and cleaning the rotation is exactly what Phase 4's modal interactions want. Note Phase 4 also uses BACK to dismiss modals — long-press-BACK-for-Settings must not fire while a modal is open (dismiss wins).
- **Risk:** on some firmware, BACK long-press is system-reserved for quick-launch; verify `window_long_click_subscribe(BUTTON_ID_BACK, …)` actually receives the event in a watchapp on SDK 4.9. If it doesn't, fall back to keeping Settings as the terminal card (the runner-up option) and log it in DECISIONS.md.

### Open questions for research
- ~~Long-press BACK vs double-tap BACK vs keep-Settings-as-last-card~~ **Resolved: long-press BACK + Clay parity** (see above).
- Does removing Settings from rotation interact with `LoopNavigation` edge-exit behavior (`prv_nav_step`, `nav.c:250`)?

---

## Task 2.2 — Migrate configuration to Clay

### Current state
- Clay (`@rebble/clay`) already hosts Theme, TimeFormat, Units, UseDewPoint, LoopNavigation, BackgroundUpdateInterval, LocationOverride, ShowLocation (`config.js`, decoded `comm.c:63`).
- Card **enable/disable** and **visual order** live on-watch (`settings.c` keys `210-220`), edited in the Settings card. An `EnabledMask` message key already exists in `package.json` for carrying enable state.

### Proposed design (hybrid — recommended)
Clay has **no native drag-to-reorder control**, so a full migration of ordering is awkward. Split by fit:

- **Move to Clay:** per-card visibility toggles (one toggle per card). Clay is a natural fit; sync via the existing `EnabledMask` key. Decide sync direction — recommend **Clay is authoritative for visibility** on save, with the watch reflecting it; keep the on-watch toggle as a convenience mirror or retire it.
- **Keep on-watch:** card **reordering** (drag isn't feasible in stock Clay). Either keep the UP/DOWN-long reorder in the Settings card, or build a custom Clay component later.

Reconcile the two sources of truth: define precedence (last-writer, or Clay-on-save wins) and ensure persisted keys `210-220` stay the on-device cache of whatever Clay sends.

### Files to touch
- `config.js` — add a Card Visibility section (toggle per card). `package.json` — confirm `EnabledMask` and any new keys.
- `src/pkjs/index.js` — include visibility in the settings dict sent on `webviewclosed`.
- `comm.c` — decode `EnabledMask` (or per-card keys) → `settings_set_enabled` + `nav_set_enabled`.
- `cards/settings.c` / `settings.c` — reconcile on-watch toggles with incoming Clay state; keep reorder on-watch.

### New keys
- Reuse `EnabledMask` if present; otherwise a bitmask int message key. Persist reuses existing `210-220`. **No `WeatherData` change → no cache bump.**

### Dependencies & risks
- **Risk:** two sources of truth for visibility (watch + Clay) drifting — define precedence explicitly.
- **Risk:** Clay reorder isn't natively supported; don't over-invest trying to force it.
- Builds on 2.1's cleaned rotation.

### Open questions for research
- ~~Does `EnabledMask` already round-trip today, or is it declared but unused?~~ **Resolved (2026-07-03):** declared in `package.json:139` only — **zero references anywhere in `src/`**. It's a free, unused key: no legacy behavior to preserve, so the Clay-authoritative sync direction can be designed cleanly. `comm.c` will need a fresh decode branch.
- Is a custom Clay component for reordering worth it, or is on-watch reorder good enough long-term?

### Verification
- Build + install emery.
- Toggle a card off in Clay → save → confirm it disappears from rotation and page dots on the watch.
- Reorder on-watch → confirm order persists across relaunch and isn't clobbered by a later Clay save (per chosen precedence).
- Double-tap/long BACK → lands on Settings; single BACK → still exits promptly.
