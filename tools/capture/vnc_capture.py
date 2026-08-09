#!/usr/bin/env python3
"""Minimal RFB 3.x client that records the QEMU framebuffer to PNG frames.

QEMU's VNC server only pushes an update when pixels actually change, so we
capture on change and stamp each frame with a monotonic timestamp. The
assembler resamples that irregular timeline to constant fps.
"""
import argparse, json, os, socket, struct, sys, time
from PIL import Image

RAW = 0


def recv_exact(sock, n):
    buf = bytearray()
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            raise ConnectionError(f"socket closed with {n - len(buf)} bytes outstanding")
        buf += chunk
    return bytes(buf)


def handshake(sock):
    version = recv_exact(sock, 12)
    if not version.startswith(b"RFB "):
        raise RuntimeError(f"not an RFB server: {version!r}")
    major, minor = int(version[4:7]), int(version[8:11])
    sock.sendall(version)

    if (major, minor) >= (3, 7):
        count = recv_exact(sock, 1)[0]
        if count == 0:
            reason_len = struct.unpack(">I", recv_exact(sock, 4))[0]
            raise RuntimeError(recv_exact(sock, reason_len).decode(errors="replace"))
        types = recv_exact(sock, count)
        if 1 not in types:
            raise RuntimeError(f"server needs auth; offered types {list(types)}")
        sock.sendall(bytes([1]))
    else:
        sec = struct.unpack(">I", recv_exact(sock, 4))[0]
        if sec != 1:
            raise RuntimeError(f"server needs auth (type {sec})")

    # SecurityResult is sent for 3.8+, and for 3.7 only on failure paths we
    # never reach with security type None.
    if (major, minor) >= (3, 8):
        if struct.unpack(">I", recv_exact(sock, 4))[0] != 0:
            raise RuntimeError("security handshake rejected")

    sock.sendall(bytes([1]))  # ClientInit, shared=1
    w, h = struct.unpack(">HH", recv_exact(sock, 4))
    recv_exact(sock, 16)  # server pixel format, replaced below
    name_len = struct.unpack(">I", recv_exact(sock, 4))[0]
    name = recv_exact(sock, name_len).decode(errors="replace")
    return w, h, name


def set_pixel_format(sock):
    # 32bpp true colour, shifts R=16 G=8 B=0, little-endian -> bytes are BGRX.
    fmt = struct.pack(">BBBBHHHBBB3x", 32, 24, 0, 1, 255, 255, 255, 16, 8, 0)
    sock.sendall(struct.pack(">B3x", 0) + fmt)


def set_encodings(sock, encodings=(RAW,)):
    msg = struct.pack(">BxH", 2, len(encodings))
    for enc in encodings:
        msg += struct.pack(">i", enc)
    sock.sendall(msg)


def request_update(sock, w, h, incremental):
    sock.sendall(struct.pack(">BBHHHH", 3, 1 if incremental else 0, 0, 0, w, h))


def read_update(sock, fb, fbw):
    """Read one server message; apply Raw rects to fb. Returns True if pixels moved."""
    msg_type = recv_exact(sock, 1)[0]
    if msg_type != 0:
        if msg_type == 2:  # Bell
            return False
        if msg_type == 3:  # ServerCutText
            recv_exact(sock, 3)
            n = struct.unpack(">I", recv_exact(sock, 4))[0]
            recv_exact(sock, n)
            return False
        raise RuntimeError(f"unexpected server message type {msg_type}")

    recv_exact(sock, 1)
    (nrects,) = struct.unpack(">H", recv_exact(sock, 2))
    changed = False
    for _ in range(nrects):
        x, y, rw, rh, enc = struct.unpack(">HHHHi", recv_exact(sock, 12))
        if enc != RAW:
            raise RuntimeError(f"server used encoding {enc}; only Raw was negotiated")
        if rw == 0 or rh == 0:
            continue
        data = recv_exact(sock, rw * rh * 4)
        stride = fbw * 4
        for row in range(rh):
            off = (y + row) * stride + x * 4
            fb[off:off + rw * 4] = data[row * rw * 4:(row + 1) * rw * 4]
        changed = True
    return changed


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, required=True)
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--out", required=True)
    ap.add_argument("--duration", type=float, default=20.0)
    args = ap.parse_args()

    os.makedirs(args.out, exist_ok=True)
    sock = socket.create_connection((args.host, args.port), timeout=10)
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    w, h, name = handshake(sock)
    print(f"connected: {w}x{h} '{name}'", flush=True)
    set_pixel_format(sock)
    set_encodings(sock)

    fb = bytearray(w * h * 4)
    # Buffer raw frames in RAM; PNG encoding inside the loop adds enough latency
    # that QEMU's adaptive VNC refresh never ramps down to its 30ms floor.
    captured = []
    start = time.monotonic()
    request_update(sock, w, h, incremental=False)

    while time.monotonic() - start < args.duration:
        sock.settimeout(max(0.05, args.duration - (time.monotonic() - start)))
        try:
            changed = read_update(sock, fb, w)
        except socket.timeout:
            break
        if changed:
            captured.append((time.monotonic() - start, bytes(fb)))
        request_update(sock, w, h, incremental=True)

    frames = []
    for idx, (ts, raw) in enumerate(captured):
        img = Image.frombytes("RGB", (w, h), raw, "raw", "BGRX")
        path = os.path.join(args.out, f"frame_{idx:05d}.png")
        img.save(path)
        frames.append({"idx": idx, "t": round(ts, 4), "file": os.path.basename(path)})

    with open(os.path.join(args.out, "frames.json"), "w") as fh:
        json.dump({"width": w, "height": h, "duration": args.duration, "frames": frames}, fh, indent=1)

    gaps = [round(b["t"] - a["t"], 3) for a, b in zip(frames, frames[1:])]
    if gaps:
        busy = sorted(gaps)[: max(1, len(gaps) // 2)]
        print(f"inter-frame gap: min={min(gaps)}s median={sorted(gaps)[len(gaps)//2]}s "
              f"fastest-half-mean={sum(busy)/len(busy):.3f}s", flush=True)
    print(f"captured {len(frames)} frames over {args.duration}s -> {args.out}", flush=True)


if __name__ == "__main__":
    sys.exit(main())
