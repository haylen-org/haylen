"""Imports the Tiny Swords pack by Pixel Frog into the Tiny Island package.

Every image is copied with a `snake_case` path, and the UI sheets, which keep their nine-slice pieces apart with empty space, are packed into images the engine can stretch, with their borders in `ui/sliced.json`.
"""

from __future__ import annotations

import argparse
import json
import re
import shutil
import zipfile
from pathlib import Path

import png_image

ROOT_FOLDER = "Tiny Swords (Free Pack)/"

# Folder prefixes of the pack and where they land, before every path component turns `snake_case`.
FOLDERS = [
    ("UI Elements/UI Elements/", "ui/"),
    ("UI Elements/UI Banners from the store page/", "ui/store/"),
    ("Particle FX/", "effects/"),
    ("Terrain/", "terrain/"),
    ("Buildings/", "buildings/"),
    ("Units/", "units/"),
]

# UI sheets to pack, with the names of their rows when one sheet holds several pieces of the same kind.
SLICED = {
    "ui/banners/banner.png": ["banner"],
    "ui/papers/regular_paper.png": ["regular_paper"],
    "ui/papers/special_paper.png": ["special_paper"],
    "ui/wood_table/wood_table.png": ["wood_table"],
    "ui/buttons/big_blue_button_regular.png": ["big_blue_button"],
    "ui/buttons/big_blue_button_pressed.png": ["big_blue_button_pressed"],
    "ui/buttons/big_red_button_regular.png": ["big_red_button"],
    "ui/buttons/big_red_button_pressed.png": ["big_red_button_pressed"],
    "ui/bars/big_bar_base.png": ["big_bar"],
    "ui/bars/small_bar_base.png": ["small_bar"],
    "ui/ribbons/big_ribbons.png": ["ribbon_blue", "ribbon_red", "ribbon_yellow", "ribbon_purple", "ribbon_black"],
}

# Red bar fills turned light, so a UI theme can color every bar while the shading of the pack stays.
LIGHTENED = {
    "ui/bars/big_bar_fill.png": "ui/sliced/big_bar_fill_light.png",
    "ui/bars/small_bar_fill.png": "ui/sliced/small_bar_fill_light.png",
}


def snake_case(component: str) -> str:
    name = re.sub(r"(?<=[a-z0-9])(?=[A-Z])", "_", component)
    name = re.sub(r"[\s\-()]+", "_", name).lower()
    return re.sub(r"_+", "_", name).strip("_")


def destination_path(name: str) -> str | None:
    """Maps a path inside the pack to its path in the package, or returns `None` for files the game does not use."""
    relative = name[len(ROOT_FOLDER) :]
    if not relative.lower().endswith(".png"):
        return None
    for prefix, target in FOLDERS:
        if relative.startswith(prefix):
            rest = relative[len(prefix) :]
            # Colored folders repeat the kind, as in `Blue Units` or `Red Buildings`, and only the color matters.
            rest = re.sub(r"^(\w+) (Units|Buildings)/", r"\1/", rest)
            parts = [snake_case(part) for part in Path(rest).with_suffix("").parts]
            return target + "/".join(parts) + ".png"
    return None


def pack_band(sheet: png_image.Image, top: int, bottom: int, columns: list[tuple[int, int]], rows: list[tuple[int, int]]) -> tuple[png_image.Image, dict[str, int]]:
    """Packs the pieces of one band tightly, keeping the grid of three columns and one or three rows."""
    width = sum(end - start for start, end in columns)
    height = sum(end - start for start, end in rows)
    packed = png_image.Image.blank(width, height)
    y = 0
    for row_start, row_end in rows:
        x = 0
        for column_start, column_end in columns:
            packed.paste(sheet.crop(column_start, top + row_start, column_end - column_start, row_end - row_start), x, y)
            x += column_end - column_start
        y += row_end - row_start
    borders = {"left": columns[0][1] - columns[0][0], "right": columns[2][1] - columns[2][0], "top": 0, "bottom": 0}
    if len(rows) == 3:
        borders["top"] = rows[0][1] - rows[0][0]
        borders["bottom"] = rows[2][1] - rows[2][0]
    return packed, borders


def slice_sheet(sheet: png_image.Image, names: list[str], destination: Path) -> dict[str, dict]:
    bands = sheet.opaque_runs("y")
    if len(names) > 1:
        # Sheets with one piece per row, like the ribbons, split into bands of equal height first.
        band_height = sheet.height // len(names)
        bands = [(index * band_height, (index + 1) * band_height) for index in range(len(names))]
        row_groups = [[(0, band_height)] for _ in names]
    else:
        row_groups = [bands]
        bands = [(0, sheet.height)]

    result = {}
    for name, (top, bottom), rows in zip(names, bands, row_groups):
        band = sheet.crop(0, top, sheet.width, bottom - top)
        columns = band.opaque_runs("x")
        if len(columns) != 3 or len(rows) not in (1, 3):
            raise ValueError(f'The UI sheet for "{name}" does not split into three columns and one or three rows.')
        if len(rows) == 1:
            rows = band.opaque_runs("y")
            if len(rows) != 1:
                raise ValueError(f'The UI sheet for "{name}" holds more than one row of pieces.')
        packed, borders = pack_band(band, 0, band.height, columns, rows)
        path = f"ui/sliced/{name}.png"
        png_image.write(packed, destination / path)
        result[name] = {"image": path, **borders}
    return result


def lighten(image: png_image.Image) -> png_image.Image:
    """Returns the image in gray, scaled so its brightest opaque pixel is white."""
    pixels = bytearray(image.pixels)
    levels = [(pixels[i] * 299 + pixels[i + 1] * 587 + pixels[i + 2] * 114) // 1000 for i in range(0, len(pixels), 4)]
    brightest = max((level for index, level in enumerate(levels) if pixels[index * 4 + 3] > 0), default=0) or 255
    for index, level in enumerate(levels):
        gray = min(255, level * 255 // brightest)
        pixels[index * 4 : index * 4 + 3] = bytes((gray, gray, gray))
    return png_image.Image(image.width, image.height, pixels)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("archive", type=Path, help='Path to "Tiny Swords (Free Pack).zip".')
    parser.add_argument("--destination", type=Path, required=True, help='The "tiny_swords" folder inside the content folder of the package.')
    args = parser.parse_args()

    destination: Path = args.destination
    if destination.exists():
        shutil.rmtree(destination)

    copied = 0
    with zipfile.ZipFile(args.archive) as archive:
        for name in archive.namelist():
            if name.startswith("__MACOSX") or not name.startswith(ROOT_FOLDER):
                continue
            target = destination_path(name)
            if target is None:
                continue
            path = destination / target
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(archive.read(name))
            copied += 1

    sliced = {}
    for source, names in SLICED.items():
        sliced.update(slice_sheet(png_image.read(destination / source), names, destination))
    (destination / "ui" / "sliced.json").write_text(json.dumps(sliced, indent=4, sort_keys=True) + "\n")
    for source, target in LIGHTENED.items():
        png_image.write(lighten(png_image.read(destination / source)), destination / target)
    print(f'Copied {copied} images and packed {len(sliced)} UI pieces into "{destination}".')


if __name__ == "__main__":
    main()
