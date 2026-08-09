#!/usr/bin/env python3
"""Walk every card and detail sheet in the NO-READING state and capture it.

    # in src/pkjs/index.js set CAPTURE_NO_DATA = true, then:
    pebble build && python3 tools/capture_nodata.py <platform> <outdir>

This is the proof for the placeholder work: before the first reading ever
lands, no card may show a number. With CAPTURE_NO_DATA the phone never answers,
so the app holds that state indefinitely instead of leaving it ~1s after launch
— which is the only reason it can be photographed at all.

Shares the emulator traps documented in capture_walk.py (two installs, sleep 18,
read the walk off the pixels, assert in Python). What differs is the assertion.

WHAT DISTINCTNESS MEANS HERE
----------------------------
Every card draws the SAME panel, so the usual "all shots unique" check is the
wrong one. What must hold instead:

  * the 11 card shots are distinct from each other — each carries its own header
    ("6 HOURS", "WEEK AHEAD", ...), and the Main card has no header at all. If
    two card shots match, the walk did not advance and the set is stale.
  * the detail sheets are EXPECTED to be identical to each other: a sheet draws
    no header in this state, so all five are the same panel on the same surface.
    That is a correctness signal, not a stale capture.

So this script asserts distinctness within the cards and reports the sheets
separately rather than failing on them.
"""
import hashlib
import os
import subprocess
import sys
import time

if len(sys.argv) < 3:
    sys.exit(__doc__)

PLATFORM = sys.argv[1]
OUT = sys.argv[2]
PROJ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.makedirs(OUT, exist_ok=True)

SETTLE_AFTER_INSTALL = 18

# Carousel order (CLAUDE.md). Radar is registered but runtime-disabled.
# `sheet` marks the cards whose long-press opens a detail modal.
CARDS = [
    ("00_main", False),
    ("01_advice", False),
    ("02_hours", True),
    ("03_week", True),
    ("04_precip", True),
    ("05_uv", True),
    ("06_aq", True),
    ("07_sun", False),
    ("08_night", False),
    ("09_golden", False),
    ("10_settings", False),
]


def run(args, timeout=300):
    return subprocess.run(args, cwd=PROJ, capture_output=True, text=True,
                          timeout=timeout)


def shot(name):
    path = os.path.join(OUT, name + ".png")
    if os.path.exists(path):
        os.remove(path)
    r = run(["pebble", "screenshot", "--emulator", PLATFORM, "--no-open", path])
    if not os.path.exists(path):
        print("  !! screenshot failed for %s: %s" % (name, r.stderr.strip()[:200]))
        return None
    print("  captured %s" % name)
    return path


def press(button, duration=None, settle=1.4):
    args = ["pebble", "emu-button", "--emulator", PLATFORM, "click", button]
    if duration:
        args += ["--duration", str(duration)]
    run(args)
    time.sleep(settle)


def install():
    run(["pebble", "install", "--emulator", PLATFORM])
    time.sleep(SETTLE_AFTER_INSTALL)


print("== install on %s (twice — disarms the What's New modal) ==" % PLATFORM)
install()
install()

card_shots, sheet_shots = [], []
for i, (name, has_sheet) in enumerate(CARDS):
    if i:
        press("down")
    shot(name)
    card_shots.append(name + ".png")
    if has_sheet:
        # SELECT is the theme toggle on ordinary cards; only the LONG press
        # opens a sheet, so this never flips the theme mid-walk.
        press("select", duration=800, settle=2.0)
        shot(name + "_sheet")
        sheet_shots.append(name + "_sheet.png")
        press("back", settle=1.6)


def digest(fname):
    with open(os.path.join(OUT, fname), "rb") as fh:
        return hashlib.md5(fh.read()).hexdigest()


print("\n== cards: %d shots ==" % len(card_shots))
seen = {}
for f in card_shots:
    if os.path.exists(os.path.join(OUT, f)):
        seen.setdefault(digest(f), []).append(f)
dupes = [v for v in seen.values() if len(v) > 1]
print("  %d unique" % len(seen))
for names in dupes:
    print("  STALE? duplicate cards: %s" % ", ".join(names))
if not dupes:
    print("  all card shots distinct — the walk advanced")

print("\n== sheets: %d shots ==" % len(sheet_shots))
sheet_seen = {}
for f in sheet_shots:
    if os.path.exists(os.path.join(OUT, f)):
        sheet_seen.setdefault(digest(f), []).append(f)
print("  %d unique (1 expected — sheets draw no header in this state)"
      % len(sheet_seen))
for d, names in sheet_seen.items():
    print("    %s: %s" % (d[:8], ", ".join(names)))

sys.exit(1 if dupes else 0)
