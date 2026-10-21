"""Packs the roofs of the group layers test from the painting of them made for the project.

The painting holds three roofs seen from straight above in one row on flat magenta, painted with the "codex" command line in the style of the README of the test project: red clay tiles, blue slate and golden thatch. Run it with `python3 samples/tests/tools/tiled/pack-roofs.py <painting>`. The tool keys out the magenta with the keyer of the props of the Camera tests, trims every roof, halves its size by averaging each square of four pixels, packs the roofs in a row into `content/tiled/tiles/roofs.png` with a clear gutter around each, and prints the area of each roof for `tilesets.py`.
"""

from __future__ import annotations

import importlib.util
import sys
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[4] / "tools"))

from png_image import Image, read, write  # noqa: E402

TOOLS = Path(__file__).resolve().parents[1]
SHEET = Path(__file__).resolve().parents[2] / "content" / "tiled" / "tiles" / "roofs.png"
NAMES = ("clay", "slate", "thatch")
GUTTER = 6

# The keyer and the trim of the props of the Camera tests, whose painting is keyed the same way.
_spec = importlib.util.spec_from_file_location("pack_props", TOOLS / "camera" / "pack-props.py")
props = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(props)


def halved(image: Image) -> Image:
    """Halves an image by averaging each square of four pixels, weighing their colors by their alpha so the edges keep their outline."""
    width, height = image.width // 2, image.height // 2
    result = Image.blank(width, height)
    for y in range(height):
        for x in range(width):
            total, red, green, blue = 0, 0, 0, 0
            for dx, dy in ((0, 0), (1, 0), (0, 1), (1, 1)):
                offset = ((y * 2 + dy) * image.width + x * 2 + dx) * 4
                r, g, b, a = image.pixels[offset : offset + 4]
                total, red, green, blue = total + a, red + r * a, green + g * a, blue + b * a
            pixel = (round(red / total), round(green / total), round(blue / total), round(total / 4)) if total else (0, 0, 0, 0)
            result.pixels[(y * width + x) * 4 : (y * width + x) * 4 + 4] = bytes(pixel)
    return result


def main() -> None:
    if len(sys.argv) != 2:
        sys.exit("Name the painting of the roofs, such as \"python3 pack-roofs.py roofs.png\".")
    painting = read(Path(sys.argv[1]))
    width = painting.width // len(NAMES)
    roofs = [halved(props.trimmed(props.key(painting.crop(index * width, 0, width, painting.height)))) for index in range(len(NAMES))]
    sheet = Image.blank(sum(roof.width for roof in roofs) + GUTTER * 2 * len(roofs), max(roof.height for roof in roofs) + GUTTER * 2)
    x = GUTTER
    for name, roof in zip(NAMES, roofs):
        sheet.paste(roof, x, GUTTER)
        print(f'("{name}", ({x}, {GUTTER}, {roof.width}, {roof.height})),')
        x += roof.width + GUTTER * 2
    write(sheet, SHEET)


if __name__ == "__main__":
    main()
