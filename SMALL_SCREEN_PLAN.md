# TouchyWeather — Small-Screen Remediation Plan
### basalt · chalk · diorite · flint  (emery + gabbro FROZEN)

Review basis: all 68 screenshots in `store/screenshots/{basalt,chalk,diorite,flint}/` at 4×/3× zoom,
cross-referenced against `src/c/ui.c`, `src/c/nav.c`, `src/c/detail_modal.c`, `src/c/cards/*.c`.
**No code has been changed.**

---

# PART 0 — THE LOCK: how emery + gabbro stay bit-identical

This is the first thing to get right, because every fix below depends on it.

`ui.h` already resolves exactly one of four compile-time classes per build:

| macro | platforms | resolution | colour |
|---|---|---|---|
| `UI_SCREEN_SMALL_RECT` | basalt, diorite, flint | 144×168 | basalt 64-c · diorite/flint **1-bit** |
| `UI_SCREEN_SMALL_ROUND` | chalk | 180×180 | 64-c |
| `UI_SCREEN_LARGE_RECT` | **emery** | 200×228 | 64-c |
| `UI_SCREEN_LARGE_ROUND` | **gabbro** | 260×260 | 64-c |

### The four safe edit axes

| axis | guard | reaches | touches emery/gabbro? |
|---|---|---|---|
| A1 | `#if defined(UI_SCREEN_SMALL_RECT)` | basalt, diorite, flint | **no** |
| A2 | `#if defined(UI_SCREEN_SMALL_ROUND)` | chalk | **no** |
| A3 | `#if defined(PBL_BW)` | diorite, flint | **no** (both locked units are colour) |
| A4 | new file / new function called only from A1–A3 paths | — | **no** |

`PBL_BW` is the quiet win here: every 1-bit fix is automatically out of reach of the locked pair,
because emery and gabbro are both colour. No screen-class guard needed on those.

### Three hard rules

1. **Never edit an `#else` / `LARGE_*` branch.** If a value must change for a small class, add a
   branch above it; the large branch keeps its *verbatim expression*, character for character.
   (`ui.c:90-111` already models this correctly.)
2. **Never change a shared default.** e.g. do not change `ui_header_height()`'s `return 24;` —
   add `#if defined(UI_SCREEN_SMALL_*)` above it.
