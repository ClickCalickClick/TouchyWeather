> **ASSEMBLY NOTES — delete everything above the horizontal rule before posting.**
>
> 1. Compose in **Markdown mode** first: paste from the rule down.
> 2. Then switch to the **rich-text (fancy-pants) editor** and drag these three
>    onto their `⟦…⟧` markers, replacing each marker line:
>    - `store/video/promo_app.png` — the twelve-card compilation
>    - `store/video/detail_sheet.gif` — the hold-SELECT animation
>    - `store/video/promo_face.png` — the watch-face compilation
>
>    Inline images only work in rich text, and the markdown→rich-text switch is
>    effectively one-way, so do this last.
> 3. Old.reddit renders no inline images — the post is written to read
>    complete without them. Fill in the album link on the "screenshots" line
>    so those users still have a path.
> 4. Fill in the two appstore links at the bottom.

---

# TouchyWeather 2.0 is out, and there's now a matching watch face

Two releases at once: **TouchyWeather 2.0** (the watchapp) and **TouchyWeather Face**, a brand-new weather watch face that's the companion to it.

Both run on **all six modern Pebbles** — Pebble Time, Time Round, Pebble 2, Pebble 2 Duo, Pebble Time 2 and Round 2. Weather comes from Open-Meteo. No account, no API key, nothing to sign up for.

