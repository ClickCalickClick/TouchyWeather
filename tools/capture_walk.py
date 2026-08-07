#!/usr/bin/env python3
"""Drive an emulator through the card carousel and its detail sheets, capturing
every step, then assert the shots are distinct.

    python3 tools/capture_walk.py <platform> <outdir>

Written for round B (Big Mode). Kept in-tree because it encodes four emulator
traps that each cost a real debugging run — re-deriving them is the expensive
part, not the script.

TRAPS THIS SCRIPT ENCODES
-------------------------
1. TWO installs before the walk. On an emulator that has never run this build,
   the What's New modal is armed; it SWALLOWS every early button press, and the
   `back` that follows exits the watchapp to the Timeline ("No events"). A whole
   14-shot emery set came back as notes screens. update_notes_maybe_show()
   persists the seen-version the instant it displays, so the first launch
   disarms it and the second starts on card 0.

2. sleep 18 after an install, not 2-5. The relaunch needs 12-14s to render;
   anything shorter screenshots the PREVIOUS state and yields a whole capture
   set of a stale card.

3. Screenshot EVERY step and read the walk off the pixels. The first `down`
   after an install is sometimes swallowed and sometimes not — the same script
   landed on different cards on two consecutive runs. Never count presses.

4. Assert distinctness in PYTHON, not shell. A shell `N=$(ls $SET | wc -l)`
   where $SET fails to word-split gives N=0 unique=0, which "passes" vacuously.
   This is the only check that has ever caught a stale-capture run.

Expected duplicates, which are correctness signals rather than failures:
  __prime == 00_main          (both are the Main card, before and after install 2)
  hours page 1 == page 1 again (paging away and back is pixel-identical)

Not a general-purpose tool: the walk below is this app's card order. The
watchface repo needs its own walk, but the scaffolding above is the reusable
part.
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

# 12-14s to relaunch and render. See trap 2.
SETTLE_AFTER_INSTALL = 18


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


print("== install on %s (twice — see trap 1) ==" % PLATFORM)
install()
shot("__prime")
install()

shot("00_main")
press("down")
shot("01_card_advice")
press("down")
shot("02_hours_page1")

print("== hours paging (Big Mode only; SELECT toggles theme otherwise) ==")
press("select")
shot("03_hours_page2")
press("select")
shot("04_hours_page1_again")

print("== detail sheets (long-press SELECT; `back` dismisses the modal) ==")
for idx, (card, sheet) in enumerate([
        (None, "05_sheet_hours"),
        ("06_card_week", "07_sheet_week"),
        ("08_card_precip", "09_sheet_precip"),
        ("10_card_uv", "11_sheet_uv"),
        ("12_card_aq", "13_sheet_aq")]):
    if card:
        press("down")
        shot(card)
    press("select", duration=800, settle=2.0)
    shot(sheet)
    press("back", settle=1.6)

# Trap 4 — distinctness, in Python.
files = sorted(f for f in os.listdir(OUT) if f.endswith(".png"))
digests = {}
for f in files:
    with open(os.path.join(OUT, f), "rb") as fh:
        digests.setdefault(hashlib.md5(fh.read()).hexdigest(), []).append(f)
print("\n== %d shots, %d unique ==" % (len(files), len(digests)))
dupes = [names for names in digests.values() if len(names) > 1]
for names in dupes:
    print("  DUPLICATE: %s" % ", ".join(names))
if not dupes:
    print("  all distinct")
