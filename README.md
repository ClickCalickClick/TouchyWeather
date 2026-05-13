# TouchyWeather ⌚🌦️  
**Weather, but with attitude. Built for touch-first Pebbles.**

TouchyWeather is a playful, card-based weather app for Pebble that mixes useful forecast data with personality.  
It’s designed around fast swipes, quick glances, and weather advice that can be practical, sarcastic, and occasionally unreasonably honest.

<div align="center">

## 🌟 Why this app is different
### **Touchscreen-first weather UX** on Pebble  
### **Built only for `gabbro` and `emery`**  

Most Pebble weather apps are button-first. TouchyWeather is built around touch interactions from day one:
**swipe left/right to move cards, pull down to refresh, tap to move Settings cursor**.

</div>

---

## Feature Overview

TouchyWeather is a carousel of focused weather cards:

- **Main** — current temp, feels-like, high/low, wind, humidity, quick status
- **Touch & Go** — dynamic weather guidance with humor/sarcasm
- **6 Hours** — next 6-hour outlook
- **Week Ahead** — 4-day forecast snapshot
- **Precipitation** — near-term rain probability view
- **UV** — UV index + risk context
- **Air Quality** — AQI + air quality signal
- **Sun Cycle** — sunrise/sunset timing
- **Night Sky** — moon phase + illumination
- **Golden Hour** — blue/golden hour timing blocks
- **Radar** — streamed radar image card with location crosshair
- **Settings** — enable/disable cards to build your own weather flow

---

## Radar: Highlight Feature

The Radar card streams a watch-optimized image from the radar proxy and renders it directly on-watch, including:

- loading/progress states
- error handling/retry
- center crosshair over your location
- RainViewer attribution

You can force a radar refresh with **SELECT** while on the Radar card.

---

## Settings Card: Your Deck, Your Rules

TouchyWeather includes an in-app **Manage Cards** screen where you can enable/disable cards anytime.  
`MAIN` and `TOUCH & GO` stay locked on (always available), while other cards can be toggled on/off.

This means you can run ultra-minimal (just core weather) or full nerd mode (everything enabled).

---

## Touch & Go (the personality engine)

Touch & Go classifies live conditions into tiers (storm, rain soon, hot, high UV, bad air, pleasant, etc.) and picks a fitting line.

Examples from the app’s phrase pools:

- “**Lightning out. Stay in. Don't be a conductor.**”
- “**Hydrate or wilt.**”
- “**Hair plans? Cancelled.**”
- “**Sunscreen isn't optional.**”
- “**Boring weather. The good kind.**”

It’s meant to feel like your weather app has opinions.

---

## Controls

- **Swipe left/right**: previous/next card
- **Pull down**: manual weather refresh
- **Tap (on Settings card)**: move settings cursor
- **SELECT (Settings)**: toggle highlighted card on/off
- **SELECT (Radar)**: force radar refresh
- **SELECT (other cards)**: toggle light/dark theme

Buttons still work too (`UP`/`DOWN` for navigation), but touch is the star.

---

## Platform Support

TouchyWeather targets only:

- `emery`
- `gabbro`

No legacy non-touch Pebble targets are included.

---

## Build / Run

```bash
pebble build
```

Install to emulator (headless-friendly):

```bash
pebble install --emulator basalt --vnc
```

Take a screenshot:

```bash
pebble screenshot --emulator basalt --vnc --no-open screenshot.png
```

---

## Open Source

TouchyWeather is open source in this repository.  
Feel free to explore, fork, tweak card behavior, expand phrases, and make it even more delightfully extra.