*(Every card on every watch, as an album: LINK — for anyone on old.reddit, where the inline images below don't render.)*

---

## First, the elephant

In the [July 2026 Mega Update](https://repebble.com/blog/pebble-mega-update-july-2026), Eric listed a *"beautiful new weather app for PT2 and PR2"* on the roadmap, built by **grim** (a Spring Developer Contest winner). It's entirely possible something like that ends up bundled into a future firmware as the default weather app.

That is not a reason for me to stop, and I want to say so plainly up front: **TouchyWeather is still in active development and will stay that way.** More weather apps on Pebble is good for Pebble. A bundled default and a deep third-party option are different products serving different people — I'd rather build the opinionated one and let you choose. If grim's app ships as the default and it's great, install both. I'll keep shipping.

---

## What's new in TouchyWeather 2.0 (the app)

⟦INLINE IMAGE: store/video/promo_app.png⟧

TouchyWeather started life as a **touchscreen-only** app for the Pebble Time 2 and Round 2. That was the whole premise, and it also meant most of you couldn't run it. 2.0 fixes that.

**🎉 It now runs on six watches, not two.** Pebble Time, Time Round, Pebble 2, Pebble 2 Duo, Time 2 and Round 2. And not as a squeezed-down port — layouts branch across four screen classes (small-rect, small-round, large-rect, large-round) so each display gets geometry actually tuned for it. On the 1-bit models (Pebble 2 / 2 Duo) colour accents become dither patterns so charts and icons stay readable without hue.

**Every touch gesture has a button equivalent.** Touch is an enhancement, never a requirement. Swipe L/R → UP/DOWN. Pull-to-refresh → SELECT on the main card. Swipe up for detail → hold SELECT. Nothing is locked behind a touchscreen you don't have.

**🔍 Big Mode.** A real accessibility mode for reduced eyesight: much larger fonts, high contrast, and simplified cards showing fewer, bigger items. The main card drops to temperature and condition. The full detail is still one SELECT-hold away. If you don't turn it on, the app is pixel-identical to before.

**📊 Detail sheets.** Hold SELECT (or swipe up) on **6 Hours, Week, Precipitation, UV or Air Quality** and a bottom sheet slides up with a proper vector chart — the temperature trend, the hourly UV curve, hourly rainfall amounts, one detailed day at a time, or the pollutant breakdown (PM2.5, PM10, ozone, NO₂). SELECT toggles a secondary overlay onto it, like the rain-probability line over the temperature trend.

⟦INLINE GIF: store/video/detail_sheet.gif⟧

*Above: holding SELECT on the 6 Hours card pulls up the temperature trend, then SELECT drops the rain-probability line onto it. BACK slides it away. (Pebble Time 2.)*

**🃏 Card management from your phone too.** Show, hide and drag-reorder your cards from the watch *or* from the phone settings. Main is locked first, Settings locked last, everything in between is yours. It's deliberately fail-safe — the Settings card can only be hidden while phone-side management is on, so you can never lock yourself out.

**🌧️ Rain and Radar can hide themselves** when no rain is expected, so your deck stays short on dry days.

**🔋 Longer battery life** — animations pause when you stop interacting.

### And what was already there, if you're new to it

Twelve cards total: Main · Touch & Go · 6 Hours · Week Ahead · Precipitation · UV · Air Quality · Sun Cycle · Night Sky · Golden Hour · Radar · Settings.

- **Live radar on your wrist.** A real RainViewer precipitation frame, pre-quantised to Pebble's palette by a small proxy, reassembled and drawn on-device with a crosshair on your exact location. It needs ~51 KB of peak heap to assemble a frame, so it's PT2/PR2 only — on every other watch the card is simply removed from the carousel rather than failing at runtime.
- **Touch & Go**, the personality engine. It sorts live conditions into one of fifteen tiers — storm, rain soon, cold rain, snow, hot, cold, wind, high UV, bad air, muggy, pleasant, stale data — and picks a line from that tier. It also shows you the reading that triggered it, so you know *why* it said that. Actual lines from the app:
  > *"Electrical storm active. Don't be a conductor."*
  > *"Hydrate or wilt. It is cooking out there."*
  > *"Slush is the new ice. Step lighter."*
- **It works while it's closed.** Background refresh every 30 or 60 minutes, and a launcher glance so your launcher entry shows the current temp and condition without opening anything.

---

## The new watch face: TouchyWeather Face

⟦INLINE IMAGE: store/video/promo_face.png⟧

Different codebase, same design language. This is the glanceable half of the pair.

**The layout is measured, not templated.** Six possible rows — time, date, complication lines, weather, badges, last-updated — get measured, stacked with elastic gaps and centred *every draw*. Switch a row off and the rest don't just shuffle up: the whole stack re-centres around what's left. Row heights use the glyphs' real ink heights, not the fonts' layout boxes, so the top and bottom margins come out genuinely equal instead of top-heavy. There's no fixed chrome band anywhere reserving space, so nothing is left behind holding a gap open.

**Four complication slots, one menu.** Two text lines under the date, two coloured pills below the weather. Any slot takes any reading:

> feels like · wind · humidity · dew point · UV index · air quality · rain chance · step count *(lines only)* · **watch battery**

Two readings on one line draw as a single centred group split by a hairline rule — `FEELS 75° | 12 NW` — each keeping its own accent colour. No reading can occupy two slots at once: the phone pickers grey out what another slot holds, and the watch de-duplicates at draw time.

Each badge has an opt-in **"only when notable"** filter, so a pill stays hidden until it's worth reading — rain ≥ 50%, UV ≥ 6 at midday, battery ≤ 20% or charging.

**Nudge for the forecast.** A watch face gets no buttons and no touch events (`touch_service` is watchapp-only on current firmware), so this one listens to your wrist. **Nudge** the watch — flick, or a firm tap on the glass — and the lower half deals a peek card: **6 Hours → Week Ahead → Conditions → Sun + Moon**. It auto-returns after ~7 s, and the deck *resumes* — your next nudge picks up after the last card you saw instead of restarting at page one.

You can set a nudge to instead open **one fixed view** (the dense everything-overlay, or any page you pin), let the pages **auto-rotate** on a timer, or turn it **off** and keep just the clock. "Nudge input" picks which motion counts: wrist flick (X/Y), tap (Z), or either.

*(The full touch implementation — tap, swipe L/R, swipe up, swipe down — is already written and ships compiled out behind a flag, using the app's exact thresholds. The day firmware delivers touch events to faces, it's a one-line rebuild.)*

**It watches the sky for you.**
- **Rain auto-peek** — the hourly page flashes up by itself when rain is due within the hour (edge-triggered, fires once per event, not in a loop).
- **Night mode** — after sunset the face goes dark and swaps the hero icon for tonight's moon phase, then restores your daytime theme at sunrise.
- **Rain alert takeover** — the bottom row alternates `RAIN IN 15M` (orange) with `UPDATED 5M AGO` (muted), even if you switched the status row off.
- **Quick View reflow** — when the timeline Quick View band eats the bottom of the screen, the face sheds optional rows bottom-up rather than clipping. Except during a rain alert, where the status row sheds *last* so the warning survives.

**Battery discipline.** At rest with no rain alert it wakes **exactly once per minute**, on the tick. Every other timer is conditional and self-cancelling — the 10 Hz animation only inside its 8 s window, the banner flip only during a rain alert, the idle return only while a peek is open. It also needs no background wakeups to stay fresh: a face's PKJS runs while the face is on screen, so the refetch rides the minute tick.

**The phone settings page has a live schematic of your face** that reshapes as you change slots — boxes appear, disappear and re-centre, and a conditional pill draws dashed. It follows your watch's actual shape and colour depth.

---

## If you install one, why the other is worth it

Fair question, so here's the honest split rather than a sales pitch.

**They share a lot.** Same Open-Meteo forecast and air-quality pipeline, same reverse geocoding, no API keys either way. Same six platforms, same light/dark theme, same icon set and accent colours. Both are configured from your phone with Clay, both cache the last reading on the watch so something is on screen before the first new byte arrives, and both show a one-time "what changed" card after an update. Both offer imperial/metric, dew point instead of humidity, and a manual `lat,lon` location override. Set one up and the other will feel immediately familiar.

*(They are two separate installs with two separate settings pages — configuring one doesn't configure the other.)*

**What the face does that the app can't:** tell you the time. It's on your wrist all day, holds four readings you picked, and needs no launching. Plus the ambient stuff — rain auto-peek, night mode, Quick View reflow — and **watch battery / step count** as complications, which the app has no place for.

**What the app does that the face can't:** live radar, Touch & Go, the detail sheets with real charts, dedicated Golden Hour / Precipitation / UV / Air Quality cards, pollen, the pollutant breakdown, Big Mode, on-watch card management, and a launcher glance.

> **The face is where the weather comes to you. The app is where you go to read it properly.**

So: if you're already running the face and you've ever nudged it wishing you could dig further — that's the app. And if you're an app user who has to launch it to check the temperature, that's the face.

---

## Platforms

| Watch | Platform | Display | Colour | Touch | Radar |
|---|---|---|---|---|---|
| Pebble Time 2 | `emery` | 200×228 rect | ✅ | ✅ | ✅ |
| Pebble Round 2 | `gabbro` | 260×260 round | ✅ | ✅ | ✅ |
| Pebble Time | `basalt` | 144×168 rect | ✅ | — | — |
| Pebble Time Round | `chalk` | 180×180 round | ✅ | — | — |
| Pebble 2 | `diorite` | 144×168 rect | 1-bit | — | — |
| Pebble 2 Duo | `flint` | 144×168 rect | 1-bit | — | — |

Graceful degradation, not blocked features: non-touch models lose touch *gestures* (every one has a button equivalent), and the 1-bit models additionally lose radar and colour accents. No platform loses a weather *feature* it can physically render.

**Privacy:** one anonymous ping per day so I can see active-user counts. A per-user token (no name, no email), hashed server-side, and coordinates rounded to 0.1° (~11 km). Nothing else leaves the device, and a failed ping never affects your weather.

---

**TouchyWeather 2.0** — LINK
**TouchyWeather Face** — LINK

Feedback, bug reports and "why doesn't it do X" all welcome — this thread or the repo. Thanks for making Pebble development fun again. 🙏
