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

# Optional per-month header picture (MM_top.jpg) for the calendar page. The page's picture
# window is 138-184 px tall depending on the advertising lines in config.txt; the CYD
# centers this 240x184 picture in it.
TOP_H = 184


def blur_fit(img: Image.Image, bars: bool = False) -> Image.Image:
    """Whole picture fitted inside 240x320. The empty space is a blurred, darkened copy of
    the picture (bars=False) or plain near-black bars (bars=True, e.g. for night shots)."""
    img = ImageOps.exif_transpose(img).convert("RGB")
    if bars:
        canvas = Image.new("RGB", (W, H), (12, 12, 14))
    else:
        bg = ImageOps.fit(img, (W, H), Image.LANCZOS).filter(ImageFilter.GaussianBlur(14))
        canvas = Image.blend(bg, Image.new("RGB", (W, H), (0, 0, 0)), 0.35)
    fg = img.copy()
    fg.thumbnail((W, H), Image.LANCZOS)
    canvas.paste(fg, ((W - fg.width) // 2, (H - fg.height) // 2))
    return canvas


def top_crop(img: Image.Image, focus_y="0.5") -> Image.Image:
    """Calendar-page header, 240xTOP_H. focus_y: a 0-1 crop position for the band
    (0.35 = upper-middle), or "fit" to show the whole picture with blurred fill above and
    below (for wide photos such as long guns, which a crop would cut off)."""
    img = ImageOps.exif_transpose(img).convert("RGB")
    if str(focus_y) == "fit":
        bg = ImageOps.fit(img, (W, TOP_H), Image.LANCZOS).filter(ImageFilter.GaussianBlur(14))
        canvas = Image.blend(bg, Image.new("RGB", (W, TOP_H), (0, 0, 0)), 0.35)
        fg = img.copy()
        fg.thumbnail((W, TOP_H), Image.LANCZOS)
        canvas.paste(fg, ((W - fg.width) // 2, (TOP_H - fg.height) // 2))
        return canvas
    return ImageOps.fit(img, (W, TOP_H), Image.LANCZOS, centering=(0.5, float(focus_y)))


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


def save_jpeg(img: Image.Image, path: Path, quality: int = 88) -> int:
    for quality in [quality] + [q for q in (82, 75, 68, 60) if q < quality]:
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


def wants_frame(img: Image.Image, fit_mode: str) -> bool:
    if fit_mode == "auto":  # portrait-ish pictures fill the screen; anything wider is framed
        return not 0.6 <= img.width / img.height <= 0.9
    return fit_mode == "frame"


def write_edition(root: Path, year: int, title: str, images, captions, focus, crops=None, fit_mode="crop",
                  first_month=1, fits=None, tops=None, quality=88):
    """images fill months first_month.. in order; months outside that range get no picture
    (the CYD shows the series placeholder there).

    fits: optional per-image fit mode (overrides fit_mode).
    tops: optional per-image calendar-page header focus (a float, see top_crop) or None for
          no separate header; a header is written as MM_top.jpg.
    """
    out = root / str(year)
    out.mkdir(parents=True, exist_ok=True)
    months = []
    for m in range(1, 13):
        i = m - first_month
        path, top_path = out / f"{m:02d}.jpg", out / f"{m:02d}_top.jpg"
        if not 0 <= i < len(images):
            path.unlink(missing_ok=True)
            top_path.unlink(missing_ok=True)
            months.append({"caption": "", "crop_y": 0})
            continue
        mode = fits[i] if fits else fit_mode
        if mode in ("crop", "frame", "auto"):
            framed = wants_frame(images[i], mode)
            pic = frame(images[i]) if framed else fit(images[i], focus)
        else:  # blur / bars: whole picture, centered
            framed = True
            pic = blur_fit(images[i], bars=(mode == "bars"))
        size = save_jpeg(pic, path, quality)
        crop = crops[i] if crops else (FRAME_CROP_Y if framed else default_crop(focus))
        entry = {"caption": captions[i] if i < len(captions) else "", "crop_y": crop}
        if framed:
            entry["fit"] = "frame"  # tells the CYD the photo is centered, not top-aligned
        note = f"  ({mode})" if mode != "crop" else ""
        if tops and tops[i] is not None:
            save_jpeg(top_crop(images[i], tops[i]), top_path, quality)
            note += "  + top"
        else:
            top_path.unlink(missing_ok=True)
        months.append(entry)
        print(f"  {year}/{m:02d}.jpg  {size // 1024} KB{note}")
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
        jpg, top = year_dir / f"{i + 1:02d}.jpg", year_dir / f"{i + 1:02d}_top.jpg"
        if jpg.exists():
            m["sha"] = short_hash(jpg.read_bytes())
        else:
            m.pop("sha", None)
        if top.exists():
            m["top_sha"] = short_hash(top.read_bytes())
        else:
            m.pop("top_sha", None)
    basis = {k: meta.get(k) for k in ("year", "title", "fit", "months")}
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


# ---------------------------------------------------------------- manifest

MONTHS3 = "jan feb mar apr may jun jul aug sep oct nov dec".split()


def build_manifest(root: Path, manifest: Path, titles: dict, quality: int, export_dir=None):
    """CSV columns: year, month, image, fit, top, caption
         fit: crop | frame | auto | blur | bars   (how the full-screen picture is made)
         top: blank for none, or a 0-1 vertical focus for a separate calendar-page header
              (0.5 = center, 0.35 = upper-middle), or "fit" for the whole picture with
              blurred fill (wide photos)
    Each year becomes an edition; months a year doesn't list show the placeholder."""
    import csv
    rows = list(csv.DictReader(manifest.open()))
    by_year = {}
    for r in rows:
        by_year.setdefault(int(r["year"]), []).append(r)
    for year, rs in sorted(by_year.items()):
        rs.sort(key=lambda r: int(r["month"]))
        first, last = int(rs[0]["month"]), int(rs[-1]["month"])
        if [int(r["month"]) for r in rs] != list(range(first, last + 1)):
            sys.exit(f"{year}: months must be consecutive")
        if year not in titles:
            sys.exit(f"no title for {year}: pass --titles {year}=\"...\"")
        images = [Image.open(Path(r["image"]).expanduser()) for r in rs]
        write_edition(root, year, titles[year], images, [r.get("caption", "") for r in rs], "center",
                      first_month=first, fits=[r["fit"] or "crop" for r in rs],
                      tops=[r["top"] if r.get("top") else None for r in rs], quality=quality)
        if export_dir:  # copies named like bwca_2026_10_oct_full.jpg / _top.jpg
            export_dir.mkdir(parents=True, exist_ok=True)
            for r in rs:
                m = int(r["month"])
                stem = f"{root.name}_{year}_{m:02d}_{MONTHS3[m - 1]}"
                for src, suffix in ((f"{m:02d}.jpg", "full"), (f"{m:02d}_top.jpg", "top")):
                    if (root / str(year) / src).exists():
                        (export_dir / f"{stem}_{suffix}.jpg").write_bytes((root / str(year) / src).read_bytes())


# ---------------------------------------------------------------- CLI

def load_images(args, first_month=1):
    paths = [Path(p).expanduser() for p in args]
    if len(paths) == 1 and paths[0].is_dir():
        paths = sorted(p for p in paths[0].iterdir() if p.suffix.lower() in EXTS)
    room = 13 - first_month
    if first_month == 1 and len(paths) != 12 and len(paths) > 0 and "--partial" not in sys.argv:
        sys.exit(f"need 12 images, got {len(paths)} (add --partial for a short year)")
    if not 1 <= len(paths) <= room:
        sys.exit(f"need 1-{room} images starting at month {first_month}, got {len(paths)}")
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
    ap.add_argument("--fit", choices=["crop", "frame", "auto", "blur", "bars"], default="crop",
                    help="crop: fill the screen (portrait art); frame: whole photo on a sunburst "
                         "(landscape photos); auto: decide per picture")
    ap.add_argument("--first-month", type=int, default=1, choices=range(1, 13), metavar="1-12",
                    help="month the first image goes to (for a year that starts late)")
    ap.add_argument("--partial", action="store_true", help="allow fewer than 12 images (year ends early)")
    ap.add_argument("--placeholder", help="image shown for years with no edition")
    ap.add_argument("--manifest", help="CSV with year,month,image,fit,top,caption (see build_manifest)")
    ap.add_argument("--titles", nargs="+", default=[], help='edition titles for --manifest: 2027="Title" ...')
    ap.add_argument("--quality", type=int, default=88, help="starting JPEG quality")
    ap.add_argument("--export", help="with --manifest: also copy outputs here as <series>_YYYY_MM_mon_full/_top.jpg")
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
    if a.manifest:
        titles = {int(t.split("=", 1)[0]): t.split("=", 1)[1] for t in a.titles}
        build_manifest(root, Path(a.manifest).expanduser(), titles, a.quality,
                       Path(a.export).expanduser() if a.export else None)
    if a.placeholder:
        write_placeholder(root, Image.open(Path(a.placeholder).expanduser()))
    if a.images:
        if not a.year or not a.title:
            ap.error("--images needs --year and --title")
        captions = Path(a.captions).read_text().splitlines()[:12] if a.captions else []
        write_edition(root, a.year, a.title, load_images(a.images, a.first_month), captions, a.focus,
                      fit_mode=a.fit, first_month=a.first_month)
    elif not a.placeholder and not a.manifest:
        ap.error("nothing to do: pass --images, --manifest and/or --placeholder")

    if custom:
        print(f"Wrote custom calendar to {root}")
    else:
        rebuild_catalog()


if __name__ == "__main__":
    main()
