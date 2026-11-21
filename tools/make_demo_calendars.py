#!/usr/bin/env python3
"""Generate placeholder Dogs / Guns / Bikinis calendars so the whole pipeline
(GitHub -> CYD download -> SD card -> display) can be tested before real art exists.

    python3 tools/make_demo_calendars.py [--year 2027]

Replace them later by running build_calendar.py with the same --id and real pictures.
"""
import argparse
import math

from PIL import Image, ImageDraw, ImageFont

from build_calendar import H, W, write_calendar

MONTHS = ["January", "February", "March", "April", "May", "June",
          "July", "August", "September", "October", "November", "December"]

DEMOS = [
    ("dogs", "Good Boys & Girls", "Dogs", [(245, 190, 80), (200, 90, 40)]),
    ("guns", "Iron & Walnut", "Guns", [(120, 140, 110), (50, 60, 45)]),
    ("bikinis", "Sun, Sand & Swimsuits", "Bikinis", [(90, 200, 230), (240, 110, 120)]),
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
    a = ap.parse_args()
    for slug, title, category, colors in DEMOS:
        cal_id = f"{slug}-{a.year}"
        print(cal_id)
        images = [poster(category, m, colors) for m in range(1, 13)]
        captions = [f"Miss {m}" for m in MONTHS]
        # demo art has its badge near the top, so show rows 30..199 on the calendar page
        write_calendar(cal_id, title, category, a.year, images, captions, "top", crops=[30] * 12)


if __name__ == "__main__":
    main()
