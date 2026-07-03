# Phase 3 — Accessibility & Smart UI

**Goal:** Broaden usability for older eyesight (Big Mode) and reclaim screen space when it's dry (conditional precip UI).

> **Effort correction:** The backlog labels both tasks "medium." In reality **Task 3.2 is low effort** (the mechanism and data already exist) and **Task 3.1 is the highest-effort item in the entire roadmap** (fonts/layout are hardcoded per card with no abstraction). Sequence 3.2 early (alongside/after Phase 1–2) and treat 3.1 as its own project done last.

---

## Task 3.2 — Conditional precipitation UI  *(low effort — do this early)*

### Current state
- Card hiding already exists: `nav_set_enabled(idx, bool)` (`nav.c:278`) skips a card in navigation and hides it from the page dots.
- Forecast data already present in `WeatherData` — **no phone-side work needed**:
  - `hours_pop[6]` — precip probability +1h…+6h (covers the "next 4–6 hours" window exactly).
  - `hours_precip_x10[6]` — precip amount, +1h…+6h.
  - `rain_alert_min` — minutes until rain, -1 if none.
- Affected cards: Precipitation (index 4, `cards/precipitation.c`) and Radar (index 10, `cards/radar.c`).

### Proposed design
After each data update (`comm.c` update callback), evaluate a "rain expected soon" predicate and auto-hide/show the precip + radar cards.

**Predicate (tune during research):** rain expected if `rain_alert_min >= 0 && rain_alert_min <= WINDOW`, OR any of `hours_pop[0..N]` ≥ threshold (e.g. N=6, threshold 30–40%). Use amount (`hours_precip_x10`) as a secondary signal to avoid hiding on high-POP-but-trace-amount hours.

**Guardrails the backlog explicitly asks for (prevent false negatives):**
1. **Fail open on bad data.** If `WeatherData.valid` is false or the data is stale (old `last_updated`), **show** the cards. Never hide based on missing/old data.
2. **Hysteresis.** Require the dry condition to hold across consecutive updates (or a min-hidden duration) before hiding, and show immediately when rain appears — asymmetric thresholds so the radar reappears fast but hides slowly.
3. **Respect the user's manual toggles.** Auto-hide may only hide a card the user has *enabled*; it must **never auto-show a card the user disabled**, and must not overwrite the persisted user preference — track auto-hidden state separately from the user's `210-220` enable flags so toggling back is clean.
4. **Clay master switch** `AutoHidePrecip` (default off, opt-in) so users who always want radar keep it.

Implementation: introduce a runtime "effective visibility = user_enabled AND NOT auto_hidden" that feeds `nav_set_enabled`, without mutating persisted user flags.

### Files to touch
- `comm.c` — after update, compute predicate and apply effective visibility.
- `nav.c` / `settings.c` — a place to hold auto-hidden state distinct from persisted user enable flags; ensure the traversal rebuild (`TouchWeather.c:43`) uses effective visibility.
- `config.js` + `package.json` — `AutoHidePrecip` toggle + message key.
- `comm.c` — decode `MESSAGE_KEY_AutoHidePrecip`.

### New keys
- **Message key:** `AutoHidePrecip`. **Persist key:** next free int (e.g. `205`). **No `WeatherData` change → no cache bump.**

### Open questions for research
- Exact window (4 vs 6 h), POP threshold, and amount floor.
- Staleness cutoff for "fail open" (reuse whatever the refresh logic considers stale).
- Should hiding both precip *and* radar be one setting or two?

### Verification
- Mock dry data (`weather_data_init_mock` or a crafted payload) → precip + radar hidden, page dots shrink.
- Mock incoming rain → both reappear immediately.
- Mark data invalid/stale → both shown (fail-open).
- User disables Radar manually → stays hidden regardless of predicate; re-enabling restores normal behavior.

---

## Task 3.1 — Big Mode  *(highest effort — do last)*

### Current state — why this is expensive
- **No font or layout abstraction.** Every card calls `fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD)` etc. directly, with hardcoded coordinates; layout constants are compile-time macros in `ui.h` branched only by `PBL_IF_ROUND_ELSE`. There is no runtime hook to swap sizes.
- Shared draw helpers exist (`ui_draw_header`, `ui_draw_card_header_with_icon`, `ui_draw_status_banner`, `ui.c:82-114`) but each card also draws its own body directly.
- Theme colors are functions (`theme.c`), two modes only.

### Proposed design (two stages)

**Stage A — introduce a metrics/scale abstraction (the real work):**
- Add a `UiScale` concept (Normal / Big) and route font + key layout constants through accessor functions keyed by scale, e.g. `ui_font_header()`, `ui_font_body()`, `ui_font_number()`, `ui_row_height()`, `ui_margin_x()`. Start in `ui.h`/`ui.c`.
- Migrate cards to use these accessors instead of direct `fonts_get_system_font` / literals. This is a per-card sweep across all 12 cards; the shared helpers reduce but don't eliminate it.
- **(Rev 2) Stage A is now a shared prerequisite:** the same accessor layer must also branch on *screen class* for [Phase 5's](PHASE_5_PLATFORM_EXPANSION.md) multi-platform layout work. **(Rev 3, verified against the SDK)** there are **four** screen classes, not three: small-rect 144×168 (aplite/basalt/diorite/flint), small-round 180×180 (chalk), large-rect 200×228 (emery), large-round 260×260 (gabbro — *not* 180×180 as earlier assumed; today's round branch is gabbro-tuned). Design the accessors with **two axes — screen class × scale** — from the start, and sequence Stage A *before* Phase 5 (see master-report sequencing). Big Mode's visual layer (Stage B) then lands last on the stabilized base.

**Stage B — Big Mode content policy:**
- Big Mode maximizes font sizes and object boundaries, and **prioritizes readability over aesthetics**: fewer elements per screen, larger icons, and **hardcoded high-contrast colors** (a third theme mode or a Big-Mode override in the color functions) rather than the standard muted/secondary grays.
- Some cards may need a simplified Big layout (e.g. Main card shows temp + condition only, larger).

### Files to touch
- `ui.h` / `ui.c` — scale accessors, Big-Mode metrics.
- `theme.c` / `theme.h` — high-contrast color variant for Big Mode.
- Every `cards/*.c` — swap direct font/layout calls for the accessors; add Big layouts where needed.
- `settings.c` / `settings.h`, `comm.c`, `config.js`, `package.json` — `BigMode` setting + message key.

### New keys
- **Message key:** `BigMode`. **Persist key:** next free int (e.g. `206`). **No `WeatherData` change → no cache bump.**

### Dependencies & risks
- **Risk:** the per-card sweep is broad and easy to under-scope. Do Stage A as a standalone refactor (no behavior change) first, verify parity via screenshots, then layer Big Mode on top.
- Sequencing (Rev 2): Stage A after Phase 4, then Phase 5 consumes it, then Stage B (Big Mode proper) last — see master-report dependency graph.

### Open questions for research
- Which specific fonts for Big Mode (GOTHIC_24_BOLD, LECO_42, larger?) and do round (gabbro) vs rect (emery) need different Big layouts?
- Full third theme mode vs. a "high-contrast override" flag on the existing two modes?
- Which cards get simplified Big layouts vs. just larger fonts?

### Verification
- Stage A: build after the refactor with Big Mode off → screenshots must match pre-refactor (parity).
- Stage B: enable Big Mode → screenshot each card on emery *and* gabbro; verify legibility, contrast, and no clipping/overflow.
