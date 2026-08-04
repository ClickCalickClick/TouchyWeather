#!/bin/zsh
# Capture and assemble a complete store set for one platform, end to end.
#
# usage: [EXPECT_LOC=Orlando] run_platform.sh <platform> <ncards> <coords> <scratch> [--radar]
#
# `pebble wipe` runs first, for one reason: the What's New modal is armed by a
# version bump and consumed by the first launch that shows it, so any emulator
# that has already opened 2.0.0 will silently give a Main-card screenshot where
# the release-notes screen was expected. Wiping re-arms it. Wipe is global and
# also clears every platform's localstorage, which is why the location seed has
# to come after it — and why a platform must be fully assembled before the next
# one starts.
set -eu
P=$1
NCARDS=$2
COORDS=$3
S=$4
RADAR=${5:-}
EXPECT_LOC=${EXPECT_LOC:-}
HERE=${0:A:h}

TRAIL="$S/trail_$P"
rm -rf "$TRAIL"
mkdir -p "$TRAIL"

echo "=== $P: clearing stale emulators"
pebble kill >/dev/null 2>&1 || true
# `pebble kill` does not always reap a wedged QEMU, and the survivor keeps the
# VNC port, which makes every later launch fail with "Address already in use".
for pid in $(lsof -ti :5901 2>/dev/null); do kill -9 "$pid" 2>/dev/null || true; done
sleep 3

echo "=== $P: wipe (re-arms What's New)"
pebble wipe >/dev/null 2>&1 || true

echo "=== $P: first launch — What's New + create localstorage"
pebble install --emulator "$P" --vnc >/dev/null 2>&1
sleep 18
# Kept in the trail rather than written straight to its final name so it lands
# on the contact sheet and gets eyeballed like every other frame.
pebble screenshot --emulator "$P" --vnc --no-open "$TRAIL/whatsnew.png" >/dev/null 2>&1

echo "=== $P: seeding $COORDS"
pebble kill >/dev/null 2>&1 || true
sleep 2
python3 "$HERE/seed_location.py" "$P" --coords "$COORDS" --clear

echo "=== $P: capturing trail"
"$HERE/capture_all.sh" "$P" "$TRAIL" "$NCARDS" "$EXPECT_LOC"

echo "=== $P: contact sheet"
python3 "$HERE/sheet.py" "$TRAIL" "$S/sheet_$P.png" --cols 6

echo "=== $P: assembling"
python3 "$HERE/assemble.py" "$P" "$TRAIL" "$S/out" \
  --whats-new "$TRAIL/whatsnew.png" ${RADAR:+--radar}