3. **Big Mode changes are NOT automatically safe.** `settings_get_big_mode()` is a *runtime*
   branch that runs on emery/gabbro too. Any Big-Mode fix must additionally be wrapped in A1/A2.
   (Defect #71 below is a Big-Mode bug that must be fixed under a `SMALL_RECT` guard only.)

### The proof harness (build before touching anything)

There is no pixel guard in the repo today. Build one first — it is what makes the whole plan
safe to execute:

```
tools/lock_guard.sh
  1. git stash                        (clean tree)
  2. pebble build
  3. for p in emery gabbro:           install + screenshot all 18 states → baseline/$p/
  4. git stash pop                    (your changes)
  5. pebble build
  6. same 18 captures                 → after/$p/
  7. python3 diff: assert 0 differing pixels for every pair; print any offender
```

Two known capture hazards from prior sessions (`memory/emulator-pixel-verify.md`):
the hero icon is animated and the "UPDATED Nm AGO" pill drifts with wall-clock. Freeze both for
the capture run (`AnimationsEnabled=false`, and pin `last_updated` to a fixed value in a
capture-only build flag), or diff with those two rects masked.

Run the harness on **every commit**. A non-zero diff on emery or gabbro = revert, no discussion.

---

# PART 1 — PIXEL-DENSITY / SPACE BUDGET

This is the number that explains ~70% of the defect list.

Pebble's system fonts are **fixed pixel sizes on every platform**. `GOTHIC_18_BOLD` is 18 px tall
on a 260 px gabbro and on a 144 px flint. So the "density" that matters is not PPI — it is
**how many glyphs and rows fit in the content band**, and the small classes were given the
large classes' content at the large classes' font sizes.

### Derived budgets (measured from the code, not guessed)

| | SMALL_RECT | SMALL_ROUND | LARGE_RECT (emery) | LARGE_ROUND (gabbro) |
|---|---|---|---|---|
| resolution | 144×168 | 180×180 | 200×228 | 260×260 |
| `ui_margin_x()` | 12 | 20 | 12 | 20 |
| usable width | **120** | **140** nominal, **~134** at the low chord | **176** | **220** |
| `ui_header_y()` | 8 | 24 | 8 | 24 |
| header ink bottom | 26 | 42 | 26 | 42 |
| status-pill top | 126 | 140 | 186 | 203 |
| **content band height** | **100** | **98** | **160** | **161** |
| content band **area** | 12 000 px² | ~13 700 px² | 28 160 px² | 35 420 px² |
| **area vs emery** | **43 %** | 49 % | 100 % | 126 % |
| **area vs gabbro** | **34 %** | 39 % | 79 % | 100 % |

> **The headline number: the small classes have 34–49 % of the drawing area of the locked pair,
> and are currently rendering the same content at the same font sizes.**
> Content must come out, or type must come down. Both, mostly.

### Character budget per line (the practical "density" rule)

| font | avg advance | SMALL_RECT (120 px) | SMALL_ROUND (~134 px) | LARGE_RECT (176 px) | LARGE_ROUND (220 px) |
|---|---|---|---|---|---|
| `GOTHIC_14` / `_14_BOLD` | ~7 px | **17 ch** | 19 ch | 25 ch | 31 ch |
| `GOTHIC_18_BOLD` | ~9 px | **13 ch** | 14 ch | 19 ch | 24 ch |
| `GOTHIC_24_BOLD` | ~12 px | **10 ch** | 11 ch | 14 ch | 18 ch |
| `LECO_36` "72°" | — | 66 px = **55 % of the line** | — | — | — |
| `LECO_42` "72°" | — | — | 78 px | 78 px | 78 px |

### Row budget in the content band

| row pitch | SMALL_RECT (100) | SMALL_ROUND (98) | LARGE_RECT (160) | LARGE_ROUND (161) |
|---|---|---|---|---|
| 13 px | 7 | **7** | 12 | 12 |
| 15 px | **6** | 6 | 10 | 10 |
| 19 px | 5 | 5 | **8** | 8 |
| 26 px | **3** | 3 | 6 | 6 |
| 28 px | 3 | **3** | 5 | **5** |

**Design rule falling out of this table:** any card whose large-class design needs ≥ 5 rows of
19 px, or ≥ 4 rows of 26 px, *cannot* keep that shape on a small class. It must either drop rows,
drop to the 14 px font, or re-flow to a grid. Golden Hour (4×26) and Settings (10×14) are the two
that break hardest, and they are exactly the two catastrophes in the screenshots.

### Colour-depth axis (orthogonal, and just as damaging)

| | basalt | chalk | **diorite** | **flint** | emery | gabbro |
|---|---|---|---|---|---|---|
| depth | 64-colour | 64-colour | **1-bit** | **1-bit** | 64-c | 64-c |
| `theme.c` has a B&W fallback | n/a | n/a | ✅ yes | ✅ yes | n/a | n/a |
| `icons.c` has a B&W fallback | n/a | n/a | ❌ **none** | ❌ **none** | n/a | n/a |
| `air_quality.c` gauge | n/a | n/a | ❌ **none** | ❌ **none** | n/a | n/a |

`theme.c:80-97` does the right thing — accents collapse to `theme_fg()` on 1-bit.
`icons.c` and `air_quality.c` **bypass that layer entirely** and hardcode `GColorChromeYellow`,
`GColorLightGray`, `GColorVividCerulean`, `GColorIslamicGreen`, `GColorOrange`… On 1-bit the SDK
reduces those by luminance:

- `GColorChromeYellow` → **white** → the sun icon is **invisible on the light theme**
- `GColorLightGray` → **50 % checkerboard dither** → every cloud is a shapeless smudge
- `GColorIslamicGreen` (AQI fill) and `GColorLightGray` (AQI track) → **both dither identically**
  → the AQ gauge conveys nothing

The UV card is the control group: it draws a **solid** fill against a **dithered** track, and it
is the one gauge both 1-bit reviewers marked clean. That's the pattern to copy everywhere.

---

# PART 2 — ROOT CAUSES

Eight systemic causes generate nearly every defect. Fixing these eight kills ~80 of the ~100
findings; the rest are one-offs.

| # | root cause | evidence | blast radius |
|---|---|---|---|
| **R1** | **Untuned metric accessors.** `ui_margin_x()`, `ui_header_y()` still carry `// TODO(5.2): tune; currently == large-*`. The small classes literally inherit the large classes' margins and header offsets. | `ui.c:90-111` | every card |
| **R2** | **Three competing models of where the status pill is.** `ui.c` says `pad_bottom = 18 / 20 / 35`; `week.c:157` says `40 / 22`; `advice.c:911` says `70 / 56`. Nobody agrees, and the pill is drawn **over** content instead of content being clamped **above** it. | `ui.c:138-151`, `week.c:157-159`, `advice.c:911` | 8 cards |
| **R3** | **Horizontal clusters clamp left but never right.** Every list card does `cluster_x = (W - cluster_w)/2; if (cluster_x < floor_x) cluster_x = floor_x;` — and then draws off the right edge when `cluster_w > usable`. There is **no width-driven column-drop policy** on the normal path. | `hours.c:204-206`, `week.c:216-218`, `golden_hour.c:172-174`, `settings.c:56-58` | 4 cards |
| **R4** | **Nothing is chord-aware on round.** The app has no equivalent of the watchface's `face_layout_band_w()`. Full-width rects (pill, page dots, settings rows, hi/lo column, chart labels) run under the chalk bezel. | all round drawing | chalk, every card |
| **R5** | **`cards/settings.c` has ZERO screen-class branches.** It is the only card still on pure `PBL_IF_ROUND_ELSE`, and its row block is sized for a 228/260 px screen. | `settings.c` (5× `PBL_IF_ROUND_ELSE`, 0× `UI_SCREEN_*`) | catastrophic on all 4 |
| **R6** | **`detail_modal.c` bypasses the whole design system.** 14 raw `fonts_get_system_font()` calls, zero `UI_SCREEN_*` branches, and `SHEET_HEIGHT_PCT 80` leaves a 34 px strip of the card behind visible → the "double header". | `detail_modal.c:18, 177, 192, 226…` | both detail sheets, all 4 |
| **R7** | **`icons.c` bypasses `theme.c`'s B&W layer.** Raw `GColor*` constants in `icon_draw_condition()` / `_animated()`. | `icons.c:118-137, 439-462` | diorite, flint — every card with an icon |
| **R8** | **Main card's hero band is over-subscribed.** On SMALL_RECT the location text (y 2–20), the 28 px icon (y 2–30, x 58–86) and the LECO_36 temp (x 12–78) are all assigned the same 30 px band. Three-way collision by construction. | `main_card.c:110-120, 169-209` | main card ×2 themes ×4 platforms |

---

# PART 3 — THE DEFECT REGISTER

Severity: **B** = blocker (data unreadable / screen unusable) · **M** = major · **m** = minor.
Platform: **b**=basalt **c**=chalk **d**=diorite **f**=flint.

## 3.1 — Settings / "Manage Cards" card  (worst screen in the app)

Geometry, SMALL_RECT: `top_y = 8+24+6 = 38`, `row_h = 14`, 10 rows (1 locked + 9 visible; radar
is carved out below 128 KB). Last row box = 164→180. **Screen is 168.**
`label_max_w = 140`, `gap = 10`, `box = 14` → `row_w = 164` on a **144 px** screen.
`hint_y = H-32 = 136` lands *inside* rows 6–7.

Geometry, SMALL_ROUND: `top_y = 24+24+2 = 50`, `row_h = 13`, last row 167→183. **Screen is 180.**
`row_w = 154`, forced to `floor_x = 20` → checkbox column at x 160–174, outside the chord.
`hint_y = H-58 = 122` lands inside rows 5–6.

| # | defect | sev | plat |
|---|---|---|---|
| 1 | Footer hint overprints menu rows — renders as literal garbage: `S0NDOYPCLB0VE UP` ("SUN CYCLE" + "…MOVE UP" on identical pixels) | **B** | b c d f |
| 2 | Page-dot indicator strip drawn on top of the "NIGHT SKY" row | **B** | b c d f |
| 3 | Bottom rows run past the screen — "GOLDEN HR" never renders at all | **B** | b c d f |
| 4 | Checkbox column at x 162–176 on a 144 px screen — **every checkbox is off-screen**, so the card cannot show or confirm any toggle state | **B** | b d f |
| 5 | Checkbox column clipped by the round bezel; lock glyph half-eaten | **B** | c |
| 6 | Rows 6–10 clipped on the left by the chalk chord (`UV INDEX`, `AIR QUAL`, `SUN CYCLE`, `NIGHT SKY`, `GOLDEN HR`) | **B** | c |
| 7 | Last row bottom-clipped mid-glyph | M | b c d f |
| 8 | No scroll affordance — nothing signals rows exist below the fold | m | b c d f |
| 9 | Header reads "CARDS" on rect vs "MANAGE CARDS" on round — inconsistent, and "CARDS" alone doesn't say what the screen does | m | b d f |

## 3.2 — Main card (normal mode)

| # | defect | sev | plat |
|---|---|---|---|
| 10 | Condition icon (x 58–86, y 2–30) drawn on top of the centred location label — "San **Fran**cisco" → "San ▓▓▓cisco" | **B** | b c d f |
| 11 | Icon overlaps the LECO temp ink (icon 58–86 vs temp 12–78 = **20 px overlap**); the `°` of "72°" is eaten | M | b c d f |
| 12 | Hi/lo **up-arrow buried under the icon** — `hilo_x = 80`, arrow centre (80, 18) is inside the icon box. "61°" gets its ↓ but "84°" has no ↑ → the pair reads as unrelated numbers | M | b c d f |
| 13 | Hi value clipped by the chalk chord: `hilo_x = 108`, text right-edge 160, chord at y≈26 is x≤153 → **"84°" renders as "84"** (degree lost) while "61°" keeps its degree — actively misleading | **B** | c |
| 14 | Sun/clear icon **invisible on the 1-bit light theme** (`GColorChromeYellow` → white on white). This is also why the icon "appears" to move between themes | M | d f |
| 15 | Condition icon is a 50 % dither blob with no outline — sun/cloud/rain indistinguishable | M | d f |
| 16 | Icon top ray reaches row 0 in dark mode (agent pixel-scan) — verify whether `icon_draw_sun` rays exceed `size/2`; harmless in light mode only because #14 hides them | M | d f |
| 17 | "12MPH NW" sits ~2 px from the left edge while the right side keeps its margin — asymmetric gutter | m | b c d f |
| 18 | Status pill spills past the round mask: pill 20–160, chord at pill bottom (y 162) is 36–144 → **16 px cut off each side**, ends render flat | M | c |

## 3.3 — Advice / "Touch & Go" card

`body_bot = H - PBL_IF_ROUND_ELSE(70, 56)` — a third, independent guess at the pill position (R2).

| # | defect | sev | plat |
|---|---|---|---|
| 19 | Quip box is **24 px tall** on SMALL_RECT — one `GOTHIC_24_BOLD` line plus a *horizontally sliced* second line, which then disappears behind the pill ("Calm skies / overhe▓▓") | **B** | b d f |
| 20 | Quip box is **2 px tall** on chalk (`quip_top = 108`, `body_bot = 110`). One line renders by luck; the card is structurally broken | **B** | c |
| 21 | `prv_audit_phrases(quip_r)` audits the phrase pool against that 2 px / 24 px box — the content audit is meaningless on both small classes | M | b c d f |
| 22 | Tier badge + headline + quip = 3 stacked rows in a 100 px band; the badge and headline consume 48 px for ~4 words | M | b c d f |
| 23 | Body text is cut with **no ellipsis and no continuation cue** — reads as a rendering fault, not truncation | M | b c d f |

## 3.4 — 6 Hours card

Cluster width, SMALL_RECT worst case (`GOTHIC_14_BOLD`):
`"12 AM" 34 + 4 + icon 14 + 4 + "81°" 24 + 4 + (arrow 12+3+"16" 14) + 4 + (drop 10+3+"0.2\"" 26)`
= **≈156 px into 120 px usable → 36 px of overflow.**

| # | defect | sev | plat |
|---|---|---|---|
| 24 | Precip-amount column clipped at the right edge on 4 of 6 rows — every value renders as `0.` with the digits gone | **B** | b d f |
| 25 | Same overflow eats the wind speed's last digit on the widest rows | M | b d f |
| 26 | Per-hour condition icons are identical dither squares — the icon column carries zero information | M | d f |
| 27 | `°` kerns into the preceding digit at 14 px ("78°" reads "78o") | m | b d f |
| 28 | Asymmetric margins: 12 px left gutter, 0 px right (a direct symptom of R3) | m | b d f |
| 29 | Row icons ride ~4 px high relative to their text baseline (`icon_cy = row_y + 8` vs a 15 px row) | m | b c d f |
| 30 | *(chalk only)* all 5 columns actually fit — chalk's 140 px usable is enough. **Do not apply the SMALL_RECT column drop to chalk.** | — | c |

## 3.5 — Week Ahead card

Cluster width, SMALL_RECT: `26+6+14+6+24+4+5+4+24+6+40` = **≈159 px into 120 → 39 px overflow.**

| # | defect | sev | plat |
|---|---|---|---|
| 31 | Precip-% column clipped — "70%" → `7(`, "65%" → `6!` | **B** | b d f |
| 32 | **SUN and MON have no icon at all** — the clear/sunny glyph is `GColorChromeYellow` → white → invisible on the 1-bit light theme (same root as #14). Rows silently lose their condition | M | d f |
| 33 | Remaining icons are dither blobs | M | d f |
| 34 | `week.c` models the pill as `40 / 22` while `ui.c` uses `18 / 20 / 35` — on chalk it believes the pill top is 118 when it is actually 140, so its "centre the block" math is 22 px wrong | M | c |
| 35 | 5 rows × 16 px in a 100 px band leaves the block visually top-heavy with dead space above the pill | m | b c d f |

## 3.6 — Golden Hour card

`top_y = 42`, `row_h = 26` → rows at 42 / 68 / 94 / **120**; pill top = **126**. Chalk: rows at
62 / 90 / 118 / **146**; pill top = **140**.

| # | defect | sev | plat |
|---|---|---|---|
| 36 | **The 4th row (evening blue hour) is drawn underneath the pill on both small classes.** Only a 6 px sliver of its colour chip is visible. A card whose entire purpose is four times shows three | **B** | b c d f |
| 37 | Blue and gold chips are identical solid black rectangles on 1-bit — the chip column is decorative noise | m | d f |
| 38 | The word column ("BLUE"/"GOLD") is dropped in Big Mode on small classes but kept in normal mode, where it costs ~46 px of the 120 px line | m | b c d f |

## 3.7 — Precipitation card

`total_bars_w = 5×22 + 4×6 = 134` on a 144 px screen → `bar_x0 = 5`.

| # | defect | sev | plat |
|---|---|---|---|
| 39 | Bars span x 5–139, breaking the 12 px margin on both sides; the "+4h" bar and its "55%" label touch the right edge | M | b d f |
| 40 | Bars span x 14–166 against a 20 px margin on chalk; the outer bars sit under the bezel curve | M | c |
| 41 | **46 px of dead space** between the hour labels (~y 114) and the pill (y 140) on chalk — `chart_bot = H-86` is an emery-tuned constant | M | c |
| 42 | ~25 % of the card height is empty between the header and the tallest bar (bars are bottom-anchored, header is top-anchored, nothing fills the middle) | m | b c d f |
| 43 | Header rain icon is an unreadable dither smudge on 1-bit | m | d f |

## 3.8 — UV card

**The one card both 1-bit reviewers passed clean.** Solid fill vs dithered track — this is the
reference pattern for #45.

| # | defect | sev | plat |
|---|---|---|---|
| 44 | "PEAK 9" sits 4 px above the pill; at Big-Mode label size it would collide | m | b c d f |

## 3.9 — Air Quality card

| # | defect | sev | plat |
|---|---|---|---|
| 45 | **Gauge is uniform dither end-to-end — fill and track are indistinguishable.** `aqi_category_color()` returns raw `GColorIslamicGreen`, the track is `theme_muted()` → `GColorLightGray`; both reduce to the same 50 % checkerboard. The gauge conveys no value. (Contrast the UV card, which is correct.) | M | d f |
| 46 | `aqi_category_color()` and the pollen `pcolor` bypass `theme.c` entirely — no B&W path, no Big-Mode path | M | d f |
| 47 | The "42" numeral overlaps the arc inner edge on both small classes (`num_top = c.y - 38` against `radius 42/44`) | m | b c |
| 48 | Header pulse icon reads as a jet/arrow silhouette at 18 px | m | b c d f |

## 3.10 — Sun Cycle card

The healthiest card in the set on all four platforms.

| # | defect | sev | plat |
|---|---|---|---|
| 49 | Sunrise and sunset glyphs are identical apart from a small arrow; on 1-bit the arrow carries 100 % of the meaning | m | d f |

## 3.11 — Night Sky card

| # | defect | sev | plat |
|---|---|---|---|
| 50 | "72% LIT" is bisected by the pill — bottom half of the glyphs is gone | M | b c d f |
| 51 | Moon disc bottom touches the "WAXING" cap-line (no gap) | m | b c d f |
| 52 | Moon phase is ambiguous on 1-bit: `GColorIcterine` (lit) and `GColorOxfordBlue` (shadow) both reduce badly — the graphic reads as a thin crescent while the caption says "72 % LIT" | M | d f |
| 53 | Header moon glyph is a featureless black dot | m | b c d f |

## 3.12 — Detail sheets (6 Hours ▸ Temp Trend, Precipitation ▸ Rainfall, UV ▸ UV Today)

`SHEET_HEIGHT_PCT 80` → SMALL_RECT sheet is 134 px starting at y 34; chalk sheet is 144 px at y 36.

| # | defect | sev | plat |
|---|---|---|---|
| 54 | **Double header.** The 34 px strip of the card behind the sheet shows *its* header, so the screen reads "6 HOURS" then "TEMP TREND". 20 % of a 168 px screen spent on a redundant title | M | b d f |
| 55 | Same strip is **bisected and chord-clipped** on chalk — "6 HOURS" / "UV INDEX" render as sliced half-glyphs above the sheet | **B** | c |
| 56 | Chart value labels collide with the plot: "74" is drawn through the trend line and its first marker (reads "7ч") | M | b c d f |
| 57 | X-axis labels sit on the last pixel rows of the screen, no bottom margin | M | b c d f |
| 58 | X-axis labels **overlap each other** on chalk — 40 px label boxes at 3 positions on a chord-narrowed line → "PM2 AM1 AM2 A" | **B** | c |
| 59 | Rainfall detail's time row truncates to `1…` `1…` `2 …` | M | b d f |
| 60 | ~90 px of dead band between the sheet header and the chart, while the chart is crushed against the bottom | M | b c d f |
| 61 | `detail_modal.c` uses 14 hardcoded `fonts_get_system_font()` calls — the small classes get emery's type ramp with none of emery's room | M | b c d f |
| 62 | "SEL: show rain %" hint renders in `GOTHIC_14` muted grey → dithers illegibly on 1-bit | m | d f |
| 63 | UV detail's x-axis is labelled with **night hours** ("10 PM 12 AM 2 AM") and peaks at midnight — the hourly buffer's labels are reused for the UV series. **Platform-independent; also affects emery/gabbro. Out of scope for this pass — flag only.** | M | all |
| 64 | UV detail curve has no min/max value annotation (temp trend has 74/83) | m | b c d f |

## 3.13 — What's New screen

| # | defect | sev | plat |
|---|---|---|---|
| 65 | Body text runs off the bottom mid-word with **no scroll affordance** — reads as a bug, not as "scroll for more" | M | b c d f |
| 66 | Top ~40 px is blank while the body overflows — the sun + headline + divider block is tuned for a 228/260 px screen | M | b c d f |
| 67 | Body is clipped by the round bottom curve on chalk before it is clipped by the scroll extent | M | c |
| 68 | Stray black fragment at the top-left corner on diorite — investigate (clipped element or scroll-layer artifact) | m | d |

## 3.14 — Shared chrome: status pill + page indicator

| # | defect | sev | plat |
|---|---|---|---|
| 69 | **The pill is drawn *over* content, not reserved *out of* it.** Root cause of #19, #36, #50 and half of #23 | **B** | b c d f |
| 70 | `banner_w = 130` on a 144 px screen = 7 px gutters vs the card's own 12 px margin — the pill is visibly wider than every other element | m | b d f |
| 71 | **Big Mode pill is `130 + 20 = 150 px` on a 144 px screen** → both rounded ends are cut flat by the screen edges | M | b d f |
| 72 | Pill is not chord-clamped on chalk — 16 px spills past the circle on each side at the pill's lower edge (#18) | M | c |
| 73 | Page-dot strip is not chord-clamped: 11 dots = 116 px wide at y 166, where the chord is only 82 px → **~3 dots clipped off each end** | M | c |
| 74 | Inactive dots use `theme_muted()` → `GColorLightGray` → ragged dither diamonds on 1-bit | m | d f |
| 75 | Three files hold three different constants for the same pill position (R2) — any pill change silently desyncs the cards | M | b c d f |

## 3.15 — Big Mode on small classes

| # | defect | sev | plat |
|---|---|---|---|
| 76 | Hi/lo row bottoms sit 0–2 px above the pill; "61°"'s degree ring merges with the pill outline | m | b d f |
| 77 | ~90 px dead band between the Big-Mode icon and the temperature while the bottom is crowded | m | b c d f |
| 78 | Big-Mode hours: row icons float between baselines, appearing to belong to the row above | m | b c d f |
| 79 | Big-Mode arrows abut their digits with no kerning gap ("↑84°" glyphs touch) | m | b c d f |
| 80 | Big-Mode condition icon is a large borderless dither field on 1-bit — the worst instance of #15 | M | d f |

## 3.16 — Cross-cutting 1-bit (diorite + flint)

| # | defect | sev | plat |
|---|---|---|---|
| 81 | `icon_draw_condition()` / `_animated()` hardcode 6 raw `GColor` constants — **no B&W branch anywhere in `icons.c`** | M | d f |
| 82 | Clear/sunny is invisible on the light theme (ChromeYellow → white) | M | d f |
| 83 | All cloud-family icons are borderless 50 % dither → no silhouette | M | d f |
| 84 | Rain/snow/fog are mutually indistinguishable at 14–16 px | M | d f |
| 85 | Golden-hour chips, AQI arc, pollen colours all lose their meaning (#37, #45, #46) | M | d f |
| 86 | Muted greys (`theme_muted`, `theme_secondary` in light mode) dither into illegibility at 14 px | m | d f |
| 95 | **Fog draws nothing at all** — `icon_draw_fog()`/`_animated()` are the only condition icons made of strokes with no fill, and a stroke does not dither: `theme_muted()` quantizes to the *background* in both themes. Found in Phase 5; the 5-macro port alone does NOT fix it (it shares `ICON_CLOUD_COLOR`). Fixed via `ICON_FOG_COLOR` → `theme_secondary()` | **B** | d f |

## 3.17 — Coverage gaps in the screenshot matrix

| # | gap |
|---|---|
| 87 | ~~`refresh_sheet.c` not captured on any small platform~~ — **CAPTURED IN PHASE 0.** Happy path is clean on both basalt and chalk (spinner + one/two-line phrase, no clipping). Partially cleared; the residual real defect is #92 below |
| 88 | No dark-theme captures except `17-dark-main` — dark mode is untested on 15 of 16 card states |
| 89 | No capture of a **maximally-enabled** carousel (all 11 cards on) — the page-dot overflow on chalk gets worse with count |
| 90 | No capture of long location names, 3-digit temperatures, or negative temperatures on the small classes |
| 91 | No capture of the "no data yet" / first-launch state on any small platform |

## 3.18 — Found during Phase 0 (not in the original screenshot matrix)

| # | defect | sev | plat |
|---|---|---|---|
| 92 | **Refresh sheet: the long phrases overflow their 2-line box, and the second line clips on the chalk chord.** `refresh_sheet.c:365-368` computes `text_h = h - (cy + 28) - 8` → **48 px on SMALL_RECT, 54 px on SMALL_ROUND** = room for exactly 2 lines of `GOTHIC_24_BOLD`. At ~10 chars/line on SMALL_RECT, `FALLBACK_PHRASE` ("Couldn't reach the forecast just now...", 39 ch) needs ~4 lines and `"Waiting on the latest forecast..."` (32 ch) needs ~3. Both clip. Separately the box is `x 8 .. w-8` with no chord clamp, so on chalk a second line at y≈145 (chord x 22..158) is cut at both ends. **Only reachable on a slow or failed refresh**, which is why the happy-path captures looked clean. `ui_band_w()` (Phase 1) fixes the chalk half; the box needs the 18 px font or a 3-line allowance on the small classes | M | b c d f |

## 3.19 — Found during Phase 2 (affects the LOCKED pair — baseline deliberately moved)

| # | defect | sev | plat |
|---|---|---|---|
| 93 | **Big Mode's rain pill renders as an empty slab on the light theme.** `ui.c:187-189` hardcodes the rain banner's text to `GColorBlack` because the accent beneath it is normally chrome yellow. Big Mode's high-contrast policy collapses `theme_accent_orange()` to `theme_fg()` (`theme.c:82`), so on the light theme the pill becomes solid black and the black text vanishes into it. `ui_draw_auto_banner` alternates UPDATED/RAIN every 4 s whenever rain is forecast, so the alert disappears for **half of every 8-second cycle**, on every card that draws the pill (which is all of them). Dark mode escapes by luck (`theme_fg()` is white there); normal mode escapes because the accent stays yellow; **diorite/flint escape because the `PBL_BW` branch already uses a correctly inverted pill** — that branch is the pattern the colour path should have copied. **Fixed:** `txt_color = big ? theme_bg() : GColorBlack`, so the text inverts with the pill. | **M** | b c **e g** |

### Why this one was allowed to break the lock

Defect #93 is in shared code and affects **emery and gabbro identically** — it is not a small-screen
defect. The three options were: fix it and re-baseline, scope it to the small classes (knowingly
leaving the locked pair broken), or defer it like #63. **The user chose to fix it properly.** The
reasoning: it is a legibility failure in the *accessibility* mode, so the users most likely to hit
it are the ones least able to tolerate it, and protecting a byte comparison at the cost of
knowingly shipping that inverts the purpose of the lock.

`tools/lock_baseline.txt` was therefore regenerated with `--save` on 2026-07-31. **This is the only
sanctioned baseline move to date.** Say so in the commit message.

The pre-fix binaries independently confirmed the blast radius, which is worth keeping as evidence:
after the fix, **basalt and chalk moved, and diorite and flint were byte-identical** — exactly the
four-colour-platform footprint the diagnosis predicted, since the 1-bit pair compiles the untouched
`PBL_BW` branch.

New baseline:

```
543e34054adfd226c88c43a920494c0bdaf120422c437aea298238139153126f  emery
7f76d42c1d3677d8cf035517bedbefb676b053c153b00ee062506ed1e44332ad  gabbro
```

## 3.20 — Found during Phase 3 (latent bug in the Phase-1 helper)

| # | defect | sev | plat |
|---|---|---|---|
| 94 | **`ui_band_w()` measured its chord against the caller's rect instead of the screen.** The round branch derived the circle from the rect handed to it (`r = bounds.size.w/2`, `screen_cy = bounds.origin.y + bounds.size.h/2`), which is only correct when that rect *is* the screen — but `ui_layout_solve()` passes its `avail` band. Phase 2 never saw it: the settings card's band (y 50–136) happens to centre within 3 px of the glass. The main card's band runs y 0–136, centre 68 against the true 90, so its top row at cy 19 was measured at dy 49 instead of dy 71 and came back **40 px too wide** (134 vs 94) — which would have put the new location row straight back under the chalk bezel, recreating the exact defect class Phase 1 exists to close. **Fixed:** the round branch now uses `PBL_DISPLAY_WIDTH/2` and `PBL_DISPLAY_HEIGHT/2`, so `y`/`h` are screen-absolute and a caller may pass any sub-band. Verified on the live card — the location row solves to a 94 px box at y 15, matching the chord by hand-calculation — and the settings card re-screenshotted unchanged. | **M** | c |

## 3.21 — Found during Phase 6 (the two detail sheets §3.12 never reviewed)

§3.12 was written from captures of the Temp Trend, Rainfall and UV sheets only. The Week and Air
Quality sheets are reachable the same way (long-press SELECT) and were never shot, so the two
defects below — one of them a blocker that costs the sheet its entire payload — sat unrecorded
through five phases. **Lesson: a screenshot matrix's coverage gaps are defects in the matrix, not
absences of defects.** #89–#91 are the same shape and are still open.

| # | defect | sev | plat |
|---|---|---|---|
| 96 | **Air Quality sheet drops the NO2 row and the caption off the bottom.** 4 rows × `row_h 24` from `top + 26` needs 122 px below the title; a SMALL_RECT sheet had 96. NO2 and the pollen/units line simply never rendered — a quarter of the sheet's data, silently absent on every small platform since it shipped. On chalk O3 is lost too, so half the breakdown is gone. **Fixed:** the pitch is now derived from the room that actually exists (`avail / 4`, clamped 15..24) instead of asserted, so it also survives a taller caption | **M** b d f · **B** c | b c d f |
| 97 | **Week sheet: the day's high and low — its entire reason to exist — are clipped by the sheet bottom and overprinted by the page dots, and the POP row never renders at all.** The large classes' 42 px icon + 32 px numeral row need ~155 px below the title against ~110 available, so `if (pop_y + 18 <= content_bottom)` silently failed and the LECO_28 numerals ran under the dot strip. On chalk only orange slivers of "78°/58°" survived. The POP row is the one reading the Week CARD does not already give you. **Fixed:** full-screen, a smaller icon (34 rect / 26 round), tightened gaps, and a `content_bottom` measured off the dot row rather than off a fixed 20 px strip reserved for 3 px dots | **B** | b c d f |
| 98 | **Every muted stroke and glyph in `detail_modal.c` is invisible on 1-bit** — all three chart baselines, the "SEL: show rain %" hint, the Rainfall hour labels, the Week HIGH/LOW captions and its inactive page dots, the AQ units caption. `ui_draw_dotted_hline()` draws with `graphics_draw_pixel`, which is a stroke and cannot dither, and `theme_muted()` is LightGray on light / DarkGray on dark — the background in **both** themes. Identical root cause to #95 (fog), found by the same reasoning rather than by another lucky screenshot. **Fixed:** a `DM_MUTED_INK` macro (→ `theme_secondary()` under `PBL_BW`) applied to every muted **text/stroke** site; the two **fill** sites (grab notch, pollutant track) keep `theme_muted()` deliberately, because a fill *does* dither and stays visible | **M** | d f |
| 99 | **`detail_modal.c` keeps its own copy of `aqi_category_color()` and Phase 5 fixed only the card.** The local `prv_aqi_color()` returns raw `GColorIslamicGreen` / `GColorOrange` / `GColorRed` / `GColorPurple`, which dither to roughly the same 50 % check as the `theme_muted()` track behind them — the exact #45/#46 failure, surviving in the sheet at every AQI **outside** 51..100 (that band escapes only because `theme_accent_orange()` already has a 1-bit fallback, which is why the emulator's AQI 52 looked fine). **Fixed:** `theme_fg()` under `PBL_BW`. **Generalise: when a defect is fixed in a helper that was deliberately duplicated "to keep the modules decoupled", grep for the twin.** | **M** | d f |

---

# PART 4 — PER-UNIT DESIGN CALLS

You asked me to make the calls. Here they are, with the reasoning and the fallback.

## Call 1 — Manage Cards moves to the phone on all four small units  ✅ *(your proposal, and it's right)*

**Do:** On `SMALL_RECT` and `SMALL_ROUND`, the Settings card stops being a 10-row editor and
becomes a **4-row status card**:

```
        ⚙ SETTINGS
   THEME        LIGHT
   BIG MODE     OFF
   CARDS        IN APP
   ────────────────────
       v2.0.0
```

- Rows: 4 × 19 px = 76 px, fits the 100 px band with room to spare, on both small classes.
- The on-watch cursor / toggle / reorder input is compiled out under the same guard
  (5 call sites in `TouchWeather.c`: lines 176, 217-218, 287, 299).
- **Force `PhoneManagesCards = true`** at load on these platforms so the Clay `CardEnabled*` and
  `CardOrder` keys actually apply — otherwise the feature moves to the phone and then the phone's
  writes get ignored.
- SELECT still toggles theme; the card is still the escape hatch. `HideSettingsCard` still works.

**Why this is the right call:** 10 rows × 14 px + a locked row + a rotating hint + the page dots
need ~170 px of band. SMALL_RECT has 100. There is no font that fixes a 70 % overage. Clay already
owns `CardEnabledHours…CardEnabledAdvice`, `CardOrder` and `HideSettingsCard` — **the phone-side
feature already exists and is already wired.** This deletes the app's worst screen and loses
nothing that isn't already reachable.

**Emery + gabbro keep the full on-watch editor, untouched.**

*Fallback if you'd rather keep it on-watch:* paginate — 5 rows/page, UP/DOWN long-press pages,
the hint moves into the header. More code, more input surface, and chalk's chord still clips the
checkbox column, so it needs #4/#5 fixed regardless. I don't recommend it.

## Call 2 — Golden Hour becomes a 2×2 grid on small classes

The Big-Mode path **already draws exactly this** (`golden_hour.c:56-64`): rows = BLUE/GOLD,
columns = AM/PM, `showword = 0` on small classes. 2 rows × 26 px = 52 px, fits comfortably.

**Do:** promote that grid to be the *normal* small-class layout. All four times stay visible,
zero rows hide under the pill, and the code already exists and is already screen-class-branched.
Cheapest high-value fix in the plan.

## Call 3 — 6 Hours drops the precip-**amount** column on SMALL_RECT only

36 px of overflow. Something must go. Options ranked:

1. **Drop precip amount, keep wind** ← recommended. Amounts in mm/inches are the least glanceable
   number on the card, and the *chance* of rain is already the whole Precipitation card. Saves 43 px
   — enough with margin to spare.
2. Drop wind, keep precip — loses more (wind direction has no other home on the small units).
3. Alternate columns per row — unreadable.

**Chalk keeps all five columns** (measured: 140 px usable, cluster fits). Do not touch it.

Implement as a **policy, not a constant**: measure `cluster_w`, and if it exceeds
`W - 2×margin`, drop the lowest-priority column and re-measure. That makes it robust to long
labels, 3-digit temps, and metric/imperial switches, and it fixes #25 for free.

## Call 4 — Week Ahead drops the precip-% column on SMALL_RECT only

Same 39 px overflow, same policy, same reasoning. Chalk fits — leave it.

## Call 5 — Advice drops to the 18 px font and takes the whole band

- Quip font: `ui_font_body()` (24 px) → `ui_font_header()` (18 px) on both small classes.
- Merge the tier badge into the header row (icon + "TOUCH & GO · NICE") — reclaims 22 px.
- `body_bot` = the shared pill-top accessor from Call 8, not a per-file guess.
- Result: ~62 px of quip band on SMALL_RECT = **3 full lines at 18 px** vs today's 1¼ at 24 px.
- Re-run `prv_audit_phrases()` against the real box and shorten any phrase that still overflows.

## Call 6 — Main card: move the icon out of the hero band

The one card that needs a genuine redesign rather than a constant. Proposed small-class layout:

```
SMALL_RECT (144×168)                 y
┌──────────────────────────┐
│ San Francisco        ☀   │   0–18   location left @14px · icon 24px top-right
│  72°            ↑84°     │  20–58   LECO_36 left · hi/lo stacked right
│                 ↓61°     │
│      FEELS 75°           │  62–82   18px
│   ≋ 12MPH NW │ 💧 58%    │  86–112  14px, split row
│  ▓▓ UPDATED NOW ▓▓       │ 126–148  pill
│   • • ▬ • • • • •        │ 154      dots
└──────────────────────────┘
```

- Kills #10, #11, #12 in one move: no element shares a band with another.
- Icon at top-right, 24 px, clear of both the location text and the temp ink.
- Hi/lo stacks vertically in the right column instead of competing with the icon for the same x.
- **Chalk:** same structure, but every x extent must run through the new `ui_band_w()` chord
  helper (Call 7) so #13 (the lost degree on "84°") cannot recur.
- This one needs 2–3 screenshot iterations. Budget for it.

## Call 7 — Port `face_layout_band_w()` from the watchface as `ui_band_w()`

Your watchface already solved chalk. `face_layout.h:54`:

> *"Usable width of a horizontal band spanning y..y+h. Full width minus margins on the rect
> classes; the inscribed chord at the band's worst edge on the round ones, so text and pills
> ellipsize instead of running under the bezel."*

That is precisely R4. Port it into `ui.c` as `int ui_band_w(GRect bounds, int y, int h)` and route
the chalk paths through it. **One helper fixes #5, #6, #13, #18, #40, #55, #58, #72, #73** —
nine defects, including two blockers.

Rect classes return `W - 2×margin` unchanged, so `SMALL_RECT` is unaffected and the large
branches are never called. Zero risk to the lock.

## Call 8 — One source of truth for the status pill

Add to `ui.h`:

```c
int ui_status_pill_top(GRect bounds);   // topmost y the pill occupies
int ui_content_bottom(GRect bounds);    // = pill_top - 4  ← cards clamp to THIS
```

Then delete the three private models: `week.c:157-159`, `advice.c:911`, and every card's
hand-rolled bottom padding. Fixes #19, #20, #34, #36, #50, #69, #75 and prevents the entire class
from ever recurring. Large-class values must come out **numerically identical** — that's what the
harness proves.

While in there: `banner_w = min(banner_w, ui_band_w(...))` fixes #70, #71 and #72.

## Call 9 — Detail sheets go full-screen on small classes

- `SHEET_HEIGHT_PCT` → **100** on `SMALL_RECT` and `SMALL_ROUND`. Kills the double header (#54)
  and the chalk half-glyph strip (#55), and reclaims 34 px for the chart.
- Route the 14 raw font calls through `ui_font_*` with small-class branches (#61).
- Reserve 16 px below `chart_bottom` for the x-axis labels (#57).
- Small classes label **first / middle / last** x positions only, not all six (#58, #59).
- Draw the min/max value labels **offset from the plot line**, or draw a 1 px background knockout
  behind them (#56).

## Call 10 — `icons.c` gets a 1-bit path (diorite + flint)

Add `#if defined(PBL_BW)` branches to `icon_draw_condition()` / `_animated()` that select by
**silhouette**, not hue — because hue is gone:

| condition | 1-bit rendering |
|---|---|
| clear | **solid** filled disc + rays (never white — always `theme_fg()`) |
| partly cloudy | filled disc + **outlined** cloud (2 px stroke, hollow) |
| cloudy | **outlined** cloud, hollow |
| rain | outlined cloud + 3 **solid** drop strokes |
| snow | outlined cloud + solid asterisk |
| storm | outlined cloud + **solid filled** bolt |
| fog | 3 solid horizontal bars |

Fill vs outline vs stroke-count is the only vocabulary a 1-bit panel has. Fixes #14, #15, #26,
#32, #33, #43, #80, #81–#84.

Same treatment for the AQ gauge (#45): **solid fg fill on a hollow 1 px outlined track** — copy
the UV card, which already reads correctly on both 1-bit units.

`PBL_BW` is false on emery and gabbro, so none of this can reach them.

## Call 11 — What's New shrinks its header and gains a scroll cue

- Small classes: sun 24→**18 px**, drop the dotted divider rule, tighten the version line.
  Reclaims ~28 px (#66).
- Add a small `▾` chevron at the bottom edge while more content exists (#65).
- Clamp the body's right edge through `ui_band_w()` on chalk (#67).

## Call 12 — Explicitly out of scope this pass

- **#63** (UV detail plotting night hours) — a data bug, affects emery/gabbro equally. File it
  separately; fixing it here would break the lock.
- **#88–#91** (dark mode, max-carousel, edge-case data, first-launch) — capture first, then triage.

---

# PART 5 — EXECUTION ORDER

Each phase ends with `tools/lock_guard.sh` green and a fresh 4-platform capture.

| phase | work | defects closed | risk |
|---|---|---|---|
| **0** | Build `tools/lock_guard.sh`; capture emery+gabbro baselines; capture the 5 missing states (#87–#91) | — | none |
| **1** | `ui_band_w()` + `ui_status_pill_top()` / `ui_content_bottom()`; migrate `week.c` / `advice.c` off their private pill constants | #5 #6 #13 #18 #34 #40 #55 #58 #69 #70 #71 #72 #73 #75 | low — pure addition, large branches numerically unchanged |
| **2** | Settings card → 4-row status card; compile out on-watch management; force `PhoneManagesCards` | #1 #2 #3 #4 #7 #8 #9 | low — new code path, old path preserved for large |
| **3** | Golden Hour 2×2 grid; Advice re-flow; Week/Hours column-drop policy | #19 #20 #21 #22 #23 #24 #25 #28 #31 #35 #36 #38 | medium — needs screenshot iteration |
| **4** | `icons.c` + AQ gauge 1-bit paths | #14 #15 #26 #32 #33 #37 #43 #45 #46 #52 #80 #81–#86 | low — `PBL_BW` guard cannot reach the lock |
| **5** | Main card small-class redesign | #10 #11 #12 #16 #17 | **highest** — budget 2–3 iterations |
| **6** | Detail sheets full-screen + font ramp + axis fixes | #54 #56 #57 #59 #60 #61 #62 #64 | medium |
| **7** | What's New; Precipitation spacing; Night Sky; Big-Mode polish | #39 #41 #42 #44 #47 #48 #49 #50 #51 #53 #65 #66 #67 #68 #76–#79 | low |

**Totals:** 91 numbered defects · 14 blockers · 34 major · 43 minor.
Blockers concentrate in phases 1–3, so the app is shippable on all four units after phase 3;
phases 4–7 are quality.

---

# PART 6 — DECISIONS I NEED FROM YOU

1. **Settings card** — confirm the 4-row status card (Call 1), or do you want the paginated
   on-watch editor instead?
2. **6 Hours / Week column drop** — confirm precip is the column that goes on SMALL_RECT, and
   that chalk keeps all five.
3. **Main card** (Call 6) — is the proposed 4-band layout the direction, or do you have a
   different arrangement in mind? This is the one that needs your eye before I build it.
4. **Scope of phase 4** — the 1-bit icon rework changes how diorite and flint *look*, not just
   where things sit. Confirm you want the visual language changed there.

---
---

# PART 7 — LOCKED DECISIONS
### *(supersedes Part 4 wherever the two differ; Part 4 is kept as the audit trail of what was originally recommended vs. what was chosen)*

## D1 — Settings card → 4-row status card ✅ *(as Call 1)*

Unchanged from Call 1. On both small classes the Settings card becomes read-only status
(THEME / BIG MODE / CARDS: IN APP / version), on-watch cursor+toggle+reorder is compiled out at
the 5 call sites in `TouchWeather.c`, and `PhoneManagesCards` is forced on so Clay's writes apply.
Emery and gabbro keep the full editor.

## D2 — Hours drops **wind**, keeps precip  ⚠️ *(reverses Call 3 — user's call)*

On `SMALL_RECT` the 6 Hours card drops the **wind** column (compass arrow + speed) and **keeps**
the precip-amount column. Chalk keeps all five columns.

**Honest arithmetic — this is marginal and the implementation must account for it.**
Measured worst-case cluster on `SMALL_RECT` at `GOTHIC_14_BOLD`:

| column | width |
|---|---|
| time `"12 AM"` | 34 |
| icon | 14 |
| temp `"81°"` | 24 |
| wind (arrow 12 + gap 3 + `"16"` 14) | 29 |
| precip (drop 10 + gap 3 + `"0.2\""` 26) | 39 |
| 4 × inter-column gap @ 4 | 16 |
| **total** | **156** into **120 usable** |

- Dropping **precip** (−43) → **113**. Fits with 7 px headroom. *(the Call 3 recommendation)*
- Dropping **wind** (−33) → **123**. **Still ~3 px over.** *(the chosen option)*

The chosen option does not fit on its own at the widest data. Implement as a **measured cascade**,
never a hardcoded column set — that respects the decision and still cannot clip:

1. Measure the real `cluster_w` (the card already does this).
2. If `cluster_w > W − 2×margin` → drop **wind**. Re-measure.
3. If still over → tighten inter-column gap 4 → 3 and drop icon 14 → 13 (−9 px). Re-measure.
4. If still over → shorten the precip format (`0.2"` → `.2"`). Re-measure.
5. Only if still over → drop precip, as the last resort.

Steps 3–4 recover ~9 px, clearing the 3 px gap with margin at every data width measured, so precip
should survive in practice. Step 5 exists so the card can never clip, not because it is expected
to fire.

## D3 — Week Ahead: **needs its own call — D2 does not map**

Week has **no wind column**. Its columns are day / icon / low / `/` / high / pop%. The overflowing
column *is* the pop%, so "keep precip" and "stop the clipping" are in direct conflict here.

| Week column set | width | vs 120 usable |
|---|---|---|
| all columns (today) | 159 | **+39 over** |
| drop pop% | 113 | fits, +7 headroom |
| keep pop%, drop the condition icon | 139 | **+19 over** |
| keep pop%, drop icon, drop the `°` glyphs (`57/73`) | ~125 | **+5 over** |
| keep pop%, drop icon, drop `°` **and** the `/` separator | ~120 | exactly at the limit, zero headroom |

**Blocked on your answer.** Recommendation stands: drop pop% on `SMALL_RECT` (chalk fits
everything — leave it). Rain probability already has a dedicated card two swipes away; the
condition icon is the only at-a-glance signal in a Week row and is worth more per pixel.

## D4 — Main card: **full port of the watchface layout system** ✅

Port `face_layout.c` → `src/c/ui_layout.c`, both functions, per-class frame retuned for cards:

- `ui_band_w(bounds, y, h)` — chord at the row's **vertical center** (not the far edge), integer
  Newton sqrt (no soft-FP), floored at 48 px so pill text rects can never go negative.
- `ui_layout_solve(rows, avail)` — measure → stack with elastic `GAP_MIN..GAP_MAX` → center the
  slack. Returns `fits`; top-aligns at min gaps on overflow so nothing clips at the top.

The small-class main card becomes a flow of rows using the watchface's weather-row idiom —
`[icon] [temp] [↑hi over ↓lo]`, three columns on **one midline**, measured as a cluster and centered:

```
   San Francisco          <- D5: own row, ellipsized to ui_band_w, optional
   FEELS 75°
   [icon]  72°   ^ 84°    <- weather row: 3 columns, 1 midline
                 v 61°
   ~ 12MPH NW | 58%
   ## UPDATED NOW ##
    . . -- . . . .
```

Kills #10, #11, #12 structurally — nothing shares a band, so nothing can overlap. Also adopt the
watchface's `FZ_TEXT_RISE` idiom: reserve each row's **visible ink height**, not the font's layout
box, since the box's dead top-side leading is what inflates every gap.

`ui_band_w` then gives Hours / Week / Golden Hour / Settings the right-edge clamp they have never
had — **closing R3 and R4 with one helper** — and retires `main_card.c:147-148`'s `block_shift`
hack, the exact anti-pattern `face_layout.h`'s header comment was written about.

`ui.h` differs between the two repos by 5 lines, all comment, so the port is clean: same class
macros, same idioms, same `theme_*` API.

## D5 — Location gets its own top row ✅

14 px row at the top of the flow, clamped to `ui_band_w()` so long names ellipsize instead of
running under the chalk bezel. Because the solver reflows on `present = false`, turning it off
(Clay "Show location", default OFF) recenters everything below with **no hole left behind** —
precisely what `block_shift` was hand-rolling badly.

## D6 — 1-bit: port the watchface's colour remap + fix the gauges ✅ *(replaces Call 10)*

Copy the `PBL_BW` block from the watchface's `icons.c` verbatim — **not** the shape rework Call 10
proposed. The watchface's own comment: *"The upstream app never verified condition icons on B&W."*

```c
#if defined(PBL_BW)
#define ICON_SUN_COLOR   (theme_fg())      // was ChromeYellow -> white -> invisible
#define ICON_CLOUD_COLOR (theme_muted())   // dithers, but visible
#define ICON_DROP_COLOR  (theme_fg())
#define ICON_BOLT_COLOR  (theme_fg())
#define ICON_STORM_CLOUD (theme_muted())
#else
   ...verbatim current constants...
#endif
```

Plus the two rules the watchface learned the hard way:

1. **`theme_muted()` is fine for fills, fatal for strokes on 1-bit.** It dithers when filled
   (visible) but quantizes to the *background* in both themes when stroked at 1 px
   (LightGray→white on light, DarkGray→black on dark) and the element vanishes. Use
   `theme_secondary()` for strokes. (`clock_zone.c:425`)
2. **The status pill on 1-bit is a solid inverted pill** (fg fill, bg text). The app already does
   this correctly at `ui.c:153-158` — no change needed, just don't regress it.

AQ gauge (#45/#46): **solid `theme_fg()` fill on a `theme_muted()` track** under `PBL_BW` — the
same solid-on-dither vocabulary the UV card already uses and that both 1-bit reviewers passed
clean. Route `aqi_category_color()` and the pollen colours through a theme accessor so they stop
bypassing `theme.c`.

**Explicitly NOT in scope (D6b):** the 14 px icon column in Hours/Week. The watchface's icon is
~40 px, so it never had to solve legibility at 14 px — at that size a dithered cloud is a smudge
whatever colour it is. The remap fixes every *invisible* icon (#14, #32, #82); the
*indistinguishable* tiny ones (#26, #33, #84) stay open as a separate, later call.

## D7 — Revised execution order

| phase | work | risk |
|---|---|---|
| **0** ✅ | `tools/lock_guard.sh` + emery/gabbro baselines + capture the 5 missing states (#87–#91) | none — additive tooling only |
| **1** ✅ | Port `ui_layout.c` (`ui_band_w` + `ui_layout_solve`); add `ui_status_pill_top()` / `ui_content_bottom()`; migrate `week.c` + `advice.c` off their private pill constants | low — pure addition; large branches numerically unchanged |
| **2** ✅ | Settings → 4-row status card (D1); compile out on-watch management; force `PhoneManagesCards` | low |
| **3** ✅ | Main card → flow layout + watchface weather row (D4, D5) | medium — the payoff phase; budget 2–3 screenshot iterations |
| **4** ✅ | Golden Hour 2×2 grid; Advice re-flow at 18 px; Hours measured cascade (D2); Week per D3 | medium |
| **5** ✅ | 1-bit remap + AQ/pollen gauge fix (D6) — **#37 closed early, in Phase 4** | low — `PBL_BW` cannot reach the lock |
| **6** ✅ | Detail sheets full-screen + font ramp + axis fixes | medium |
| **7** | What's New; Precipitation spacing; Night Sky; Big-Mode polish | low |

Phase 1 moved ahead of the card work because every later phase consumes `ui_band_w()` and
`ui_content_bottom()`. `tools/lock_guard.sh` must come back green on emery **and** gabbro at the
end of every phase; a non-zero pixel diff means revert.

---

## D8 — The endgame: D7's Phase 7 splits into four *(supersedes the D7 table's row 7)*

After Phase 6 the register held **37 open defects, and D7's single "Phase 7" row covered only
about 21 of them.** The remainder had never been allocated to any phase at all:

* the **shared-chrome group #69–#75**, which Phase 1 was supposed to close and demonstrably did
  not — "Phase 3 as built" records #18 and #73 still open, and `radar.c` still holds private pill
  constants against #75;
* the **D6b group** (#26/#33/#84), deferred out of Phase 5 by design;
* **#92** (refresh-sheet phrase overflow), found in Phase 0 and never scheduled;
* four polish minors (#27, #29, #35, #86).

Two calls were taken before starting. **D6b is in scope** — it is the last M-severity 1-bit group,
and folding #80 (the Big-Mode hero icon) into the same `icons.c` pass means the file's 1-bit
vocabulary is designed once rather than twice. **Four phases, one commit each**, matching the
Phase 0–6 granularity: each has a distinct verification matrix, and a single 23-defect commit
spanning seven files would not be reviewable.

**Shared chrome goes first, and that ordering is load-bearing.** Night Sky's #50 ("72% LIT"
bisected by the pill) is a *symptom* of #69, not an independent defect; the What's New and
Precipitation work both sit against the same pill and dot geometry. Fixing the chrome first means
those screens are laid out against final geometry instead of being solved twice.

| phase | work | defects | risk |
|---|---|---|---|
| **7** ✅ | Shared chrome: pill chord-clamp + width, page-dot chord window, 1-bit dots, `radar.c` migration | #18 #69 #70 #71 #72 #73 #74 #75 | low code volume, **highest lock exposure** — all three files are compiled by the locked pair |
| **8** ✅ | Screen sweep: What's New, Precipitation, Night Sky, refresh sheet, and the #44/#47/#48/#49 singles | #39 #41 #42 #43 #44 #47 #48 #49 #50 #51 #52 #53 #65 #66 #67 #68 #86 #92 | medium — four independent screens, three of which no phase has entered |
| **9** ✅ | Big Mode on the small classes | #76 #77 #78 #79 | medium — a **runtime** branch that also runs on emery/gabbro, so every fix needs a compile-time small-class guard too (Part 0 rule 3) |
| **10** ✅ | 1-bit iconography (D6b) + the polish tail | #26 #27 #29 #33 #35 #80 #84 | low — `PBL_BW` cannot reach the lock; iteration-heavy, not risk-heavy |

**All four phases are complete** (`dd14e58`, `efeb525`, `0635485`, `d2076a5`), and the small-screen
remediation is closed: every numbered defect in the register is either fixed, or measured and
recorded as not reproducing, or explicitly out of scope. One defect was added along the way (#100,
the Big-Mode hero icon at row 0) and closed in the same phase that found it.

**What remains, by decision rather than omission:**

* **#63** — UV plotted against the temp buffer's night hours. Platform-independent, so fixing it
  moves emery and gabbro. Needs its own decision like #93's, and should be filed separately.
* **#90** — no captures of long location names, 3-digit or negative temperatures on the small
  classes. The cascades that would handle them (Hours, Week, the pill label) are measured policies
  rather than constants, so they should hold; that is a prediction, not a verification.
* **#27** — cosmetic, and font-intrinsic. Documented in "Phase 10 as built".
* **The v2.0.0 release prep** (`package.json`, `README.md`, `CHANGELOG.md`, `screenshots/v2.0/`) is
  still uncommitted, and **`screenshots/v2.0/` is now stale** — the pill, the page dots, What's New,
  Precipitation, Night Sky and every 1-bit condition icon have changed appearance since it was
  captured. Re-shoot before release.

Out of scope at the end of Phase 10, by decision rather than omission: **#63** (UV plotted against
the temp buffer's night hours — platform-independent, would move the locked pair; file separately)
and **#90** (edge-case data captures).

---

## Phase 3 as built

`main_card.c`'s normal-mode path is now split: the two small classes run a four-row
`ui_layout_solve()` stack (location / FEELS / weather / wind), and the large classes keep the
pre-Phase-5 anchor code verbatim in the `#else`. The Big Mode path is untouched (Phase 7).

**Closed:** #10, #11 and #12 *structurally* — no two elements share a band, so they cannot
overlap regardless of data. #13 on chalk (verified: "84°" keeps its degree). #17 (the wind row is
a measured, centred cluster, so its gutters are symmetric by construction). `block_shift` is
retired — the solver reflows natively when the location row is absent, which is the whole point
of D5. #94 above was found and fixed on the way.

**Still open on this card:** #14/#15/#16 (1-bit icon legibility — Phase 5 / D6), #18 and #73
(the status pill and page dots are still not chord-clamped — shared chrome in `ui.c`/`nav.c`).

Three calls made during implementation that the plan did not specify:

1. **Weather-row temp is `ui_font_title()` (GOTHIC_28_BOLD), not LECO.** "72°" is 66 px of
   LECO_36 — over half the 120 px line — and no icon and hi/lo column fit beside it. This is the
   tier the watchface settled on for the same row on the same hardware, and being a full font it
   carries real degree and minus glyphs. *(User decision.)*
2. **FEELS dropped from 24B to 18B.** At 24B against a 28B hero temp the two readings carry the
   same visual weight, and because D4 puts FEELS *above* the temp the eye landed on the
   feels-like number first. 18B makes it read as the secondary value it is and gives the card a
   clean 28 / 18 / 14 ramp.
3. **SMALL_RECT drops the wind swoosh but keeps the droplet.** 120 px at 14B holds one glyph, not
   two. Letting the measurement pick per frame made both glyphs blink in and out as the readings
   changed — the pass/fail margin was literally 0 px — so the swoosh is dropped at compile time
   ("MPH"/"KMH" already labels that reading) and the droplet is kept, because it is what stops
   "58%" reading as a rain chance and, in dew-point mode, "58°" reading as a temperature. Chalk's
   ~156 px line keeps both. A measured backstop still drops the rest at the widest data.

Verified on basalt, chalk and diorite, with the location row forced on for a 4-row capture
(live PKJS name "Haina (Kloster)") and then reverted. `lock_guard.py` green on emery and gabbro
at the final clean build.

---

## Phase 4 as built

The four remaining content cards, `SMALL_RECT` + `SMALL_ROUND` only. Every step was clean-built
and `lock_guard.py`-verified on its own before the next one started; emery and gabbro are
byte-identical to the Phase-3 baseline at the final build. **Nothing is committed.**

**Closed:** #19 #20 #21 #22 #23 (Advice) · #24 #25 #28 (6 Hours) · #31 (Week) · #34 (the
Phase-1 carry-over) · #36 #37 #38 (Golden Hour). #35 is partly addressed as a side effect —
routing Week through the real pill geometry moved its row block down ~11 px into the dead space
on chalk.

### The APP_LOG line convention — settled empirically

The trap is real and now measured. `advice.c`'s two `APP_LOG`s were hand-pinned first, alone,
and the first attempt pinned them to the **closing-paren** line (595 / 622) on the theory that
GCC reports the last line of a multi-line macro invocation. **The guard tripped.** Re-pinned to
the **macro-name** line (593 / 620) it came back green *and* all four small-platform digests
matched the pristine tree byte for byte — which is the real proof, since it shows the pinned
literal reproduces what `__LINE__` was emitting rather than merely being self-consistent.

**For future sessions: this toolchain (arm-none-eabi-gcc 14.2.1) expands `__LINE__` to the line
of the macro NAME, not the closing parenthesis.** Pin to the `APP_LOG(` line.

### Golden Hour — 2×2 grid, and #37 pulled forward (user-approved)

The normal-mode 4-row list is wrapped in a small-class branch that promotes the Big-Mode 2×2
AM/PM grid; the large classes keep the list verbatim in the `#else`. The shared
`ui_draw_auto_banner()` call moved below the `#endif` so it is written once — the emitted token
sequence for the large classes is unchanged, which the guard confirms.

#37 was pulled forward from Phase 5/D6 so the card is only touched once, per the user's
approval. Band identity is now carried by **shape as well as hue**: filled chip = blue hour,
2 px outlined chip = golden hour. Colour platforms keep the blue/orange accents on top of the
shape. Verified on diorite: the filled and hollow chips are unambiguous where two solid black
rectangles previously carried nothing.

### Advice — Option A, and a font call the plan did not specify

Part 4's Call 5 said to merge the tier badge into the **card header**. That does not fit and was
not built: `"TOUCH & GO · RAIN SOON"` measures ~185–200 px at 18B against 120 px usable, and the
header alone is already ~123 px. The user chose **Option A**: the badge merges into the
**headline** row instead — the 16 px tier icon prepends the accent-coloured data headline, which
already states the same trigger. The tier *name* is dropped; icon + accent + headline carry it.

Two calls made during implementation:

1. **The headline is `ui_font_label()` (14B), not 18B.** At 18B the merged row does not fit
   either — `"DATA MAY BE STALE"` is ~153 px against the ~100 px left beside the icon, so the
   card's own data would have ellipsized. 14B is also the font the badge already used, so the
   row reads as the merged badge it is, and it gives the card a real hierarchy: the 18B quip is
   the content, the 14B headline is the supporting "why". Four headlines still needed shorter
   small-class wordings (`RAIN IN %dM`, `COLD RAIN %d°`, `AQI %d POOR`, `DATA IS STALE`) plus
   `FEELS %d°`, which matches the main card's own label and leaves room for a 3-digit reading.
2. **28 phrases got small-class variants, via a `PHRASE(full, small)` macro.** The re-flow gives
   the quip ~68 px = three lines of 18B, and the first audit run flagged 31 phrase-slots (28
   unique) that wrapped to four and clipped. Rather than shrink the type again, those phrases
   were rewritten tighter. The macro expands to `full` on the locked classes, so their `.rodata`
   is byte-identical, and it keeps both wordings on adjacent lines where the voice can be
   compared. `#undef`'d beside `POOL`.

   **Lesson worth keeping: short in characters is not short in LINES.** `"Rain mode engaged.
   Umbrellas mandatory."` is 39 chars — well under the ~45-char guideline — and still wrapped to
   four, because `"Umbrellas"` and `"mandatory."` are each ~10 chars against a ~13-char line and
   refuse to pair up. It took shorter *words* (`"Umbrella weather. No exceptions."`), not fewer
   of them. Only the audit catches this; do not eyeball it.

`prv_audit_phrases()` now measures in `ui_font_header()` on the small classes — auditing 24B
against an 18B box was flagging phrases that fit and passing ones that clipped, which is what
made #21 a real defect rather than a cosmetic one. Final audit: **zero overflows on basalt and
on chalk**, each confirmed against live app-log output rather than an empty log (see below).

### 6 Hours (D2) and Week Ahead (D3) — measured cascades

Both are surgical guarded inserts around the existing measured-cluster code, not duplicated
layouts, and both are `SMALL_RECT`-only — chalk is untouched and still draws all five Hours
columns and its pop% column, verified by screenshot (#30).

Hours implements D2's cascade in full: drop wind → tighten gap 4→3 and icon 14→13 → shorten the
imperial precip format (`0.2"` → `.2"`) → drop precip only as a last resort, re-measuring after
every step, with a final check that takes wind back if it fits again once something else was
dropped. Week drops pop% and keeps the condition icon (D3), with a gap 6→4 backstop for
pathological data.

**Both cascades were verified by instrumenting them, not by inference.** Temporary `APP_LOG`s
placed *inside* the `SMALL_RECT` guards (so the locked pair never compiled them, and the lock
stayed green throughout) reported the real decisions on live data:

* **Week: the cascade genuinely fires.** `usable=120 cluster=148 any_pop=1 popw=38` → pop%
  dropped → `cluster=104`. #31 is closed by measurement, not by luck.
* **Hours: `usable=120 cluster=117` — it correctly did nothing.** The emulator's hour labels
  measure 6 px, far narrower than the plan's 34 px `"12 AM"` worst case, so the row already fit.
  That left the drop path unexercised, so `usable` was temporarily forced to 80 to drive it:
  `cluster=76 wind=0 precip=1 short=1`, `precip_text_w` 23→17, the screenshot showing `.3"` /
  `.2"` with the droplet moved into the vacated wind slot and nothing clipped. Steps 1–4 and the
  wind-restore guard are all proven; step 5 (drop precip) is the untested last resort.

Both instrumentation lines and the forced `usable` were reverted, `grep`-confirmed absent, and
the tree re-clean-built green before the final captures.

### A verification trap that produced two false passes

Worth recording, because it nearly shipped an unverified result. The first phrase-audit run
reported **zero overflows on both platforms — and both were false.** `pebble install` and
`pebble logs` had been issued as *separate* tool calls, leaving tens of seconds between them:
the watchapp had already idled back out to the system watchface, so the button press went
nowhere, the card never drew, and the audit never ran. basalt's log held only a
`WebSocketConnectionClosedException` traceback and chalk's was completely empty — a zero grep
count on an empty file reads exactly like success.

**How to run it: `pebble install --emulator <p> --vnc --logs` backgrounded as ONE command, then
press and capture in the same call**, and always assert the log actually contains app output
before believing a zero count. Both final audits report 6 live app-log lines alongside their
zero.

### Still open on these cards

#26 / #33 / #84 — the 14 px condition icons in Hours and Week are still dither blobs on
diorite/flint. That is D6b, explicitly deferred: the watchface's ~40 px icon never had to solve
legibility at 14 px, and a remap does not fix a smudge. #63 (UV detail night hours) and #88–#91
(coverage gaps) remain out of scope for this pass.

---

## Phase 5 as built

D6's colour remap, plus one defect the remap alone did **not** fix. `icons.c` and
`air_quality.c` only; every edit sits behind `PBL_BW`, which cannot reach emery or gabbro
(both are colour units), so the lock was never at risk. `lock_guard.py` green at every build.
**Nothing is committed.**

**Closed:** #14 #15 #16 #32 #45 #46 #81 #82 #83 #85 — plus **#95 (new, below)**.

### The 5-macro port

`icons.c` gained `#include "theme.h"` and the watchface's `PBL_BW` block, and both dispatchers
(`icon_draw_condition`, `icon_draw_condition_animated`) now route all 22 raw colour arguments
through the macros. The `#else` branch holds the original constants verbatim, so the colour
platforms' token stream is unchanged — confirmed by emery/gabbro byte-identity *and* by basalt's
and chalk's digests being identical across builds.

### #95 — fog was invisible on 1-bit, and the plain port does not fix it

**The 5-macro port alone leaves one condition completely unrendered.** `icon_draw_fog()` and
`icon_draw_fog_animated()` are the only condition icons built from `graphics_draw_line` with **no
fill at all**, and they were sharing `ICON_CLOUD_COLOR` (`theme_muted()`). A fill in `theme_muted()`
dithers and stays visible — which is why cloud, storm and partly-cloudy all survive the remap — but
a **stroke does not dither**: it quantizes to the nearest of black/white, and `theme_muted()` is
`LightGray` on light / `DarkGray` on dark, i.e. the background in *both* themes. Fog drew the
background colour onto the background.

This is exactly D6's own rule 1, which the port would otherwise have violated while quoting it.
Fixed with a sixth macro, `ICON_FOG_COLOR` → `theme_secondary()` on `PBL_BW` (the inverse pair:
`DarkGray` on light, `LightGray` on dark, so it quantizes to the *foreground* in both) and
`GColorLightGray` in the `#else`, leaving colour platforms untouched.

**Found by instrumentation, not by reading.** A temporary `PBL_BW`-guarded counter
(`cond = s_force_cond++ % 7`) in `icon_draw_condition` made the 6 Hours column render six
*different* conditions in a single screenshot — and row 4 came back **blank**. Forcing
`cond = COND_FOG` in both dispatchers then confirmed it at both sizes: the 40 px hero icon and all
six Hours rows vanished entirely. After the fix the same forced build draws three solid bars in
every slot. The instrumentation was reverted, `grep`-confirmed absent, and the tree re-clean-built
before the final captures. Because the guard was `PBL_BW`, `lock_guard.py` stayed green *through
the instrumented builds too*, and basalt/chalk digests were unchanged by them — a useful side
proof that the temporary code reached only the 1-bit pair.

**Worth generalising: on 1-bit, "does it dither?" is a property of fill-vs-stroke, not of the
colour.** Before giving any 1-bit element a muted colour, check whether it is drawn with a fill or
a stroke. `icons.c` has 13 stroke-only draw helpers; fog was the only one receiving a muted colour,
but the next one added would silently disappear the same way.

### AQ gauge and pollen (#45/#46)

`aqi_category_color()` returns `theme_fg()` under `PBL_BW` — solid fill on the `theme_muted()`
track, the same solid-on-dither vocabulary as the UV card. The gauge now reads as an unambiguous
solid sweep against a checkerboard at AQI 53, where both halves were previously the same 50 %
dither. The same `theme_fg()` override is applied to the pollen `pcolor` (verified on flint:
"POLLEN: VERY LOW" was level 1's `GColorIslamicGreen`, now solid and legible). Both `#else`
branches are verbatim. The Big-Mode path picks the fix up for free, since it calls the same helper.

### #16 — closed by measurement, and it was already gone

`icon_draw_sun`'s rays do extend past `size/2` once the 3 px stroke is counted, so the register
entry was well founded — but Phase 3 retired the layout that exposed it. A pixel scan of the small
class main card with `COND_SUNNY` forced (the worst case, and the one #14 used to hide) reports
**zero ink in rows 0–15**; first ink is row 16, and the icon column runs 17–159. Nothing
approaches row 0 in either theme.

### #52 — diagnosed, deliberately deferred to Phase 7

Not cheap, and it is **not** a colour-quantization bug. On 1-bit `GColorIcterine` → white and
`GColorOxfordBlue` → black, so on the **dark** theme the moon reads correctly (white gibbous, dark
limb). The failure is light-theme only: the lit 73 % is white *on a white page*, so the disc reads
as an empty outlined ring with a black crescent inside — the eye takes the black shadow for the
moon, which is precisely the register's "reads as a thin crescent while the caption says 73 % LIT".

The obvious fix (invert to `theme_fg()` body / `theme_bg()` shadow) is the exact arrangement
`night_sky.c:12-16` documents having already tried and rejected, and it would also merge the lit
area into the black sky disc. The real fix is compositional — the sky disc has to become large and
solid enough for a white moon to sit *on* rather than *in* — which is Phase 7's Night Sky slot.
Left alone rather than churned.

### Verification

diorite **and** flint, both themes, five cards each (Main, 6 Hours, Week, Air Quality, Night Sky),
as a 4×5 contact sheet — every condition icon visible in all four combinations. basalt spot-checked
in full colour (grey cloud, orange/blue hi-lo arrows, orange gauge, green pollen) confirming the
colour path is untouched.

**Two emulator traps cost time here and are worth recording.** First, `pebble emu-button --repeat N`
**silently drops presses**: the card transition is 200 ms and `--interval 150` outruns it, so a
"go to Settings" of `--repeat 10` landed on Air Quality and every subsequent capture was of the
wrong card. Use single `click` invocations in a shell loop — their own ~1.5 s VNC startup is the
pacing. Second, the Phase-4 false-pass trap recurred in a new costume: an opening
`emu-button click back` intended to dismiss a possible update-notes modal instead **exited the
watchapp**, and all 13 screenshots of that run were the system watchface. Assert distinct-shot
counts (`md5 | sort -u | wc -l`) before believing any capture set.

### Still open on the 1-bit axis

#26 / #33 / #84 (D6b — the 14 px icons are legible now, but sun-vs-cloud-vs-storm are still hard to
tell apart at that size), #52 (Phase 7), #80 (Big-Mode icon), #86 (muted greys at 14 px).

---

## Phase 6 as built

`detail_modal.c` only. Every edit is behind `UI_SCREEN_SMALL_*` or `PBL_BW`, every `#else` holds the
shipped expression verbatim, and `lock_guard.py` was green on emery and gabbro at every one of the
five clean builds. **Nothing is committed.**

**Closed:** #54 #55 #56 #57 #58 #59 #60 #61 #62 #64 — plus **#96 #97 #98 #99 (all new, §3.21)**.
#63 (UV plotted against the temp buffer's night hours) stays deferred: it is platform-independent
and would move the locked pair.

### The font ramp had to be macros, not a rename (#61)

The obvious reading of Call 9 — "route the 14 `fonts_get_system_font()` calls through `ui_font_*`" —
**breaks the lock**, and it is worth being explicit about why, because the change looks like a no-op.
`detail_modal.c` is shared code that emery and gabbro compile. `ui_font_caption()` returns exactly
`FONT_KEY_GOTHIC_14` on the normal path, so the *rendering* is identical, but the *emitted call* is
not, and the guard compares binaries. The 14 sites collapse to four macros whose `#else` names the
original key verbatim; only the small classes take the accessor.

That indirection also hands the sheet the Big Mode ramp it never had, for free and at zero size cost:
on a small class `ui_font_header()` is 18_BOLD in both modes and `ui_font_caption()` only gains
weight, so no Big Mode layout can overflow as a side effect of this phase.

### Full-screen was the load-bearing change (#54, #55, #60 — and most of #96/#97)

`SHEET_HEIGHT_PCT` → 100 on both small classes retired both chalk blockers outright and paid for
nearly everything else. Confirmed against a pre-change capture set: at 80 % the leftover 34 px strip
drew the *card's* header above the sheet's own, so the screen read "6 HOURS" then "TEMP TREND" and
spent 35 % of a 168 px display on two titles; on chalk that strip was bisected through its x-height
and chord-clipped at both ends, exactly the "sliced half-glyphs" the register described. Both are
structurally impossible once the sheet covers the card.

Three details the change dragged in:

1. **Square top corners on the small classes.** The radius-6 `GCornersTop` fill sells the slide-up
   when a card stays visible behind; at full screen it just punches two 6 px notches of the old card
   through the top corners at rest.
2. **chalk carries its title lower** (`DM_TITLE_Y` 24 vs the rect classes' 10). Full-screen lifts the
   title out of the wide middle of the glass into the cap, where the chord at y=12 is only ~90 px —
   too tight for "TEMP TREND" plus its icon.
3. **`DM_CONTENT_BOT`** replaces bare `s_sheet_h` at every bottom anchor, because the sheet's lower
   edge is now the frame itself (−20 px on round for the arc, −4 on rect). It expands to a bare
   `s_sheet_h` on the large classes, so their expressions are character-identical.

### The axis rule was wrong at both ends of the range it covered (#57, #58, #59)

`i += (s_screen_w < 160) ? 2 : 1` fails twice over. On the rect classes it walks 0/2/4 and therefore
**never labels the last point**, so the axis had no end. On chalk, 180 is not < 160, so it drew all
six 40 px boxes across a chord-narrowed line — the register's "PM2 AM1 AM2 A". Rainfall was worse
again: its boxes are one `slot` wide (~19 px), which cut real hour strings to `1…` `1…` `2 …`.

Now first / middle / last, with the two end labels aligned **into** the plot (left at the first point,
right at the last) instead of centred on it, so no label can extend past the chart it describes. The
plot itself is derived from `ui_band_w()` measured at the label row's **lowest ink row** — the arc
eats a bottom-anchored box by its bottom corners, not its centre — so labels and the points they
describe always share an x-range. On chalk that costs the chart ~24 px of width and is worth it.

**#58 does not reproduce on live emulator data and that is the trap.** The emulator's hour labels are
`6`..`11`, ~8 px wide, so six of them fit chalk's line with room to spare and the screen looks clean.
The defect needs 12-hour strings (`12 AM`, ~34 px). This is the Phase-4 lesson in a new costume — the
Hours cascade measured `cluster=117` against a plan that predicted 156 for the same reason. **Fixed by
policy, not by eyeballing the shot.**

### #56: knockout, and clamp rather than flip

Value labels now draw on an opaque `theme_bg()` knockout, per Call 9. The first build placed them
above the point and **flipped them below when there was no room above** — which is how the new UV peak
annotation (#64) landed in the middle of its own curve on the first capture, since the peak is
usually the topmost point. Flipping drops the label into exactly the collision the knockout exists to
prevent. It now clamps up against the limit instead, where the worst case is touching its own marker.

The knockout does interrupt the trend line where the digits sit. That is the intended trade and it is
visible on the 1-bit captures; the value is legible, which it was not before.

#64 is implemented as a max-only annotation (a UV minimum is almost always 0). Its **value** is
correct; the **hour** it sits above is not, until #63 lands.

### The 1-bit sweep was larger than #62, and I introduced one regression fixing it

#62 named one element (the hint). The rule behind it — D6's rule 1, re-derived in Phase 5 as #95 —
condemns **nine**: three chart baselines, the hint, Rainfall's hour labels, Week's HIGH/LOW captions
and inactive page dots, and the AQ units caption. `ui_draw_dotted_hline()` was the giveaway: it draws
with `graphics_draw_pixel`, so it is a stroke and cannot dither, and `theme_muted()` is the background
colour in both themes on a 1-bit panel. All nine drew the background onto the background.

`DM_MUTED_INK` fixes them; the two `theme_muted()` **fills** (grab notch, pollutant track) were left
alone on purpose, and the AQ track's dither is load-bearing — it is what the solid fill reads against.

**Then the fix regressed something the contact sheets could not show.** Promoting the sheet's top edge
rule to `theme_secondary()` made it *visible* on 1-bit — and on a 1-bit panel there is no subtle grey,
so it became a solid bar across row 0 of the screen. Caught by sampling the pixel row rather than by
looking: `row0: 144/144 px dark` on diorite light. At full screen that rule marks no seam at all,
since nothing is behind it, so it now draws only while `s_top_y > 0` — mid-slide, when there
genuinely is a card underneath. Re-measured after: `row0 mean=255.0, dark=0/144`.

**Worth keeping: "make it visible on 1-bit" and "make it subtle" are incompatible. Before promoting a
muted element, ask whether it should be visible at all.**

### Verification

Five clean builds, `lock_guard.py` green on emery and gabbro at each. All four small platforms
captured in **both themes** — 40 sheet screenshots, every set asserted distinct via
`md5 -q | sort -u | wc -l` before being believed, and the theme toggle proved to have taken by
asserting 0 of 5 shots identical across themes per platform plus a mean-luminance check (basalt/chalk
241→17, diorite/flint 23→232). basalt spot-checked in full colour: orange accents, blue droplet and
bars, grey track all unchanged.

Captures use a **fresh install per sheet**. It costs ~10 extra installs per platform and it makes the
two traps from Phases 4 and 5 unreachable by construction: every run starts at card 0 (nav index is
not persisted), and no `back` press is ever issued, so a long-press that failed to open a sheet can
never cascade into a run of wrong-card or system-watchface captures.

### Still open on these sheets

#63 (UV night hours — platform-independent, would move the locked pair). The knockout's interruption
of the trend line is cosmetic and accepted. The Week sheet's condition icon is 34 px on rect / 26 on
round, down from 42, which is the price of getting its POP row back.

---

## Phase 7 as built

Shared chrome: `ui.c`, `nav.c`, `cards/radar.c`. All three are compiled by emery and gabbro, so this
is the phase with the least code and the most lock exposure. Every edit is a guarded insert with the
large branch preserved **verbatim** — `git diff` reports all three files as purely additive, zero
deleted lines, which is the mechanical form of that guarantee. `lock_guard.py` green at all six
clean builds, including the two instrumented ones.

**Closed:** #18/#72 #70 #71 #73 #74 #75. **#69 is deliberately left open** — its last live symptom is
Night Sky's "72% LIT" (#50), and it closes there in Phase 8.

### The pill: one clamp closes four defects (#70, #71, #18/#72)

`ui.c:172`'s `banner_w = PBL_IF_ROUND_ELSE(140, 130) + (big ? 20 : 0)` was completely unguarded —
the large classes' widths applied to every screen. Clamping it to `ui_band_w()` fixes all four
failures at once, because the helper already answers both classes' version of the question: the
class inset on rect, the inscribed chord on round.

Measured on the emulator, pill ink per row:

| | before | after | min clearance |
|---|---|---|---|
| basalt normal | 130 wide, 7 px gutters | **120**, gutters exactly **12** | = `ui_margin_x()` |
| basalt Big | 150 on a 144 px screen, origin **x = −3** | **120** | 12 px |
| chalk normal | 140, ~16 px past the glass each side | **116** (100→116→100) | **3.8 px** to the chord |
| chalk Big | 160 | **120** | inside |

The taper is the evidence that matters: the pill now measures 100 px at its top row, 116 at its
centre, 100 at its bottom, i.e. its rounded ends are **intact** rather than masked flat, which is
exactly what #18 described losing. It also retroactively justifies `ui_band_w()`'s "measure at the
band's vertical centre" rule (`ui_layout.h:57-61`) for a caller that is not text: a stadium's corners
are pulled in by `banner_h/2`, which is the same direction the chord narrows, so the centre
measurement is not merely convenient — the shape self-corrects at precisely the rows where measuring
at the far edge would have been pessimistic.

### The clamp shrinks the label, so the label had to become a cascade

Narrowing the pill narrows its inner text box to 108 px (rect) / 104 px (chalk), and Big Mode
independently promotes `ui_font_label()` to `GOTHIC_18_BOLD`. The draw call's
`GTextOverflowModeTrailingEllipsis` would have resolved the overflow by eating the tail — deleting
the number, which is the only part of the string that carries information.

`prv_fit_pill_text()` drops words instead, least informative first (" AGO", then the "UPDATED "
prefix), **measured at each step rather than switched on Big Mode**. Instrumented on live data:

```
P7PILL in='UPDATED 59M AGO' w=124 inner=108     <- Big Mode, GOTHIC_18_BOLD
P7PILL out='UPDATED 59M' banner_w=120
```

124 into 108 on both basalt and chalk, resolved to "UPDATED 59M". The estimate going in was
130–140 px; the real number is 124, and the cascade is indifferent to which was right. In normal
mode the same string measures ~96 px and the cascade correctly never fires.

### #74: theme_secondary() looked like the fix, measured as the bug

The first attempt gave the inactive dots `theme_secondary()`, reasoning from D6's rule 1 — the rule
that rescued the fog icon in Phase 5, where `theme_secondary()` quantizes to the foreground and
`theme_muted()` quantizes to the background. It came back on diorite at **37 % coverage against the
active dot's 93 %**: still a checkerboard, just a darker one. #74 would have shipped looking fixed.

**That rule is about strokes. These dots are a fill, and a fill does not quantize at all — it
dithers, whichever grey it is given.** Phase 5 recorded "does it dither?" as a fill-vs-stroke
property; the sharper statement is:

> Fill vs stroke decides **whether** an element dithers. Muted vs secondary only decides **which way
> a stroke quantizes**. A fill that must not dither has to be a pure endpoint — `theme_fg()`.

With `theme_fg()` both 1-bit platforms measure **75 % fill in both themes**, identical to the colour
platforms, and active/inactive is carried by length (a 16 px bar against 4 px squares) — the same
solid-vs-shape vocabulary as the AQ gauge and the Golden Hour chips. Note this is the second time a
correct rule has been applied one level too broadly; both times only pixel measurement caught it.

### #73: chalk cannot fit its own carousel, so the strip is windowed

With the full carousel (11 cards once radar is carved out) the strip is 116 px, and it sits at
y = 166 on a 180 px circle where `ui_band_w()` returns **66**. No gap tightening reaches that: at
gap 3 / dot 3 it is still 86. The cascade therefore tightens the gap 6→4 and then shows a **window**
of the strip centred on the active card — 7 slots at gap 4 = 64 px — with the outermost dot on any
clipped side drawn at half size as a "more this way" cue. Verified at both window positions: card 0
gives `[active][5 dots][half]`, card 5 gives `[half][2][active][2][half]`.

**SMALL_RECT is untouched and provably so** — 116 into a 120 px band fits, the cascade never fires,
and all 11 dots still draw. The window is a real loss of information and is confined to the one
class that geometrically cannot avoid it.

### radar.c (#75) — dormant, and wrong

`radar.c:217-218` was the last private pill model. It is not merely inconsistent, it is incorrect:
on SMALL_RECT `H - 35` = 133 places the attribution **7 px below the pill's own top edge**, i.e.
underneath it. It never showed because the card is carved out under 128 KB (`TW_RADAR_SUPPORTED`,
runtime-disabled in `settings.c:327-335`) and so never draws on a small unit — a dormant defect that
a screenshot matrix structurally cannot find. Now derived from `ui_content_bottom()`; the `#else`
keeps the shipped expression verbatim. The edit sits below `radar.c`'s two `APP_LOG` lines (117,
148), so their baked-in `__LINE__` values do not move.

### Verification, and a new capture trap

Four platforms × both themes, plus a Big-Mode instrumented run and a six-card regression sweep
(Advice / 6 Hours / Golden Hour on basalt and chalk) confirming nothing that consumes the pill's
geometry moved. Fresh install per capture; every set asserted distinct via `md5 -q | sort -u`.

**The theme toggle silently did nothing on the first attempt, and 8 of 8 distinct shots hid it.**
`SELECT` on the **Main card** is a manual refresh (`TouchWeather.c:240-243`), not the theme toggle —
the toggle only runs on *ordinary* cards, and card 0 is not one. Mean luminance was flat across the
"toggled" pair (28.1 → 27.6) while the shots still differed, because the hero icon animates and the
pill alternates on a 4 s cycle. Distinct-shot counting is necessary and **not sufficient**; the
theme assertion has to be a luminance flip, which is what Phase 6 used and what caught this.
**Toggle the theme from card 1, never card 0.** After navigating first, all four flipped
(28→230, 23→183, 214→41, 214→41).

One side proof worth keeping: after the instrumentation was reverted, basalt and chalk rebuilt
**byte-identical to the pre-instrumentation build**, while only diorite and flint moved from the
`theme_fg()` dot change — exactly the `PBL_BW` footprint that change should have, confirming both
that the revert was complete and that the 1-bit guard reached nothing else.

### Still open on shared chrome

#69, by design, until Night Sky lands in Phase 8. The chalk window shows 7 of 11 cards; that is a
deliberate trade against a 66 px band, not an unresolved defect.

---

## Phase 8 as built

The screen sweep: `night_sky.c`, `precipitation.c`, `update_notes.c`, `refresh_sheet.c`, plus the
`uv.c` / `air_quality.c` / `sun_cycle.c` / `icons.c` singles. Three of these files no phase had ever
entered — they were still 100 % `PBL_IF_ROUND_ELSE`, i.e. running the large classes' numbers.
`lock_guard.py` green at all fourteen clean builds, including four instrumented ones.

**Closed:** #39 #41 #42 #43 #44 #47 #48 #49 #50 #51 #52 #53 #65 #66 #67 #86 #92, and **#69** with
Night Sky, which was its last live symptom. **#68 does not reproduce** (below).

### Night Sky (#50–#53) — and #52 needed a third colour, not a bigger one

The card cascaded four rows downward from the header with fixed offsets and never looked at where
the stack ended. Replaced with a `ui_layout_solve()` stack against the real `ui_content_bottom()`.
Measured on basalt, before → after: gaps between the sky disc, the two name rows and the "% LIT"
row go from **1 / 5 / 7 px to an even 3 / 3 / 4**, and clearance above the pill from 2 px to 9.

**#50 is another #16 — it was no longer clipping.** Phase 1's pill nudge (rect −2, round −5) had
already bought it back, leaving the illum ink 2 px clear on basalt and **1 px** on chalk. It read
clean and was one font metric from not. The register's wording ("bisected by the pill") described
a state that no longer existed; solving the stack converts luck into structure.

**#52 cost two attempts and the first one made the card worse.** Phase 5's diagnosis said the sky
had to become "a field the moon sits ON rather than a rim it sits IN", so the first build widened
the 1-bit sky ring from 7 px to 10 and shrank the moon to keep the footprint. Captured, it read as
a *thicker annulus* — because the ring's width was never the problem:

> On the light theme the page is white, the sky black, the lit limb white and the shadow black.
> **Three regions have to be distinct and a 1-bit panel has two colours.** No arrangement of black
> and white separates page / lit / shadow.

The third tone is a dithered fill. The shadow now uses `theme_muted()` under `PBL_BW`, so it sits
between the solid sky and the white limb, and the moon reads as a disc with a shaded part in
**both** themes instead of an outline with a bite out of it. This is the Phase-7 page-dot rule run
backwards: there dithering was the defect and `theme_fg()` the fix; here dithering *is* the fix.
Fill-vs-stroke decides whether an element dithers — which makes it a bug or a tool depending on
what the element is for.

#53's `icon_draw_moon_small()` was a plain filled disc: its own comment narrates three abandoned
attempts at carving a crescent and settles for "an unfilled ring", which it also never draws. The
crescent is trivial once the background is known, and it is — the header paints over the card's own
fill. Implemented as a static in `night_sky.c` rather than in `icons.c`: the helper has exactly one
caller and the defect is small-classes-only, so the shared file stays out of the blast radius.

### Precipitation (#39–#43) — and a regression I caught by measuring the wrong thing first

`chart_bot = H - 86` is gabbro's constant. On chalk it put the floor at 94 against a pill at 140, so
a 100 %-probability bar was **18 px tall with 26 px of empty card beneath it**. Derived from the
pill instead, chalk's chart goes **36 px → 60** and rect's 60 → 62 (#41), which also absorbs most of
#42 — the residual gap is structural, since bars are bottom-anchored and the header is top-anchored.

Horizontally the block is now solved from `ui_band_w()` at the hour-label row: ink spans **x 14..134
on a 144 px screen, against x 1..143 before** (#39). One misstep worth recording: sizing the label
box to exactly the bar pitch ellipsized "Now" to "N…". Measured on chalk, "Now" is 23 px of ink
against a 24 px pitch — it fits — but a font's **layout** width includes side bearings and exceeds
its ink, and the draw call measures the former. The box now overhangs the pitch; boxes overlap,
ink does not, and ink is what has to clear the margin.

#43's header glyph passed `theme_accent_blue()` for **both** the cloud and the drops, so on 1-bit it
was a solid fg blob with the drops drawn in the same ink underneath. Phase 5 never reached it
because `icons.c`'s `ICON_*_COLOR` macros are file-local and only wire up the two condition
dispatchers. Fixed at **both** call sites — the Big Mode header duplicates the glyph, which is the
#99 rule applied before it could bite.

### What's New (#65–#68)

The header was eating **118 px of a 168 px screen** and the note got ~50 px. `"New on the horizon"`
is ~200 px at 24_BOLD, so it wrapped to two lines; the small classes take a one-line headline, an
18 px sun and a tighter rhythm, and the viewport roughly doubles (#66).

#65's cue is a chevron on an opaque knockout, drawn only while `off > frame_h - content_h` — the
quantity the scroll handler already computed. Verified as a real state indicator, not decoration:
after 30 down-presses the cue strip measures **0 ink pixels**. It also got its own reserved band
after the first build put it *over* the note, where the knockout simply punched a hole in whatever
line was at the bottom — trading one unreadable line for another.

#67: the body scrolls, so every line eventually passes the viewport's lowest row and the column has
to fit the chord **there**. Chalk's ran x 34..172 against a chord of ~39..141, with the accent
marker at x=20 outside the glass entirely. Now a chord-derived column at 14_BOLD.

**#68 does not reproduce.** The top-left corner is clean across three scroll positions and both
themes on diorite (0 non-background pixels in x 0..29 / y 0..15). Recorded as not-reproducing
rather than fixed, since nothing was changed with it in mind.

### The singles

* **#44** — "PEAK 7"'s box bottom landed exactly on the pill top (`c.y + 24 + 18 == 126`). Clamped
  to the content area; measured 4 px → **8 px** of clearance. But reserving the full box then
  squeezed the gap to "LOW" from 6 px to 2 — *moving* the crowding rather than removing it — so the
  clamp reserves 16 px and the row sits ~6 px off the pill and ~3 off the label.
* **#47** — the register says the numeral "overlaps the arc inner edge". Measured on diorite it
  **clears it by 1 px**; the numeral ink starts at y=57 and the arc's dither ends at y=55. 1 px is a
  coincidence rather than clearance, so both twins (`uv.c` and `air_quality.c`, identical
  construction) move 2 px down for ~3 px each way.
* **#48** — the pulse trace crams three excursions into ~12 px at the header's 18 px and merges into
  an angular wedge. One dip and one spike with real flat baseline reads as an ECG.
* **#49** — the sunrise/sunset chevron arms are a hardcoded 3 px at stroke 2, and on 1-bit that is
  the *entire* difference between the two glyphs once both collapse to `theme_fg()`. Replaced under
  `PBL_BW` with a filled triangle head that scales with the glyph.
* **#86** — `sun_cycle.c`'s two dotted separators were the last 1-bit-reachable muted **strokes** in
  `cards/`. Now visible.

`icons.c` gained `#include "ui.h"` for #48's screen-class guard; the locked pair's binaries did not
move, confirming the header contributes no code.

### Refresh sheet (#92) — and the register's numbers were stale

The register quotes a 48/54 px box; the code as it stands yields **65/72**. Either way it is two
lines of 24_BOLD and `FALLBACK_PHRASE` (39 chars) needs four. Now 18_BOLD in a chord-fitted box, and
the chord is measured at the **bottom of a three-line block** rather than at the box's centre: the
box runs to the sheet's edge and the text is top-anchored inside it, so the box's centre is nowhere
near the text — the same reasoning as the Phase-6 axis labels. First guards, and the first `ui.h`
dependency, in this file.

Verifying it needed two pieces of instrumentation, because the sheet is only reachable on a slow or
failed refresh: the phrase forced to `FALLBACK_PHRASE`, **and** `prv_start_close()` held, because
live emulator data completes the refresh before a screenshot can start. Both platforms then show
the full phrase in three lines with nothing clipped.

### Verification

Cards 4–8 on basalt, chalk and diorite (15 shots, all asserted distinct), flint spot-checked on the
two cards its `PBL_BW` changes touch, plus dedicated before/after pairs for every defect above.
Fourteen clean builds, `lock_guard.py` green at each.

A pleasant side observation: chalk's pill now reads "UPDATED 10M" where basalt reads
"UPDATED 8M AGO" — Phase 7's cascade firing on chalk's 104 px inner box and dropping a word rather
than ellipsizing the number, exactly as designed, on data nobody arranged.

### Emulator note

`pebble wipe` recovers a wedged emulator (basalt hung mid-install twice and no amount of process
killing fixed it), at the cost of resetting persisted state — which re-arms the What's New modal, so
the next launch is the notes screen and not card 0. Two capture runs were silently of the wrong
screen before that was spotted.

### Still open on these screens

#68 (not reproducing). #42's residual header-to-bar gap is structural.

---

## Phase 9 as built

Big Mode on the small classes: `main_card.c` and `hours.c`, both entirely inside their existing
`if (settings_get_big_mode())` blocks and additionally behind compile-time small-class guards, per
Part 0 rule 3 — Big Mode is a *runtime* branch that emery and gabbro also execute, so a guard on the
mode alone would have moved the locked pair. `lock_guard.py` green at every build, including the
instrumented ones.

**Closed:** #76 #77 #78 #79, plus a new defect the measurement turned up (#100, below).

### The register's numbers were wrong in both directions, and only a band scan showed it

Big Mode is phone-configured and the Settings card is read-only on the small classes since Phase 2,
so it was forced on via a guarded `settings_get_big_mode()` return. Scanning the rendered rows into
ink bands, basalt's main card measured:

```
BEFORE                                AFTER
  icon   y   0.. 36                     icon   y   3.. 40
  (gap 22)                              (gap 12)
  temp   y  59.. 87                     temp   y  53.. 81
  (gap 13)                              (gap 12)
  hi/lo + PILL  y 101..147  <- ONE      hi/lo  y  94..112
                               band     (gap 7)
                                        pill   y 120..147
```

* **#76 is confirmed in the strongest possible form**: hi/lo's ink and the pill did not merely touch,
  they scanned as a **single contiguous band**, which is what "the degree ring merges with the pill
  outline" means in pixels. Now a separate band with 7px of air.
* **#77 does not reproduce as written.** The register says "~90px dead band"; the real gap was
  **22px** against 13 below — an imbalance, not a chasm. The 90px figure appears to have been taken
  from a large class or a pre-Phase-3 layout. Rebalanced to 12/12 on rect and 17/17 on chalk.
* **#100 (new): in Big Mode the hero icon's rays reach row 0.** This is #16 — which Phase 5 closed
  by measurement — surviving in the Big Mode path, because Phase 3 only ever rewrote the *normal*
  mode layout. Same retune fixes it; the icon now starts at row 3.

The retune is stated as ink, not boxes: the band is 4..116, the three rows' ink is 37 + 29 + 19 = 85,
so 27px of slack splits into two 13px gaps. Working in box coordinates is what produced the original
table, where a 28px text box whose ink sits 5px down and 8px short reads as "clear of the pill"
while the glyphs are not.

### #78 and #79

`icon_cy = row_y + th/2 - 2` centres the row icon on its text BOX, but GOTHIC_24_BOLD's ink sits low
inside that box — measured, text ink 44..59 (centre 51.5) against an icon centre of 47. The icons
floated about 5px high and read as belonging to the row above. Centred on the ink instead.

#79 is two pixels: the arrow's ink ends ~1px short of the digit box (its arms reach `arrow/2` plus
half a 3px stroke) so the glyphs touch at 24_BOLD. `ag` 4 → 6 on the small classes.

**A caution about measuring icons across builds.** The first before/after comparison of #78 showed
the icon's ink centre barely moving, which looked like the edit had not taken. It had — the *live
condition had changed between captures*, so the two shots were of different glyphs with different
ink extents. Icon geometry can only be compared across builds with the condition pinned, or by eye
on a zoomed crop; a bare centre-of-ink number is not comparable.

### Verification

Big Mode forced on, basalt and chalk, main card and 6 Hours, with band scans before and after and
zoomed crops for the two two-pixel fixes. Then the force reverted and **normal mode re-captured** to
confirm it is untouched — the whole phase lives inside Big-Mode branches, but that is a claim worth
a screenshot rather than an assertion.

### Emulator note, and a false pass it nearly caused

`pebble install` needs roughly **10–14 seconds** before the app has relaunched and rendered; at the
`sleep 2`–`sleep 5` this project has used elsewhere the screenshot catches the *previous* state. Two
Big-Mode capture sets were of a stale card before a distinct-shot assertion caught it — the same
`md5 | sort -u` check that has caught every other capture failure in this project, and the one I
skipped on that run. Assert distinctness on every set, including the ones that "obviously" differ.

---

## Phase 10 as built

D6b plus the polish tail. `icons.c` only — every edit behind `PBL_BW`, which emery and gabbro
cannot compile, so the lock was never at risk. Green at every build.

**Closed:** #26 #33 #84 #80. **#27, #29 and #35 do not reproduce as written** and were left alone;
see below, because "measure before changing" is the whole point of recording them.

### D6b: fill-vs-outline is the alphabet

Phase 5's remap made every condition *visible*; the register was right to keep "distinguishable" as
a separate defect. Forcing all seven conditions into one Hours column showed why: at the 13–14 px
that column uses, cloudy / rain / snow / storm are the **same ~10×9 dithered smudge**, and the only
three that read — sunny, partly, fog — read because they differ in FILL rather than in detail.

So fill and outline carry the meaning:

| condition | 1-bit glyph |
|---|---|
| sunny | solid disc |
| partly | solid disc + solid cloud |
| cloudy | **hollow** cloud |
| rain | hollow cloud + 3 vertical ticks |
| snow | hollow cloud + 3 square pips |
| storm | **solid** cloud + bolt |
| fog | 3 solid bars |

"Hollow" is drawn by filling the silhouette and carving it with the background, not by stroking a
path: a stroked circle overlapping a stroked rect shows its internal seams at 14 px, while a carve
leaves one clean outline of the union. Same technique as the Night Sky crescent.

Three details only the captures could have settled:

1. **Rain's ticks merged into a solid block** at stroke 2 with `size/4` spacing — the glyph read as
   a filled cloud with a tail. 1 px at small sizes, spaced `size/3`.
2. **Snow's dots vanished.** A radius-1 circle is one pixel; next to rain's ticks it read as
   nothing. Square 2×2 pips read as "not a line", which is the distinction that matters.
3. **Partly-cloudy's hollow cloud didn't work at 3/4 scale** — the carve is too small to register.
   It is solid there, and CLOUDY owns the hollow form. Disc-plus-solid against outline-only is a
   stronger pair than two near-identical outlines anyway.

### #80 was the twin, again

`icon_draw_condition_animated()` is a **second dispatcher** over the same conditions, used by the
hero and by What's New. Fixing only `icon_draw_condition()` would have left the same condition
rendering as a solid silhouette in a Hours row and as a borderless dither field in the hero directly
above it. **This is the third time in this project that a defect lived in a duplicated helper**
(#99 in the detail sheet, #43 in Precipitation's two headers, #80 here). The rule has earned its
place: when a fix lands in one of two dispatchers, go and find the other one.

Verified at hero scale with the condition pinned per build — cloudy, rain, snow and storm are four
obviously different 40 px glyphs. The rotating sun is kept on 1-bit; it is the app's signature and a
solid disc with rays reads perfectly well. The other conditions lose their bob, which was never
legible at 1 bit.

### The polish tail: three defects that measured as non-defects

* **#27** ("`°` kerns into the preceding digit; '78°' reads '78o'"). The stated mechanism is wrong.
  A pixel map of "71°" at 14_BOLD shows a **3 px gap** between the "1" and the degree ring, and the
  ring sitting in the upper half of the digit height — which is correct degree placement. What is
  true is that GOTHIC_14_BOLD's `°` is a chunky 5×5 ring; that is the system font's glyph, and
  replacing it means hand-drawing rings on the row-draw path for every temperature on two cards.
  Not worth it for a cosmetic minor. **Documented, not changed.**
* **#29** ("row icons ride ~4 px high"). Measured: icon ink centre 42.5 against text centre 41.0 —
  **1.5 px LOW**, not 4 px high, i.e. aligned within a pixel of rounding for an 18 px-tall glyph
  against 9 px of text.
* **#35** ("5 rows in a 100 px band, top-heavy with dead space above the pill"). Measured: block ink
  38..112 in a 26..126 band — **12 px above, 13 px below**. Balanced. Phase 4 predicted exactly this
  ("partly addressed as a side effect" of routing Week through the real pill geometry) and it was
  right.

Three of the four remaining minors were already fixed or mis-stated, which is consistent with #16,
#47, #50 and #68 before them. **The register was written from screenshots taken before Phase 1;
after nine phases of geometry changes, an entry describes the app that was, not the app that is.
Measure the current build before changing code to match a description of an old one.**

### Verification

diorite in both themes on live data (theme flip asserted by mean luminance 206 → 49), showing solid
discs, disc-plus-cloud and hollow clouds as three distinct row glyphs, and the Week card rendering
four different conditions unambiguously. Hero scale verified separately with pinned conditions.
Colour platforms are untouched by construction — every edit is inside `PBL_BW`.
