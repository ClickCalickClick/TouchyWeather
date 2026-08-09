# Store listing copy — TouchyWeather

Paste-ready text for the appstore entry. See [FEATURES.md](FEATURES.md) for the
full feature breakdown and the comparison against the TouchyWeather Face.

---

## Title

```
TouchyWeather
```

## Category

Weather

## Short blurb (one line)

```
Weather, but with attitude — twelve cards, live radar, and a forecast that isn't always polite about it.
```

Alternatives:

```
A card-based weather app for Pebble: hourly, weekly, UV, air quality, radar — and opinions.
```

```
Swipe or click through twelve weather cards. Hold SELECT for the charts. Pull down to refresh.
```

## Description

```
TouchyWeather is a card-based weather app for Pebble that mixes useful forecast
data with personality. It's built for fast glances, quick swipes, and advice
that can be practical, sarcastic, and occasionally unreasonably honest.

TWELVE CARDS, YOUR DECK
Current conditions. Six hours, hour by hour. Five days ahead. A rain-probability
bar chart. UV with the day's peak. Air quality with pollen. Sunrise and sunset.
Tonight's moon. Blue hour and golden hour for both ends of the day. Live radar.
And a settings card to show, hide and drag-reorder the lot — on the watch, or
from your phone.

HOLD FOR THE CHARTS
Hold SELECT — or swipe up — on 6 Hours, Week, Precipitation, UV or Air Quality
and a sheet slides up with a proper vector chart: the temperature trend, the
hourly UV curve, rainfall amounts, one detailed day at a time, or the pollutant
breakdown. Press SELECT again to toggle an overlay onto it.

LIVE RADAR ON YOUR WRIST
The radar card streams a real precipitation frame onto the watch and draws it
on-device, centred on your exact location. (Radar needs the extra memory of the
Pebble Time 2 and Round 2.)

TOUCH & GO
The personality engine reads live conditions, sorts them into one of fifteen
tiers — storm, rain soon, hot, cold, wind, high UV, bad air, muggy, pleasant —
and picks a fitting line. It shows you the reading that triggered it, so you
know why it said that. It is not always polite.

TOUCH-FIRST, BUTTON-COMPLETE
Swipe between cards, pull down to refresh, swipe up for detail. Every one of
those has a button equivalent, so nothing is locked behind a touchscreen.

BIG MODE
An accessibility mode for reduced eyesight: much larger fonts, high-contrast
colours, and simplified cards with fewer, bigger items. The full detail is still
one SELECT-hold away.

IT WORKS WHILE IT'S CLOSED
Background refresh every 30 or 60 minutes, and a launcher glance showing the
current temperature and condition without opening the app.

Works on Pebble Time, Time Round, Pebble 2, Pebble 2 Duo, Pebble Time 2 and
Round 2. Weather from Open-Meteo, radar from RainViewer — no account, no API
key.

Want the same weather on your watch face instead? TouchyWeather Face is the
companion.
```

## Keywords

```
weather, forecast, radar, rain, UV, air quality, pollen, hourly, week,
moon, golden hour, cards, touch
```

## Privacy note for the listing

The app sends one anonymous ping per day so active-user counts can be tracked.
It carries a per-user token (no name, no email), hashed server-side, and
coordinates rounded to 0.1° (~11 km). Nothing else leaves the device, and a
failed ping never affects weather.

## Screenshot order

Upload in this order — it opens with the deck, proves the depth, then shows the
two things nothing else on the store has:

1. `01-card-main` — current conditions, the card you land on
2. `03-card-hours` — the hourly forecast
3. `13-detail-hours` — hold SELECT and the chart slides up
4. `11-card-radar` — live radar *(emery / gabbro only)*
5. `02-card-advice` — Touch & Go having an opinion
6. `12-card-settings` — the deck, your way

On the platforms without radar, use `05-card-precip` in slot 4.

Slot 6 does not show the same thing everywhere, so do not reuse one caption.
On emery and gabbro the Settings card manages the deck on the watch — SELECT
toggles a card, an UP/DOWN hold reorders it — and "build your own deck" is
accurate. On the four small-screen platforms that management moved to the phone,
so the card is a settings summary (theme, Big Mode, a CARDS IN APP pointer) and
should be captioned as such.

Native-resolution PNGs for every platform are in
[screenshots/](screenshots/).
