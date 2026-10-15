"""The grid font of LCD digits of the text tests, whose exact cells the grid font of the engine cuts."""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[4] / "tools"))

from png_image import Image  # noqa: E402


Color = tuple[int, int, int, int]

GRID_CHARACTERS = "0123456789:.-"
GRID_CELL = (12, 20)

# The segments of each digit, named `a` to `g` clockwise from the top with `g` in the middle.
SEGMENTS = {"0": "abcdef", "1": "bc", "2": "abged", "3": "abgcd", "4": "fgbc", "5": "afgcd", "6": "afgedc", "7": "abc", "8": "abcdefg", "9": "abcdfg", "-": "g"}


def rgb(text: str, alpha: int = 255) -> Color:
    return int(text[1:3], 16), int(text[3:5], 16), int(text[5:7], 16), alpha


def put(image: Image, x: int, y: int, color: Color) -> None:
    if 0 <= x < image.width and 0 <= y < image.height:
        offset = (y * image.width + x) * 4
        image.pixels[offset : offset + 4] = bytes(color)


def rect(image: Image, x: int, y: int, width: int, height: int, color: Color) -> None:
    for row in range(y, y + height):
        for column in range(x, x + width):
            put(image, column, row, color)


def lcd_digits() -> Image:
    """One row of equal cells in the order of `GRID_CHARACTERS`, lit segments in green over dim unlit ones."""
    width, height = GRID_CELL
    image = Image.blank(width * len(GRID_CHARACTERS), height)
    lit, dim = rgb("#39ff7a"), rgb("#39ff7a", 40)
    bars = {"a": (3, 1, 6, 2), "d": (3, 17, 6, 2), "g": (3, 9, 6, 2), "f": (1, 3, 2, 6), "b": (9, 3, 2, 6), "e": (1, 11, 2, 6), "c": (9, 11, 2, 6)}
    for index, character in enumerate(GRID_CHARACTERS):
        left = index * width
        if character == ":":
            rect(image, left + 5, 5, 2, 2, lit)
            rect(image, left + 5, 13, 2, 2, lit)
        elif character == ".":
            rect(image, left + 5, 17, 2, 2, lit)
        else:
            for name, (x, y, w, h) in bars.items():
                rect(image, left + x, y, w, h, lit if name in SEGMENTS[character] else dim)
    return image
