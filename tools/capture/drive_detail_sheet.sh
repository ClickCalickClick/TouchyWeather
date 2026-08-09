#!/bin/zsh
# Detail-sheet demo. Assumes the emulator sits on the 6 Hours card, light theme.
#
# No theme-flip warm-up here: capture goes through the QEMU monitor's
# screendump, which is independent of the VNC adaptive refresh timer.
#
# Presses are scheduled in parallel from t=0 because a serial `pebble
# emu-button` costs ~1.6s of process startup; backgrounding overlaps that cost
# so presses land at the true relative offsets below (all shifted by ~1.6s).
set -u

at() {
  local t=$1; shift
  ( sleep "$t"; pebble emu-button --emulator emery --vnc click "$@" >/dev/null 2>&1 ) &
}

at 0.20 select --duration 800   # hold -> detail sheet slides up
at 2.60 select                  # rain-probability overlay on
at 4.60 select                  # overlay off
at 6.40 back                    # dismiss -> sheet slides down
wait
