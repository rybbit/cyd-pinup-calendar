#!/usr/bin/env python3
"""Generate placeholder-art demo series (demo-dogs, demo-bikinis) to test the whole
pipeline (GitHub -> CYD download -> SD card -> display). They use their own series
ids so they never overwrite real calendars.

    python3 tools/make_demo_calendars.py [--year 2027] [--only dogs guns]

Replace an edition later by running build_calendar.py with the same --series/--year
and real pictures.
"""
import argparse
import math

from PIL import Image, ImageDraw, ImageFont

from build_calendar import CONTENT, H, W, ensure_series, rebuild_catalog, write_edition

MONTHS = ["January", "February", "March", "April", "May", "June",
          "July", "August", "September", "October", "November", "December"]

# series id, series title, category, edition title, colors
DEMOS = [
    ("demo-dogs", "Demo Dogs", "Demo", "Good Boys & Girls", [(245, 190, 80), (200, 90, 40)]),
    ("demo-bikinis", "Demo Bikinis", "Demo", "Sun, Sand & Swimsuits", [(90, 200, 230), (240, 110, 120)]),
]


def font(size):
    try:
        return ImageFont.truetype("DejaVuSans-Bold.ttf", size)
    except OSError:
        return ImageFont.load_default(size)


def poster(category, month, colors):
    img = Image.new("RGB", (W, H), colors[0])
    d = ImageDraw.Draw(img)
    cx, cy = W // 2, 120
    # retro sunburst, rotated a little each month
    for k in range(24):
        a0 = math.radians(k * 15 + month * 2.5)
        a1 = a0 + math.radians(7.5)
        r = 400
        d.polygon([(cx, cy), (cx + r * math.cos(a0), cy + r * math.sin(a0)),
                   (cx + r * math.cos(a1), cy + r * math.sin(a1))], fill=colors[1])
    d.ellipse([cx - 62, cy - 62, cx + 62, cy + 62], fill=(250, 243, 224), outline=(35, 30, 28), width=4)
    d.text((cx, cy - 10), category.upper(), font=font(22), fill=(196, 32, 38), anchor="mm")
    d.text((cx, cy + 18), f"{month:02d}", font=font(26), fill=(35, 30, 28), anchor="mm")
    d.rectangle([0, 250, W, H], fill=(35, 30, 28))
    d.text((cx, 268), f"MISS {MONTHS[month - 1].upper()}", font=font(18), fill=(255, 214, 90), anchor="mm")
    d.text((cx, 296), "placeholder - add your own art", font=font(11), fill=(250, 243, 224), anchor="mm")
    return img


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--year", type=int, default=2027)
    ap.add_argument("--only", nargs="+", help="series ids to generate (default: all)")
    a = ap.parse_args()
    for series_id, series_title, category, title, colors in DEMOS:
        if a.only and series_id not in a.only:
            continue
        print(f"{series_id}/{a.year}")
        root = CONTENT / "calendars" / series_id
        ensure_series(root, series_id, series_title, category)
        images = [poster(category, m, colors) for m in range(1, 13)]
        captions = [f"Miss {m}" for m in MONTHS]
        # demo art has its badge near the top, so show rows 30..199 on the calendar page
        write_edition(root, a.year, title, images, captions, "top", crops=[30] * 12)
    rebuild_catalog()


if __name__ == "__main__":
    main()
