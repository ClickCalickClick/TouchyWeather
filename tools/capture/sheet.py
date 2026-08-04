#!/usr/bin/env python3
"""Contact-sheet a capture directory and report distinctness.

No ImageMagick on this machine; PIL is available and a contact sheet is by far
the fastest way to review a set. Every frame is labelled with its filename and
a short md5, and the unique count is printed against the frame count — the
stale-capture assertion that has caught more bad runs than anything else.

Written in Python rather than shell on purpose: a shell `N=$(ls $SET | wc -l)`
where $SET fails to word-split yields N=0 unique=0, which "passes" vacuously.
"""
import argparse
import glob
import hashlib
import os
import sys

from PIL import Image, ImageDraw


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("indir")
    ap.add_argument("out")
    ap.add_argument("--cols", type=int, default=5)
    ap.add_argument("--expect", type=int, default=None,
                    help="fail unless exactly this many frames, all distinct")
    args = ap.parse_args()

    files = sorted(glob.glob(os.path.join(args.indir, "*.png")))
    if not files:
        print(f"sheet: no PNGs in {args.indir}", file=sys.stderr)
        return 1

    digests = [hashlib.md5(open(f, "rb").read()).hexdigest() for f in files]
    ims = [Image.open(f).convert("RGB") for f in files]
    w = max(i.size[0] for i in ims)
    h = max(i.size[1] for i in ims)
    cols = args.cols
    rows = (len(ims) + cols - 1) // cols
    pad, label = 16, 14

    sheet = Image.new("RGB", (cols * (w + pad) + pad, rows * (h + pad + label) + pad), (30, 30, 30))
    draw = ImageDraw.Draw(sheet)
    for idx, (path, im, dg) in enumerate(zip(files, ims, digests)):
        x = pad + (idx % cols) * (w + pad)
        y = pad + (idx // cols) * (h + pad + label)
        sheet.paste(im, (x, y))
        draw.text((x, y + h + 2), f"{os.path.basename(path)[:22]} {dg[:6]}", fill=(230, 230, 230))
    sheet.save(args.out)

    n, uniq = len(files), len(set(digests))
    print(f"sheet: {n} frames, {uniq} unique -> {args.out}")

    dupes = {}
    for path, dg in zip(files, digests):
        dupes.setdefault(dg, []).append(os.path.basename(path))
    for dg, group in dupes.items():
        if len(group) > 1:
            print(f"  DUPLICATE {dg[:8]}: {', '.join(group)}")

    if args.expect is not None:
        if n != args.expect:
            print(f"FAIL: expected {args.expect} frames, got {n}")
            return 1
        if uniq != args.expect:
            print(f"FAIL: expected {args.expect} distinct, got {uniq}")
            return 1
        print(f"OK: {args.expect} frames, all distinct")
    return 0


if __name__ == "__main__":
    sys.exit(main())
