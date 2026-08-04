#!/usr/bin/env python3
"""Build the 1080x1920 promo compilations for TouchyWeather and TouchyWeather Face.

Watch screenshots are pixel art: they are upscaled by an INTEGER factor with
NEAREST only. The 2x factor is what fixes the grid at 2 columns -- a 3-column
grid would need ~1.65x, whose uneven pixel doubling is visible on this UI's
1px rules and small caps.
"""
import os
from PIL import Image, ImageDraw, ImageFont

APP = "/Users/jaredwuerzburger/Documents/GitHub/TouchyWeather"
FACE = "/Users/jaredwuerzburger/Documents/GitHub/TouchyWeather - Watchface"
BRAND = f"{FACE}/resources/fonts/ChakraPetch-Bold.ttf"
BODY = "/System/Library/Fonts/Supplemental/Arial.ttf"
BODY_B = "/System/Library/Fonts/Supplemental/Arial Bold.ttf"

W, H = 1080, 1920
BG = (13, 16, 21)
CARD_BG = (24, 28, 35)
TITLE_C = (255, 255, 255)
SUB_C = (150, 159, 172)
CAP_C = (196, 203, 213)
ACCENT = (255, 145, 25)
RULE = (44, 50, 60)

SCALE = 2
MARGIN = 90
GUTTER = 100
CAP_H = 52


def font(path, size):
    return ImageFont.truetype(path, size)


def rounded(img, radius, border):
    """Round the screen corners and stroke a hairline frame, like the real glass."""
    w, h = img.size
    mask = Image.new("L", (w, h), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, w - 1, h - 1], radius=radius, fill=255)
    out = Image.new("RGB", (w, h), CARD_BG)
    out.paste(img, (0, 0), mask)
    ImageDraw.Draw(out).rounded_rectangle([0, 0, w - 1, h - 1], radius=radius, outline=border, width=2)
    return out, mask


def build(out_path, title, subtitle, shots, footer):
    canvas = Image.new("RGB", (W, H), BG)
    dr = ImageDraw.Draw(canvas)

    f_title = font(BRAND, 66)
    f_sub = font(BODY, 30)
    f_cap = font(BRAND, 25)
    f_foot = font(BODY, 25)

    y = 78
    dr.text((MARGIN, y), title, font=f_title, fill=TITLE_C)
    y += 78
    dr.text((MARGIN, y), subtitle, font=f_sub, fill=SUB_C)
    y += 46
    dr.line([(MARGIN, y), (W - MARGIN, y)], fill=ACCENT, width=3)
    y += 40

    # Probe cell size from the first shot.
    probe = Image.open(shots[0][0])
    cw, ch = probe.width * SCALE, probe.height * SCALE
    grid_w = 2 * cw + GUTTER
    x0 = (W - grid_w) // 2

    rows = (len(shots) + 1) // 2
    # Reserve the footer block explicitly (pad + rule + lines + bottom margin);
    # a fixed guess clips the last line.
    footer_h = 26 + 2 + 20 + len(footer) * 34 + 34
    avail = H - y - footer_h
    row_h = ch + CAP_H
    gap = max(18, (avail - rows * row_h) // max(1, rows - 1))

    for i, (path, caption) in enumerate(shots):
        r, c = divmod(i, 2)
        im = Image.open(path).convert("RGB")
        im = im.resize((im.width * SCALE, im.height * SCALE), Image.NEAREST)
        im, _ = rounded(im, 10, RULE)
        cx = x0 + c * (cw + GUTTER)
        cy = y + r * (row_h + gap)
        canvas.paste(im, (cx, cy))
        tw = dr.textlength(caption, font=f_cap)
        dr.text((cx + (cw - tw) / 2, cy + ch + 15), caption, font=f_cap, fill=CAP_C)

    fy = y + rows * row_h + (rows - 1) * gap + 26
    dr.line([(MARGIN, fy), (W - MARGIN, fy)], fill=RULE, width=2)
    fy += 20
    for line in footer:
        dr.text((MARGIN, fy), line, font=f_foot, fill=SUB_C)
        fy += 34

    canvas.save(out_path)
    print(f"{out_path}  {canvas.size}  {os.path.getsize(out_path)//1024}KB")


A = f"{APP}/store/screenshots/emery"
F = f"{FACE}/store/screenshots/emery"

build(
    f"{APP}/store/video/promo_app.png",
    "TouchyWeather 2.0",
    "Twelve cards, live radar, and a forecast with opinions.",
    [
        (f"{A}/01-card-main.png", "MAIN"),
        (f"{A}/02-card-advice.png", "TOUCH & GO"),
        (f"{A}/13-detail-hours.png", "HOLD SELECT → CHARTS"),
        (f"{A}/11-card-radar.png", "LIVE RADAR"),
        (f"{A}/06-card-uv.png", "UV INDEX"),
        (f"{A}/15-big-mode-main.png", "BIG MODE"),
    ],
    [
        "Also: Week Ahead · Precipitation · Air Quality · Sun Cycle · Night Sky ·",
        "Golden Hour · on-watch card management. Shown on Pebble Time 2.",
    ],
)

build(
    f"{APP}/store/video/promo_face.png",
    "TouchyWeather Face",
    "A clock that grows to fit. Nudge for the forecast.",
    [
        (f"{F}/02-face-loaded.png", "ALL FOUR SLOTS"),
        (f"{F}/03-face-minimal.png", "STRIPPED BACK"),
        (f"{F}/08-peek-hours.png", "NUDGE → 6 HOURS"),
        (f"{F}/09-peek-week.png", "NUDGE → WEEK AHEAD"),
        (f"{F}/07-night-mode.png", "NIGHT MODE"),
        (f"{F}/12-overlay.png", "EVERYTHING OVERLAY"),
    ],
    [
        "Four complication slots · rain auto-peek · Quick View reflow ·",
        "wakes once a minute at rest. Shown on Pebble Time 2.",
    ],
)
