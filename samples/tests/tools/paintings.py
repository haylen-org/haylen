"""Turns the paintings of the test project into the pieces its tools pack.

The art is painted for the project with the "codex" command line on flat magenta, as the README of the test project describes. This module keys the magenta out and unmixes it from the edges, finds the separate parts of a sheet, measures what a part shows and scales a part with the "sips" command of macOS, which keeps the edges smooth.
"""

from __future__ import annotations

import subprocess
import sys
import tempfile
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "tools"))

from png_image import Image, read, write  # noqa: E402

# The alpha above which a pixel counts as part of what an image shows.
VISIBLE = 128


def key(painted: Image) -> Image:
    """Turns the magenta of a painting into transparency, unmixing it from the colors of the edges."""
    result = Image.blank(painted.width, painted.height)
    for index in range(painted.width * painted.height):
        red, green, blue = painted.pixels[index * 4 : index * 4 + 3]
        alpha = min(1.0, max(0.0, 1.0 - (min(red, blue) - green - 30) / 225))
        if alpha <= 0.02:
            continue
        unmixed = [min(255, max(0, round((value - (1 - alpha) * magenta) / alpha))) for value, magenta in ((red, 255), (green, 0), (blue, 255))]
        result.pixels[index * 4 : index * 4 + 4] = bytes((*unmixed, round(alpha * 255)))
    return result


def parts(keyed: Image, step: int = 2) -> list[tuple[int, int, int, int]]:
    """Returns the boxes of the separate parts of a keyed sheet as left, top, right and bottom, in rows from the top and from the left within a row. Specks smaller than a hundredth of the largest part join the part nearest to them."""
    columns, rows = keyed.width // step, keyed.height // step
    seen = bytearray(columns * rows)
    found = []
    for start in range(columns * rows):
        if seen[start] or keyed.alpha(start % columns * step, start // columns * step) <= VISIBLE:
            continue
        seen[start] = 1
        stack, count = [start], 0
        left, top, right, bottom = columns, rows, 0, 0
        while stack:
            cell = stack.pop()
            x, y = cell % columns, cell // columns
            count += 1
            left, top, right, bottom = min(left, x), min(top, y), max(right, x), max(bottom, y)
            for nx, ny in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
                if 0 <= nx < columns and 0 <= ny < rows and not seen[ny * columns + nx] and keyed.alpha(nx * step, ny * step) > VISIBLE:
                    seen[ny * columns + nx] = 1
                    stack.append(ny * columns + nx)
        found.append([count, left * step, top * step, right * step + step, bottom * step + step])

    largest = max(entry[0] for entry in found)
    big = [entry for entry in found if entry[0] * 100 >= largest]
    for speck in (entry for entry in found if entry[0] * 100 < largest):
        nearest = min(big, key=lambda entry: max(entry[1] - speck[3], speck[1] - entry[3], 0) + max(entry[2] - speck[4], speck[2] - entry[4], 0))
        nearest[1:] = [min(nearest[1], speck[1]), min(nearest[2], speck[2]), max(nearest[3], speck[3]), max(nearest[4], speck[4])]
    boxes = [tuple(entry[1:]) for entry in big]
    height = max(box[3] - box[1] for box in boxes)
    return sorted(boxes, key=lambda box: (round((box[1] + box[3]) / 2 / height), box[0]))


def bounds(image: Image, threshold: int = VISIBLE) -> tuple[int, int, int, int]:
    """Returns the box of the pixels of an image whose alpha passes the threshold as left, top, right and bottom, inclusive."""
    xs, ys = [], []
    for y in range(image.height):
        for x in range(image.width):
            if image.alpha(x, y) > threshold:
                xs.append(x)
                ys.append(y)
    return min(xs), min(ys), max(xs), max(ys)


def cut(painted: Image, box: tuple[int, int, int, int], factor: float, margin: int = 6) -> Image:
    """Cuts a part out of a painting with a margin of magenta, scales it while the magenta still surrounds it, so the scaling blends the edges into magenta the key then unmixes, and keys it."""
    left, top = max(0, box[0] - margin), max(0, box[1] - margin)
    right, bottom = min(painted.width, box[2] + margin), min(painted.height, box[3] + margin)
    return key(scaled(painted.crop(left, top, right - left, bottom - top), factor))


def scaled(image: Image, factor: float) -> Image:
    with tempfile.TemporaryDirectory() as folder:
        source, target = Path(folder) / "source.png", Path(folder) / "scaled.png"
        write(image, source)
        width, height = max(1, round(image.width * factor)), max(1, round(image.height * factor))
        subprocess.run(["sips", "-z", str(height), str(width), str(source), "--out", str(target)], check=True, capture_output=True)
        return read(target)


def trimmed(image: Image, margin: int = 2) -> Image:
    """Crops an image to every pixel it covers, with a margin of transparency."""
    left, top, right, bottom = bounds(image, 0)
    left, top = max(0, left - margin), max(0, top - margin)
    right, bottom = min(image.width - 1, right + margin), min(image.height - 1, bottom + margin)
    return image.crop(left, top, right - left + 1, bottom - top + 1)
