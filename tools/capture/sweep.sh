#!/bin/zsh
# Walk the carousel with single DOWN presses, screenshotting after each.
#
# `pebble emu-button --repeat N` silently drops presses, so every press is its
# own click call. The first DOWN after an install is usually swallowed while the
# app settles, which is why the resulting frames are identified by LOOKING at
# them rather than by counting presses.
#
# usage: sweep.sh <platform> <outdir> <count>
set -u
PLATFORM=$1
OUT=$2
COUNT=$3
mkdir -p "$OUT"

pebble screenshot --emulator "$PLATFORM" --vnc --no-open "$OUT/seq_00.png" >/dev/null 2>&1
for i in $(seq 1 "$COUNT"); do
  pebble emu-button --emulator "$PLATFORM" --vnc click down >/dev/null 2>&1
  sleep 2.5
  pebble screenshot --emulator "$PLATFORM" --vnc --no-open "$OUT/seq_$(printf '%02d' $i).png" >/dev/null 2>&1
done
echo "swept $COUNT presses -> $OUT"
