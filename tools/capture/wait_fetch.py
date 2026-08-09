#!/usr/bin/env python3
"""Block until a platform's PKJS records a NEW completed weather fetch.

`sleep N` after an install proves only that the app is on screen, not that the
phone has fed it. A launch that renders before its payload arrives shows the
built-in mock in weather_data.c — TUE 78/58, WED 72/60, a 73%-lit moon and
hours_precip_x10 = {0,0,0,3,2,0}. That mock is stable, plausible and perfectly
distinct, so the md5 distinctness assertion passes on a set that contains no
real weather at all. This is the gate that catches it.

pypkjs writes `lastFetchAt` to its dbm.dumb shelf as each fetch completes and
flushes live, so the value is readable from outside the process while the
emulator runs.

  usage: wait_fetch.py <platform> --after <epoch_ms> [--timeout 60]
         wait_fetch.py <platform> --read
"""
import argparse
import glob
import pathlib
import sys
import time

SDK_ROOT = pathlib.Path.home() / "Library/Application Support/Pebble SDK"


def store_base(platform):
    """Newest localstorage shelf for a platform, across every SDK tree.

    Parallel SDK trees (4.9.148, 4.9.169, 4.17) each carry their own persist
    dirs and platforms do not consistently use the newest one, so the path has
    to be discovered rather than assumed.
    """
    candidates = []
    for version in SDK_ROOT.iterdir():
        ls = version / platform / "localstorage"
        if ls.is_dir():
            candidates.extend(ls.glob("*.dat"))
    if not candidates:
        return None
    return str(max(candidates, key=lambda p: p.stat().st_mtime))[:-4]


def read_keys(platform, *keys):
    import dbm.dumb
    base = store_base(platform)
    if base is None:
        return {}
    try:
        db = dbm.dumb.open(base, "r")
    except Exception:
        return {}
    try:
        return {k: db.get(k.encode()) for k in keys}
    finally:
        db.close()


def read_last_fetch(platform):
    raw = read_keys(platform, "lastFetchAt").get("lastFetchAt")
    return int(raw) if raw else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("platform")
    ap.add_argument("--after", type=int, default=None)
    ap.add_argument("--read", action="store_true")
    ap.add_argument("--timeout", type=float, default=60.0)
    ap.add_argument("--expect-location", default=None,
                    help="also assert PKJS resolved this place name")
    args = ap.parse_args()

    if args.read:
        print(read_last_fetch(args.platform) or 0)
        return 0

    if args.after is None:
        print("wait_fetch: --after is required unless --read", file=sys.stderr)
        return 2

    deadline = time.monotonic() + args.timeout
    while time.monotonic() < deadline:
        current = read_last_fetch(args.platform)
        if current and current > args.after:
            # A completed fetch proves the payload is real, not the mock. It
            # does NOT prove it is for the intended place: if the seed landed
            # in the wrong SDK tree the override is simply absent and PKJS
            # geolocates instead, which still produces a perfectly valid
            # fetch of the wrong city.
            if args.expect_location:
                got = read_keys(args.platform, "lastLocationName").get("lastLocationName")
                got = got.decode(errors="replace") if got else "(unset)"
                if got != args.expect_location:
                    print(f"wait_fetch: WRONG LOCATION — expected "
                          f"{args.expect_location!r}, PKJS resolved {got!r}. "
                          f"The override is not in effect; do not capture.",
                          file=sys.stderr)
                    return 1
                print(f"wait_fetch: {args.platform} fetched at {current} "
                      f"(+{current - args.after}ms) for {got}")
                return 0
            print(f"wait_fetch: {args.platform} fetched at {current} "
                  f"(+{current - args.after}ms)")
            return 0
        time.sleep(1.0)

    print(f"wait_fetch: TIMEOUT after {args.timeout}s — {args.platform} never "
          f"recorded a fetch newer than {args.after}. The screen is showing "
          f"cached or MOCK data; do not capture it.", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
