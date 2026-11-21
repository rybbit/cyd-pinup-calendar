#!/usr/bin/env python3
"""Build pin-up calendar editions for the CYD.

A calendar is a SERIES (e.g. "bikinis") with one EDITION per year:

    content/calendars/<series>/series.json          series title + category
    content/calendars/<series>/placeholder.jpg      optional, shown for years with no edition
    content/calendars/<series>/<year>/01.jpg..12.jpg + calendar.json

Add a year's edition (12 images, sorted by filename = Jan..Dec):

    python3 tools/build_calendar.py --series bikinis --year 2028 \\
        --title "Beach Party" --images ~/Pictures/bikinis-2028/ \\
        [--captions captions.txt]          # up to 12 lines ("Miss January - Betty")
        [--focus top|center|bottom]        # crop position if the aspect ratio differs
        [--series-title "Bikini Pin-Ups" --category Bikinis]   # needed for a new series

Set a series placeholder picture:

    python3 tools/build_calendar.py --series bikinis --placeholder art.png

Write a CUSTOM calendar straight to an SD card (shown first on the CYD, never
updated from GitHub) by adding --sd <card mount point> to either command:

    python3 tools/build_calendar.py --sd /media/$USER/CARD --series smith-family \\
        --series-title "Smith Family" --year 2027 --title "Smith Family 2027" \\
        --images ~/Pictures/family/

Without --sd, content/catalog.json is rebuilt; commit and push content/ and every
CYD picks the change up. Every edition gets a "version" and each month a "sha"
fingerprint, so the CYD downloads only what changed. After hand-editing a JSON file
or swapping a jpg, run  python3 tools/build_calendar.py --restamp

Each image becomes a 240x320 baseline JPEG (the ESP32 decoder cannot read progressive
JPEGs). The calendar page shows a 240x170 window of it; per-month "crop_y" (0..150)
in calendar.json picks which rows show. Requires Pillow.
"""
import argparse
import hashlib
import json
import re
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageOps

W, H = 240, 320
VIEW_H = 170
MAX_BYTES = 120 * 1024
EXTS = {".jpg", ".jpeg", ".png", ".webp", ".bmp", ".gif", ".tif", ".tiff"}
CONTENT = Path(__file__).resolve().parent.parent / "content"
ID_RE = re.compile(r"[A-Za-z0-9_-]{1,40}")


def fit(img: Image.Image, focus: str) -> Image.Image:
    img = ImageOps.exif_transpose(img).convert("RGB")
    if img.size == (W, H):
        return img
    centering = {"top": (0.5, 0.15), "center": (0.5, 0.5), "bottom": (0.5, 0.85)}[focus]
    return ImageOps.fit(img, (W, H), Image.LANCZOS, centering=centering)


def frame(img: Image.Image) -> Image.Image:
    """Whole picture, uncropped, as a bordered print on a retro sunburst.

    For landscape or odd-shaped photos (e.g. product shots of long guns) that a portrait
    crop would ruin. The print is centered on the canvas so it sits inside the calendar
    page's 240x170 window when crop_y = FRAME_CROP_Y.
    """
    import math
    img = trim_plain_border(ImageOps.exif_transpose(img).convert("RGB"))
    canvas = Image.new("RGB", (W, H), (240, 230, 205))
    d = ImageDraw.Draw(canvas)
    cx, cy, r = W / 2, H / 2, 400
    for k in range(0, 24, 2):
        a0, a1 = math.radians(k * 15), math.radians(k * 15 + 15)
        d.polygon([(cx, cy), (cx + r * math.cos(a0), cy + r * math.sin(a0)),
                   (cx + r * math.cos(a1), cy + r * math.sin(a1))], fill=(222, 206, 168))
    border, max_w, max_h = 5, W - 16, VIEW_H - 22
    img.thumbnail((max_w - 2 * border, max_h - 2 * border), Image.LANCZOS)
    pw, ph = img.width + 2 * border, img.height + 2 * border
    x, y = (W - pw) // 2, (H - ph) // 2
    shadow = Image.new("L", (W, H), 0)
    ImageDraw.Draw(shadow).rectangle([x + 4, y + 4, x + pw + 3, y + ph + 3], fill=110)
    canvas.paste((40, 32, 25), mask=shadow.filter(ImageFilter.GaussianBlur(3)))
    d = ImageDraw.Draw(canvas)
    d.rectangle([x, y, x + pw - 1, y + ph - 1], fill=(252, 249, 240), outline=(60, 50, 40))
    canvas.paste(img, (x + border, y + border))
    return canvas


