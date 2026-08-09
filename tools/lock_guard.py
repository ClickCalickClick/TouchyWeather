#!/usr/bin/env python3
"""Prove that the emery and gabbro binaries have not changed.

Those two platforms are LOCKED: the small-screen remediation work
(basalt / chalk / diorite / flint) must never alter what they render. The
strongest available proof is not a screenshot diff but a byte compare of the
shipped binary — if `build/<platform>/pebble-app.bin` is unchanged, the
rendering is unchanged by construction, with no emulator, no animation
nondeterminism and no flaky pixels.

Three fields in that binary are not a function of the emitted code, and all
three are masked before hashing:

  * `resource_timestamp` — uint32 at 0x7c of the PebbleProcessInfo header,
    stamped with wall-clock time. Two clean rebuilds of an identical tree
    differ here and nowhere else.
  * the **GNU build-id** — a 20-byte SHA-1 in `.note.gnu.build-id`, computed
    over the set of linked objects. Adding ANY object to the link changes it,
    including one that compiles to zero bytes of .text/.data/.bss. This is what
    made `ui_layout.c` look like it moved the locked binaries when it had in
    fact emitted nothing at all.
  * `crc` — uint32 at 0x14, a checksum OF the payload, so it necessarily moves
    when the build-id does. Masking it costs nothing: the whole payload is
    still compared byte for byte, so a real code change is caught by the
    payload itself rather than by its checksum.

Verified: with these three masked, a pristine tree and a tree carrying a
fully `#if`'d-out new translation unit differ in ZERO bytes on both locked
platforms.

Usage:
    pebble clean && pebble build && python3 tools/lock_guard.py --save
    pebble clean && pebble build && python3 tools/lock_guard.py

ALWAYS clean-build before measuring. An incremental `pebble build` can keep
linking the stale object of a source file you have since moved or deleted,
which produces both false passes and false failures.

Exit status is 0 when both locked platforms match, 1 otherwise.

If a change legitimately needs to alter the locked binaries — it almost never
should — re-baseline deliberately with --save and say so in the commit message.
"""

import argparse
import hashlib
import pathlib
import sys

# PebbleProcessInfo header fields that are not a function of the emitted code.
CRC_OFFSET = 0x14
CRC_SIZE = 4
TIMESTAMP_OFFSET = 0x7C
TIMESTAMP_SIZE = 4

# ELF note header preceding the GNU build-id: namesz=4, descsz=20, type=3,
# then the name "GNU\0". The 20-byte description that follows is the build-id.
# Located by search rather than hardcoded offset, so it survives any shift in
# the binary's layout.
BUILD_ID_NOTE_HEADER = bytes.fromhex("04000000" "14000000" "03000000") + b"GNU\0"
BUILD_ID_SIZE = 20

LOCKED = ("emery", "gabbro")
SMALL = ("basalt", "chalk", "diorite", "flint")

ROOT = pathlib.Path(__file__).resolve().parent.parent
BASELINE = ROOT / "tools" / "lock_baseline.txt"


def masked_digest(platform):
    """sha256 of the platform binary with crc, timestamp and build-id zeroed."""
    path = ROOT / "build" / platform / "pebble-app.bin"
    if not path.exists():
        return None
    data = bytearray(path.read_bytes())
    data[CRC_OFFSET:CRC_OFFSET + CRC_SIZE] = b"\0" * CRC_SIZE
    data[TIMESTAMP_OFFSET:TIMESTAMP_OFFSET + TIMESTAMP_SIZE] = b"\0" * TIMESTAMP_SIZE
    note = data.find(BUILD_ID_NOTE_HEADER)
    if note != -1:
        start = note + len(BUILD_ID_NOTE_HEADER)
        data[start:start + BUILD_ID_SIZE] = b"\0" * BUILD_ID_SIZE
    return hashlib.sha256(bytes(data)).hexdigest()


def read_baseline():
    if not BASELINE.exists():
        return {}
    out = {}
    for line in BASELINE.read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        digest, platform = line.split()
        out[platform] = digest
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--save", action="store_true",
                    help="record the current binaries as the baseline")
    args = ap.parse_args()

    missing = [p for p in LOCKED if masked_digest(p) is None]
    if missing:
        print(f"lock_guard: no build output for {', '.join(missing)} "
              f"— run `pebble build` first", file=sys.stderr)
        return 1

    if args.save:
        lines = ["# Masked sha256 of the LOCKED platform binaries.",
                 "# Regenerate deliberately (tools/lock_guard.py --save) and say so in the commit.",
                 ""]
        for platform in LOCKED:
            lines.append(f"{masked_digest(platform)}  {platform}")
        BASELINE.write_text("\n".join(lines) + "\n")
        print(f"lock_guard: baseline written to {BASELINE.relative_to(ROOT)}")
        for platform in LOCKED:
            print(f"  {platform:8s} {masked_digest(platform)}")
        return 0

    baseline = read_baseline()
    if not baseline:
        print("lock_guard: no baseline — run with --save first", file=sys.stderr)
        return 1

    failed = []
    for platform in LOCKED:
        want = baseline.get(platform)
        got = masked_digest(platform)
        if want is None:
            print(f"  {platform:8s} NO BASELINE")
            failed.append(platform)
        elif want == got:
            print(f"  {platform:8s} ok")
        else:
            print(f"  {platform:8s} CHANGED")
            print(f"           expected {want}")
            print(f"           got      {got}")
            failed.append(platform)

    # The small classes are expected to change — report them so the run shows
    # that the build actually did something, and so an accidental no-op edit is
    # visible rather than silently passing.
    print()
    for platform in SMALL:
        digest = masked_digest(platform)
        print(f"  {platform:8s} {digest[:16] if digest else '(not built)'}   (not locked)")

    print()
    if failed:
        print(f"LOCK VIOLATED: {', '.join(failed)} changed. Revert or re-scope the edit.")
        return 1
    print("LOCK HELD: emery and gabbro are byte-identical.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
