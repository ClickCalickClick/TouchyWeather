#!/usr/bin/env python3
"""Map a capture trail onto the store's NN-name.png filenames.

The trail is deliberately captured with generic positional names (card_07) and
renamed here, so that identifying which card is which is a reviewing step over
a contact sheet rather than an assumption baked into the button-pressing.

Radar sits between Golden Hour and Settings, and exists only on the 128KB
platforms (emery, gabbro) — settings.h carves it out everywhere else, so on the
small platforms Settings is one card earlier.
"""
import argparse
import hashlib
import pathlib
import shutil
import sys

CARDS = ["01-card-main", "02-card-advice", "03-card-hours", "04-card-week",
         "05-card-precip", "06-card-uv", "07-card-aq", "08-card-sun",
         "09-card-night", "10-card-golden"]

EXTRAS = {"sheet_hours": "13-detail-hours", "sheet_uv": "14-detail-uv",
          "big_main": "15-big-mode-main", "big_hours": "16-big-mode-hours",
          "dark_main": "17-dark-main"}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("platform")
    ap.add_argument("trail")
    ap.add_argument("dest")
    ap.add_argument("--radar", action="store_true")
    ap.add_argument("--whats-new", default=None,
                    help="path to a separately captured 00-whats-new.png")
    args = ap.parse_args()

    trail = pathlib.Path(args.trail)
    dest = pathlib.Path(args.dest) / args.platform
    dest.mkdir(parents=True, exist_ok=True)

    mapping = {f"card_{i:02d}": name for i, name in enumerate(CARDS)}
    nxt = len(CARDS)
    if args.radar:
        mapping[f"card_{nxt:02d}"] = "11-card-radar"
        nxt += 1
    mapping[f"card_{nxt:02d}"] = "12-card-settings"
    mapping.update(EXTRAS)

    written, missing = [], []
    for src_stem, out_name in sorted(mapping.items(), key=lambda kv: kv[1]):
        src = trail / f"{src_stem}.png"
        if not src.exists():
            missing.append(src_stem)
            continue
        shutil.copyfile(src, dest / f"{out_name}.png")
        written.append(out_name)

    if args.whats_new:
        wn = pathlib.Path(args.whats_new)
        if wn.exists():
            shutil.copyfile(wn, dest / "00-whats-new.png")
            written.append("00-whats-new")
        else:
            missing.append("00-whats-new")

    digests = {}
    for name in written:
        path = dest / f"{name}.png"
        digests.setdefault(hashlib.md5(path.read_bytes()).hexdigest(), []).append(name)

    print(f"assemble: {args.platform} -> {len(written)} files in {dest}")
    if missing:
        print(f"  MISSING: {', '.join(sorted(missing))}")
    dupes = {d: g for d, g in digests.items() if len(g) > 1}
    for d, group in dupes.items():
        print(f"  DUPLICATE {d[:8]}: {', '.join(sorted(group))}")
    if dupes:
        print("FAIL: identical frames in the assembled set")
        return 1
    print(f"OK: {len(written)} files, all distinct")
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