FRAME_CROP_Y = (H - VIEW_H) // 2


def trim_plain_border(img: Image.Image, tol: int = 18, pad: float = 0.06) -> Image.Image:
    """Crop away a uniform background (e.g. white product-shot margins), keeping a little padding."""
    from PIL import ImageChops
    corner = img.getpixel((0, 0))
    if any(img.getpixel(p) != corner and max(abs(a - b) for a, b in zip(img.getpixel(p), corner)) > tol
           for p in ((img.width - 1, 0), (0, img.height - 1), (img.width - 1, img.height - 1))):
        return img  # corners differ: not a plain background
    diff = ImageChops.difference(img, Image.new("RGB", img.size, corner)).convert("L")
    bbox = diff.point(lambda v: 255 if v > tol else 0).getbbox()
    if not bbox:
        return img
    px, py = int(img.width * pad), int(img.height * pad)
    return img.crop((max(0, bbox[0] - px), max(0, bbox[1] - py),
                     min(img.width, bbox[2] + px), min(img.height, bbox[3] + py)))


def save_jpeg(img: Image.Image, path: Path) -> int:
    for quality in (88, 82, 75, 68, 60):
        img.save(path, "JPEG", quality=quality, optimize=True, progressive=False, subsampling="4:2:0")
        if path.stat().st_size <= MAX_BYTES:
            break
    return path.stat().st_size


