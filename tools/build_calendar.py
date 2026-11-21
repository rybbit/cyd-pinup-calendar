#!/usr/bin/env python3
"""Turn 12 pictures into a pin-up calendar the CYD can download.

    python3 tools/build_calendar.py \
        --id dogs-2027 --title "Good Boys & Girls" --category Dogs --year 2027 \
        --images ~/Pictures/dogs/            # 12 images, sorted by filename = Jan..Dec
        [--captions captions.txt]            # optional, 12 lines ("Miss January - Biscuit")
        [--focus top|center|bottom]          # where to crop if the aspect ratio differs

Writes content/calendars/<id>/01.jpg..12.jpg + calendar.json and adds/updates the
entry in content/catalog.json. Commit and push content/ and every CYD can pick it up.

Each calendar gets a "version" (in catalog.json and calendar.json) and each month a
"sha" fingerprint of its image. The CYD compares them with its SD copy and downloads
only what changed. After hand-editing a calendar.json or swapping a jpg, run
    python3 tools/build_calendar.py --restamp
to refresh the fingerprints and versions.

Each image becomes a 240x320 baseline JPEG (the decoder on the ESP32 cannot read
progressive JPEGs). The calendar page shows a 240x170 window of it; per-month
"crop_y" in calendar.json (0..150) picks which rows show. Requires Pillow.
"""
import argparse
import hashlib
import json
import re
import sys
from pathlib import Path

from PIL import Image, ImageOps

W, H = 240, 320
VIEW_H = 170
MAX_BYTES = 120 * 1024
EXTS = {".jpg", ".jpeg", ".png", ".webp", ".bmp", ".gif", ".tif", ".tiff"}
CONTENT = Path(__file__).resolve().parent.parent / "content"


def fit(img: Image.Image, focus: str) -> Image.Image:
    img = ImageOps.exif_transpose(img).convert("RGB")
    centering = {"top": (0.5, 0.15), "center": (0.5, 0.5), "bottom": (0.5, 0.85)}[focus]
    return ImageOps.fit(img, (W, H), Image.LANCZOS, centering=centering)


def save_jpeg(img: Image.Image, path: Path) -> int:
    for quality in (88, 82, 75, 68, 60):
        img.save(path, "JPEG", quality=quality, optimize=True, progressive=False, subsampling="4:2:0")
        if path.stat().st_size <= MAX_BYTES:
            break
    return path.stat().st_size


def default_crop(focus: str) -> int:
    return {"top": 0, "center": (H - VIEW_H) // 2, "bottom": H - VIEW_H}[focus]


def write_calendar(cal_id, title, category, year, images, captions, focus, crops=None):
    if not re.fullmatch(r"[A-Za-z0-9_-]{1,40}", cal_id):
        sys.exit("id must be 1-40 chars of letters, digits, - or _")
    out = CONTENT / "calendars" / cal_id
    out.mkdir(parents=True, exist_ok=True)

    months = []
    for i, img in enumerate(images):
        size = save_jpeg(fit(img, focus) if img.size != (W, H) else img.convert("RGB"), out / f"{i + 1:02d}.jpg")
        crop = crops[i] if crops else default_crop(focus)
        months.append({"caption": captions[i] if i < len(captions) else "", "crop_y": crop})
        print(f"  {i + 1:02d}.jpg  {size // 1024} KB")

    meta = {"id": cal_id, "title": title, "category": category, "year": year, "months": months}
    (out / "calendar.json").write_text(json.dumps(meta, indent=2) + "\n")
    stamp(out)
    print(f"Wrote {out} and updated {CONTENT / 'catalog.json'}")


def file_sha(path: Path) -> str:
    return hashlib.sha1(path.read_bytes()).hexdigest()[:12]


def stamp(cal_dir: Path) -> str:
    """Write per-month image fingerprints and a calendar version, and sync the catalog entry."""
    meta_path = cal_dir / "calendar.json"
    meta = json.loads(meta_path.read_text())
    for i, m in enumerate(meta["months"]):
        m["sha"] = file_sha(cal_dir / f"{i + 1:02d}.jpg")
    basis = {k: meta[k] for k in ("title", "category", "year", "months")}
    meta["version"] = hashlib.sha1(json.dumps(basis, sort_keys=True).encode()).hexdigest()[:12]
    meta_path.write_text(json.dumps(meta, indent=2) + "\n")

    catalog_path = CONTENT / "catalog.json"
    catalog = json.loads(catalog_path.read_text()) if catalog_path.exists() else {"format": 1, "calendars": []}
    entry = {"id": meta["id"], "title": meta["title"], "category": meta["category"], "year": meta["year"],
             "version": meta["version"]}
    catalog["calendars"] = [c for c in catalog["calendars"] if c["id"] != meta["id"]] + [entry]
    catalog["calendars"].sort(key=lambda c: (c["category"], -c.get("year", 0), c["title"]))
    catalog_path.write_text(json.dumps(catalog, indent=2) + "\n")
    return meta["version"]


def restamp_all():
    for cal_dir in sorted((CONTENT / "calendars").iterdir()):
        if (cal_dir / "calendar.json").exists():
            print(f"{cal_dir.name}: version {stamp(cal_dir)}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--restamp", action="store_true", help="refresh versions of every calendar in content/")
    ap.add_argument("--id", required=True, help="folder name, e.g. dogs-2027")
    ap.add_argument("--title", required=True)
    ap.add_argument("--category", required=True, help="e.g. Dogs, Guns, Bikinis")
    ap.add_argument("--year", type=int, required=True)
    ap.add_argument("--images", required=True, nargs="+", help="a folder, or 12 files in Jan..Dec order")
    ap.add_argument("--captions", help="text file with up to 12 lines")
    ap.add_argument("--focus", choices=["top", "center", "bottom"], default="top")
    if "--restamp" in sys.argv[1:]:
        restamp_all()
        return
    a = ap.parse_args()

    paths = [Path(p).expanduser() for p in a.images]
    if len(paths) == 1 and paths[0].is_dir():
        paths = sorted(p for p in paths[0].iterdir() if p.suffix.lower() in EXTS)
    if len(paths) != 12:
        sys.exit(f"need exactly 12 images, got {len(paths)}")
    captions = Path(a.captions).read_text().splitlines()[:12] if a.captions else []
    write_calendar(a.id, a.title, a.category, a.year, [Image.open(p) for p in paths], captions, a.focus)


if __name__ == "__main__":
    main()
