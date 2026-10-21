"""Cuts the pictures of the Physics tests out of the three sheets painted for them.

The sheets are the vehicles, the props and the planets, painted with the "codex" command line in the style of the README of the test project on a transparent background, each a grid of equal cells with one object in every cell. Run it with `python3 samples/tests/tools/physics/pack-art.py <folder of vehicles.png, props.png and space.png>`. The tool crops every cell to the pixels that show, keeps a margin of a few transparent pixels, scales the object so its longer side has the size of its entry and writes it to `content/physics/<name>.png`. Scaling blends the magenta the sheets were painted on into a few pixels of the outlines, so run `python3 tools/check-image-edges.py --clean` on the pictures it wrote.
"""

from __future__ import annotations

import subprocess
import sys
import tempfile
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[4] / "tools"))

from png_image import Image, read, write  # noqa: E402

CONTENT = Path(__file__).resolve().parents[2] / "content" / "physics"
MARGIN = 4
# Each sheet with its grid of columns and rows and its objects in reading order, with the longer side each one is scaled to.
SHEETS = (
    ("vehicles.png", 3, 2, (("car", 384), ("wheel", 160), ("race-car", 256), ("lander", 256), ("ship", 192), ("cannon", 320))),
    ("props.png", 4, 2, (("crate", 160), ("steel-crate", 160), ("barrel", 192), ("candy", 192), ("bear", 192), ("bunny", 192), ("flag", 192), ("coin", 96))),
    ("space.png", 2, 2, (("planet-grass", 384), ("planet-ice", 384), ("planet-desert", 384), ("planet-gas", 384))),
)


def visible(image: Image) -> tuple[int, int, int, int]:
    """Returns the box around the pixels of an image that show."""
    xs, ys = [], []
    for y in range(image.height):
        for x in range(image.width):
            if image.alpha(x, y) > 8:
                xs.append(x)
                ys.append(y)
    return min(xs), min(ys), max(xs) + 1, max(ys) + 1


def scaled(image: Image, longest: int, folder: Path) -> Image:
    """Scales an image with the system tool so its longer side is `longest` pixels."""
    factor = longest / max(image.width, image.height)
    source, target = folder / "source.png", folder / "target.png"
    write(image, source)
    subprocess.run(["sips", "-z", str(round(image.height * factor)), str(round(image.width * factor)), str(source), "--out", str(target)], check=True, capture_output=True)
    return read(target)


def framed(image: Image) -> Image:
    """Returns the image inside a transparent margin, so linear filtering never cuts its outline."""
    result = Image.blank(image.width + MARGIN * 2, image.height + MARGIN * 2)
    result.paste(image, MARGIN, MARGIN)
    return result


def main() -> None:
    if len(sys.argv) != 2:
        sys.exit("Usage: pack-art.py <folder of the painted sheets>")
    folder = Path(sys.argv[1])
    CONTENT.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as scratch:
        for name, columns, rows, objects in SHEETS:
            sheet = read(folder / name)
            width, height = sheet.width // columns, sheet.height // rows
            for index, (piece, longest) in enumerate(objects):
                cell = sheet.crop((index % columns) * width, (index // columns) * height, width, height)
                left, top, right, bottom = visible(cell)
                cropped = cell.crop(left, top, right - left, bottom - top)
                write(framed(scaled(cropped, longest - MARGIN * 2, Path(scratch))), CONTENT / f"{piece}.png")
                print(f"Wrote {piece}.png.")


if __name__ == "__main__":
    main()
