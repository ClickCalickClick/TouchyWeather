# Changelog

All notable changes to TouchyWeather. The top entry is the newest release; the
build reads it to generate the in-app "What's New" screen, so keep the most
recent version at the top with `## x.y.z` and one bullet (`- `) per line.

## 2.3.0
- Opening the app before your first forecast now shows the refresh animation instead of a bare no-data screen — it slides away as soon as the forecast lands.
- Background refresh no longer blanks your watch white — thanks to Eric Migicovsky for finding and fixing it.
- A background update now shows a labelled status frame, and is skipped entirely when your phone is out of range.
- Background updates no longer stop until the next time you open the app.
- Out of range, retries now back off properly instead of repeating every few minutes.

## 2.1.0
- Wind speed can now be shown in m/s, separately from °C and °F — choose it in Settings.
- Big Mode: the 6 Hours card now reaches all six hours — press SELECT to page between them.
- Big Mode: temperature units are now labelled, and the detail views use large type too.
- The 6 Hours card no longer shows rainfall in millimetres when inches are selected.
- The Week card no longer shows placeholder Fahrenheit data when there is no fresh forecast.

## 2.0.0
- TouchyWeather now runs on six watches: Pebble Time, Time Round, Pebble 2, Pebble 2 Duo, Time 2 and Round 2.
- New Big Mode — much larger text, high contrast and simpler cards. Turn it on in Settings.
- Hold SELECT on a forecast card, or swipe up, for a detail view with charts.
- Show, hide and drag to reorder your cards from your phone as well as on the watch.
- Rain and Radar can now hide themselves when no rain is expected.
- Longer battery life: animations pause when you stop interacting.

## 1.11.1
- Background auto-refresh now keeps working after you update or reinstall the app.

## 1.11.0
- Show your location on the main screen — turn it on in Settings.
- The 6-hour forecast now always shows wind speed and direction.
- Smarter rain alerts, now based on expected rainfall.

## 1.10.0
- Choose your clock format — 12-hour, 24-hour, or match your watch — in Settings.

## 1.9.1
- last minute bug fixes

## 1.9.0
- Background auto-refresh now works — pick an interval in Settings.
- Anonymous usage stats so we can improve the app (no personal data collected).
- New "What's New" screen after each update.

## 1.8.1
- cache fix!
