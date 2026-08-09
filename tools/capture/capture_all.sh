#!/bin/zsh
# Capture a full store set for one platform.
#
# Captures a GENEROUS labelled trail rather than trusting press counts: the
# first press after an install is usually swallowed while the app settles, and
# `--repeat` silently drops presses, so every press is its own click and every
# resulting frame is kept for review. Assembly into NN-name.png happens after a
# human looks at the contact sheet.
#
# Position is re-established by REINSTALLING rather than by pressing back to
# card 0: an install always relaunches on the Main card, which is the only
# position in this app that can be asserted without looking at a screenshot.
# (`back` would exit the watchapp outright.)
#
# usage: capture_all.sh <platform> <outdir> <card_count>
set -u
P=$1
OUT=$2
NCARDS=$3
EXPECT_LOC=${4:-}
mkdir -p "$OUT"

# Built as an array, not via ${VAR:+--flag "$VAR"}: zsh does not word-split
# that expansion, so the flag and its value reach python as a single argv
# entry ("--expect-location Orlando") and argparse rejects it.
typeset -a LOC_ARGS
LOC_ARGS=()
[[ -n "$EXPECT_LOC" ]] && LOC_ARGS=(--expect-location "$EXPECT_LOC")

shot()  { pebble screenshot --emulator "$P" --vnc --no-open "$OUT/$1.png" >/dev/null 2>&1 }
# Theme is set EXPLICITLY (Clay key 10000: 0=Light, 1=Dark) rather than by
# pressing SELECT. The toggle depends on which card the press lands on — SELECT
# on Main is a refresh, not a flip — and it inherits whatever theme the previous
# run left persisted, which silently produced a whole set in the wrong theme.
set_theme() { pebble send-app-message --emulator "$P" --vnc --int 10000=$1 >/dev/null 2>&1; sleep 3 }
press() { pebble emu-button --emulator "$P" --vnc click "$1" >/dev/null 2>&1; sleep "${2:-2.5}" }
hold()  { pebble emu-button --emulator "$P" --vnc click select --duration 800 >/dev/null 2>&1; sleep "${2:-3}" }
HERE=${0:A:h}

# An install always relaunches on the Main card, and `sleep 18` is enough for
# the app to RENDER — but not necessarily to be FED. A launch that draws before
# its payload lands shows the mock in weather_data.c (TUE 78/58, a 73% moon,
# 0.01" on rows 4-5), which is stable and distinct, so md5 assertions pass on a
# set with no real weather in it. SELECT on the Main card is the documented
# force-fetch, and wait_fetch.py blocks until PKJS records that it completed.
relaunch() {
  pebble install --emulator "$P" --vnc >/dev/null 2>&1
  sleep 18
  local before=$(python3 "$HERE/wait_fetch.py" "$P" --read)
  # The first press after an install is usually swallowed while the app
  # settles, so a single SELECT is not a reliable way to force the fetch.
  # Retry until PKJS actually records one rather than assuming the press
  # landed.
  local attempt
  for attempt in 1 2 3 4; do
    pebble emu-button --emulator "$P" --vnc click select >/dev/null 2>&1
    if python3 "$HERE/wait_fetch.py" "$P" --after "$before" --timeout 35 \
         "${LOC_ARGS[@]}"; then
      sleep 5   # let the payload reach the watch and redraw
      return 0
    fi
    echo "relaunch: SELECT #$attempt did not produce a fetch; retrying" >&2
  done
  echo "relaunch: giving up on $P — refusing to capture mock data" >&2
  exit 1
}

# --- carousel -------------------------------------------------------------
relaunch
set_theme 0
shot "card_00"
for i in $(seq 1 $((NCARDS - 1))); do
  press down
  shot "card_$(printf '%02d' $i)"
done

# --- detail sheets --------------------------------------------------------
# 6 Hours is card 2, UV is card 5. Reinstall first so both are counted from a
# known Main, and shoot the approach frame too so a swallowed press is visible
# in the trail instead of silently mislabelling the sheet.
relaunch
set_theme 0
press down; press down
shot "approach_hours"
hold
shot "sheet_hours"
press back

relaunch
set_theme 0
press down; press down; press down; press down; press down
shot "approach_uv"
hold
shot "sheet_uv"
press back

# --- big mode -------------------------------------------------------------
# BigMode has no watch-side control; it is Clay-only via MESSAGE_KEY_BigMode.
# Setting it by app message avoids forcing it in C, which would move the
# locked pair.
relaunch
set_theme 0
pebble send-app-message --emulator "$P" --vnc --int 10129=1 >/dev/null 2>&1
sleep 4
shot "big_main"
press down; press down
shot "big_hours"
pebble send-app-message --emulator "$P" --vnc --int 10129=0 >/dev/null 2>&1
sleep 3

# --- dark theme -----------------------------------------------------------
relaunch
set_theme 1
shot "dark_main"
set_theme 0

echo "trail captured -> $OUT"
