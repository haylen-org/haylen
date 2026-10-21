"""Packs the props of the island of the Camera tests from the sheet of them painted for the project.

The painting holds twelve props in a grid of four columns and three rows on flat magenta, painted with the "codex" command line in the style of the README of the test project: the shrine, the cottage, the well, the signpost, the stump, the log, the mushrooms, the flowering bush, the reeds, the lily pads, the barrel and the campfire. Run it with `python3 samples/tests/tools/camera/pack-props.py <painting>`. The tool keys out the magenta around each prop and in its gaps, unmixing it from the outline it borders, so the pinks inside the props stay whole, trims every prop to its pixels, packs them in rows into `content/camera/props.png` with a clear gutter around each, and prints the area of each prop for `source/categories/camera/world.lua`.
"""

from __future__ import annotations

import sys
from collections import deque
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[4] / "tools"))

from png_image import Image, read, write  # noqa: E402

SHEET = Path(__file__).resolve().parents[2] / "content" / "camera" / "props.png"
NAMES = ("shrine", "cottage", "well", "signpost", "stump", "log", "mushrooms", "flowers", "reeds", "lilies", "barrel", "campfire")
COLUMNS, ROWS = 4, 3
WIDTH = 1024
GUTTER = 6


def coverage(red: int, green: int, blue: int) -> float:
    """Returns how much of a pixel is the prop rather than the magenta behind it."""
    return min(1.0, max(0.0, 1.0 - (min(red, blue) - green - 30) / 225))


def key(cell: Image) -> Image:
    """Makes the magenta around the prop and in its gaps transparent, and unmixes it from the pixels that border it."""
    width, height = cell.width, cell.height
    result = Image(width, height, bytearray(cell.pixels))
    seen = bytearray(width * height)
    edge = (index for index in range(width * height) if index % width in (0, width - 1) or index // width in (0, height - 1))
    enclosed = (index for index in range(width * height) if coverage(*cell.pixels[index * 4 : index * 4 + 3]) <= 0.02)
    queue = deque({*edge, *enclosed})
    for index in queue:
        seen[index] = 1
    while queue:
        index = queue.popleft()
        red, green, blue = cell.pixels[index * 4 : index * 4 + 3]
        alpha = coverage(red, green, blue)
        if alpha >= 0.98:
            continue
        if alpha <= 0.02:
            result.pixels[index * 4 : index * 4 + 4] = bytes(4)
        else:
            unmixed = [min(255, max(0, round((value - (1 - alpha) * magenta) / alpha))) for value, magenta in ((red, 255), (green, 0), (blue, 255))]
            result.pixels[index * 4 : index * 4 + 4] = bytes((*unmixed, round(alpha * 255)))
        x, y = index % width, index // width
        for nx, ny in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
            if 0 <= nx < width and 0 <= ny < height and not seen[ny * width + nx]:
                seen[ny * width + nx] = 1
                queue.append(ny * width + nx)
    return result


def trimmed(image: Image) -> Image:
    """Crops an image to the box of its visible pixels."""
    xs, ys = image.opaque_runs("x"), image.opaque_runs("y")
    left, right, top, bottom = xs[0][0], xs[-1][1], ys[0][0], ys[-1][1]
    return image.crop(left, top, right - left, bottom - top)


def main() -> None:
    if len(sys.argv) != 2:
        sys.exit("Name the painting of the props, such as \"python3 pack-props.py props-sheet.png\".")
    painting = read(Path(sys.argv[1]))
    cellWidth, cellHeight = painting.width // COLUMNS, painting.height // ROWS
    props = []
    for index, name in enumerate(NAMES):
        cell = painting.crop(index % COLUMNS * cellWidth, index // COLUMNS * cellHeight, cellWidth, cellHeight)
        props.append((name, trimmed(key(cell))))

    # Rows from the left, each as tall as its tallest prop.
    places, x, y, row = [], GUTTER, GUTTER, 0
    for name, image in props:
        if x + image.width + GUTTER > WIDTH:
            x, y, row = GUTTER, y + row + GUTTER * 2, 0
        places.append((name, image, x, y))
        x, row = x + image.width + GUTTER * 2, max(row, image.height)
    sheet = Image.blank(WIDTH, y + row + GUTTER)
    for name, image, left, top in places:
        sheet.paste(image, left, top)
        print(f"{name} = {{{left}, {top}, {image.width}, {image.height}}}")
    write(sheet, SHEET)


if __name__ == "__main__":
    main()