def default_crop(focus: str) -> int:
    return {"top": 0, "center": (H - VIEW_H) // 2, "bottom": H - VIEW_H}[focus]


def short_hash(data: bytes) -> str:
    return hashlib.sha1(data).hexdigest()[:12]


def read_json(path: Path) -> dict:
    return json.loads(path.read_text()) if path.exists() else {}


def write_json(path: Path, data: dict):
    path.write_text(json.dumps(data, indent=2) + "\n")


# ---------------------------------------------------------------- series / editions

def ensure_series(root: Path, series_id: str, title=None, category=None, custom=False):
    if not ID_RE.fullmatch(series_id) or series_id == "custom":
        sys.exit("series id must be 1-40 letters, digits, - or _ (and not 'custom')")
    root.mkdir(parents=True, exist_ok=True)
    path = root / "series.json"
    meta = read_json(path)
    if not meta and not (title and category) and not custom:
        sys.exit(f"new series '{series_id}': also pass --series-title and --category")
    meta["id"] = series_id
    if title:
        meta["title"] = title
    if category:
        meta["category"] = category
    meta.setdefault("title", series_id.replace("-", " ").replace("_", " ").title())
    meta.setdefault("category", "Custom")
    write_json(path, meta)
    stamp_series(root)


def write_edition(root: Path, year: int, title: str, images, captions, focus, crops=None, fit_mode="crop"):
    out = root / str(year)
    out.mkdir(parents=True, exist_ok=True)
    months = []
    for i, img in enumerate(images):
        pic = frame(img) if fit_mode == "frame" else fit(img, focus)
        size = save_jpeg(pic, out / f"{i + 1:02d}.jpg")
        crop = crops[i] if crops else (FRAME_CROP_Y if fit_mode == "frame" else default_crop(focus))
        months.append({"caption": captions[i] if i < len(captions) else "", "crop_y": crop})
        print(f"  {year}/{i + 1:02d}.jpg  {size // 1024} KB")
    write_json(out / "calendar.json", {"year": year, "title": title, "months": months})
    stamp_edition(out)


def write_placeholder(root: Path, img: Image.Image):
    size = save_jpeg(fit(img, "center"), root / "placeholder.jpg")
    print(f"  placeholder.jpg  {size // 1024} KB")
    stamp_series(root)


# ---------------------------------------------------------------- fingerprints

def stamp_edition(year_dir: Path) -> str:
    path = year_dir / "calendar.json"
    meta = read_json(path)
    for i, m in enumerate(meta["months"]):
        m["sha"] = short_hash((year_dir / f"{i + 1:02d}.jpg").read_bytes())
    basis = {k: meta.get(k) for k in ("year", "title", "months")}
    meta["version"] = short_hash(json.dumps(basis, sort_keys=True).encode())
    write_json(path, meta)
    return meta["version"]


def stamp_series(root: Path) -> str:
    path = root / "series.json"
    meta = read_json(path)
    ph = root / "placeholder.jpg"
    if ph.exists():
        meta["placeholder_sha"] = short_hash(ph.read_bytes())
    else:
        meta.pop("placeholder_sha", None)
    basis = {k: meta.get(k) for k in ("title", "category", "placeholder_sha")}
    meta["version"] = short_hash(json.dumps(basis, sort_keys=True).encode())
    write_json(path, meta)
    return meta["version"]


def edition_years(root: Path):
    return sorted(int(d.name) for d in root.iterdir()
                  if d.is_dir() and re.fullmatch(r"\d{4}", d.name) and (d / "calendar.json").exists())


def rebuild_catalog():
    series = []
    for root in sorted((CONTENT / "calendars").iterdir()):
        if not (root / "series.json").exists():
            continue
        meta = read_json(root / "series.json")
        years = {str(y): read_json(root / str(y) / "calendar.json")["version"] for y in edition_years(root)}
        series.append({"id": root.name, "title": meta["title"], "category": meta["category"],
                       "version": meta["version"], "years": years})
    series.sort(key=lambda s: (s["category"], s["title"]))
    write_json(CONTENT / "catalog.json", {"format": 2, "series": series})
    print(f"Updated {CONTENT / 'catalog.json'} ({len(series)} series)")


def restamp_all():
    for root in sorted((CONTENT / "calendars").iterdir()):
        if not (root / "series.json").exists():
            continue
        stamp_series(root)
        for y in edition_years(root):
            print(f"{root.name}/{y}: version {stamp_edition(root / str(y))}")
    rebuild_catalog()


# ---------------------------------------------------------------- CLI

def load_images(args):
    paths = [Path(p).expanduser() for p in args]
    if len(paths) == 1 and paths[0].is_dir():
        paths = sorted(p for p in paths[0].iterdir() if p.suffix.lower() in EXTS)
    if len(paths) != 12:
        sys.exit(f"need exactly 12 images, got {len(paths)}")
    return [Image.open(p) for p in paths]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--restamp", action="store_true", help="refresh fingerprints and rebuild catalog.json")
    ap.add_argument("--series", help="series id, e.g. bikinis")
    ap.add_argument("--series-title", help="series name shown in the picker")
    ap.add_argument("--category", help="e.g. Dogs, Guns, Bikinis")
    ap.add_argument("--year", type=int)
    ap.add_argument("--title", help="this year's edition title")
    ap.add_argument("--images", nargs="+", help="a folder, or 12 files in Jan..Dec order")
    ap.add_argument("--captions", help="text file with up to 12 lines")
    ap.add_argument("--focus", choices=["top", "center", "bottom"], default="top")
    ap.add_argument("--fit", choices=["crop", "frame"], default="crop",
                    help="crop: fill the screen (portrait art); frame: whole photo on a sunburst (landscape photos)")
    ap.add_argument("--placeholder", help="image shown for years with no edition")
    ap.add_argument("--sd", help="SD card mount point: write a custom calendar there instead of content/")
    a = ap.parse_args()

    if a.restamp:
        restamp_all()
        return
    if not a.series:
        ap.error("--series is required")
    custom = bool(a.sd)
    if custom:
        sd = Path(a.sd).expanduser()
        if not sd.is_dir():
            sys.exit(f"{sd} is not a directory (is the SD card mounted?)")
        root = sd / "calendar" / "custom" / a.series
    else:
        root = CONTENT / "calendars" / a.series

    ensure_series(root, a.series, a.series_title, a.category, custom)
    if a.placeholder:
        write_placeholder(root, Image.open(Path(a.placeholder).expanduser()))
    if a.images:
        if not a.year or not a.title:
            ap.error("--images needs --year and --title")
        captions = Path(a.captions).read_text().splitlines()[:12] if a.captions else []
        write_edition(root, a.year, a.title, load_images(a.images), captions, a.focus, fit_mode=a.fit)
    elif not a.placeholder:
        ap.error("nothing to do: pass --images and/or --placeholder")

    if custom:
        print(f"Wrote custom calendar to {root}")
    else:
        rebuild_catalog()


if __name__ == "__main__":
    main()
