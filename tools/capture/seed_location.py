#!/usr/bin/env python3
"""Seed a platform's pypkjs localStorage with a Location override.

The emulator's geolocation resolves the host's public IP through pypkjs 2.0.7's
bundled GeoLiteCity.dat (MaxMind GeoLite City, discontinued 2018). A modern IP
yields a country-level-only record, so it returns the country centroid — for DE
that is 51.0, 9.0, a field in central Germany. Every capture taken without an
override shows German weather.

`locationOverride` is the app's own supported Clay setting (index.js reads it
before calling navigator.geolocation), but it is only ever written from the
`webviewclosed` handler, so `pebble send-app-message` cannot reach it. pypkjs
persists localStorage as a dbm.dumb shelf under its --persist dir, so writing
the key there is equivalent to having saved it from the config page — and needs
no source change, which keeps the emery/gabbro lock untouched by construction.

The emulator must be STOPPED when this runs: pypkjs holds the shelf open and
flushes its own view on exit, which would clobber the write.
"""
import argparse
import dbm.dumb
import pathlib
import sys

SDK_ROOT = pathlib.Path.home() / "Library/Application Support/Pebble SDK"

# Keys that pin a previous fetch in place. lastFetchAt drives the 15-minute
# freshness gate; the geoCache pair pins the resolved place name. Dropping all
# three forces the next launch to do a real fetch against the new coordinates
# rather than re-rendering the cached German payload.
STALE_KEYS = [b"lastFetchAt", b"geoCacheCoords", b"geoCacheAt", b"lastLocationName"]


def store_path(platform):
    """Newest localstorage shelf for a platform, across every SDK tree.

    The persist root is NOT a single hardcoded version: this machine carries
    parallel trees (4.9.148, 4.9.169, 4.17) and which one a given platform's
    emulator uses varies. Seeding the wrong tree fails silently — the shelf is
    written, the emulator never reads it, and the capture proceeds against
    whatever geolocation returns. Always resolve by scanning and taking the
    most recently touched shelf.
    """
    candidates = []
    for version in SDK_ROOT.iterdir():
        ls = version / platform / "localstorage"
        if not ls.is_dir():
            continue
        for dat in ls.glob("*.dat"):
            candidates.append(dat)
    if not candidates:
        return None
    newest = max(candidates, key=lambda p: p.stat().st_mtime)
    return newest.with_suffix("")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("platform")
    ap.add_argument("--coords", required=True, help='"lat,lon"')
    ap.add_argument("--clear", action="store_true",
                    help="also drop the cached fetch/geocode keys")
    args = ap.parse_args()

    base = store_path(args.platform)
    if base is None:
        print(f"seed: no localstorage yet for {args.platform} — install once first",
              file=sys.stderr)
        return 1

    db = dbm.dumb.open(str(base), "w")
    db[b"locationOverride"] = args.coords.encode()
    if args.clear:
        for key in STALE_KEYS:
            if key in db:
                del db[key]
    written = db[b"locationOverride"].decode()
    remaining = sorted(k.decode() for k in db.keys())
    db.close()

    print(f"seed: {args.platform} locationOverride={written}")
    print(f"seed: keys now {remaining}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
