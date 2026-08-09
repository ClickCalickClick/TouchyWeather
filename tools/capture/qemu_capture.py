#!/usr/bin/env python3
"""Capture the QEMU display at a fixed frame rate via the monitor's screendump.

The VNC route works but is governed by QEMU's *adaptive* refresh timer, which
idles up to ~2s and then coalesces a whole animation into one update. The
monitor's screendump grabs the current surface on demand (~390/s measured), so
it is independent of that timer and yields genuine constant-rate frames.
"""
import argparse, json, os, socket, time


class Monitor:
    def __init__(self, port, host="127.0.0.1"):
        self.s = socket.create_connection((host, port), timeout=5)
        self.s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        self.s.settimeout(2)
        time.sleep(0.3)
        try:
            self.s.recv(65536)
        except socket.timeout:
            pass

    def cmd(self, c, wait=2.0):
        self.s.sendall((c + "\n").encode())
        buf = b""
        t0 = time.monotonic()
        while time.monotonic() - t0 < wait:
            try:
                chunk = self.s.recv(65536)
            except socket.timeout:
                break
            if not chunk:
                break
            buf += chunk
            if buf.rstrip().endswith(b"(qemu)"):
                break
        return buf

    def close(self):
        self.s.close()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--fps", type=float, default=50.0)
    ap.add_argument("--duration", type=float, default=10.0)
    args = ap.parse_args()

    out = os.path.abspath(args.out)
    os.makedirs(out, exist_ok=True)
    mon = Monitor(args.port)

    period = 1.0 / args.fps
    total = int(args.duration * args.fps)
    stamps = []
    start = time.monotonic()
    for i in range(total):
        target = start + i * period
        now = time.monotonic()
        if target > now:
            time.sleep(target - now)
        mon.cmd(f"screendump {out}/f{i:05d}.ppm", wait=0.5)
        stamps.append(round(time.monotonic() - start, 4))
    elapsed = time.monotonic() - start
    mon.close()

    with open(os.path.join(out, "stamps.json"), "w") as fh:
        json.dump({"fps": args.fps, "duration": elapsed, "stamps": stamps}, fh)

    drift = [round(stamps[i] - i * period, 3) for i in range(0, len(stamps), max(1, len(stamps) // 5))]
    print(f"{total} frames in {elapsed:.2f}s (target {args.duration}s) "
          f"-> effective {total/elapsed:.1f} fps; drift samples {drift}", flush=True)


if __name__ == "__main__":
    main()
