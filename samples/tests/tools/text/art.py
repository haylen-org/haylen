"""The images of the text tests: a grid font of LCD digits, the button prompts that `[icon]` shows and the small pictures that `[img]` shows."""

from __future__ import annotations

import math
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


def disc(image: Image, cx: float, cy: float, radius: float, color: Color) -> None:
    for row in range(math.floor(cy - radius), math.ceil(cy + radius) + 1):
        for column in range(math.floor(cx - radius), math.ceil(cx + radius) + 1):
            if (column + 0.5 - cx) ** 2 + (row + 0.5 - cy) ** 2 <= radius * radius:
                put(image, column, row, color)


def polygon(image: Image, points: list[tuple[float, float]], color: Color) -> None:
    xs, ys = [p[0] for p in points], [p[1] for p in points]
    for row in range(math.floor(min(ys)), math.ceil(max(ys)) + 1):
        for column in range(math.floor(min(xs)), math.ceil(max(xs)) + 1):
            x, y, inside = column + 0.5, row + 0.5, False
            for index, (ax, ay) in enumerate(points):
                bx, by = points[index - 1]
                if (ay > y) != (by > y) and x < (bx - ax) * (y - ay) / (by - ay) + ax:
                    inside = not inside
            if inside:
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


def prompts() -> Image:
    """The south, east, west and north buttons of a gamepad in cells of 32 pixels: a dark disc with four dots in a diamond, where the dot of the button is large and lit."""
    positions = [(0, 1), (1, 0), (-1, 0), (0, -1)]
    image = Image.blank(32 * len(positions), 32)
    for index, lit in enumerate(positions):
        cx = index * 32 + 16
        disc(image, cx, 16, 15, rgb("#8fb0ff"))
        disc(image, cx, 16, 13, rgb("#232938"))
        for dx, dy in positions:
            if (dx, dy) == lit:
                disc(image, cx + dx * 7, 16 + dy * 7, 4.5, rgb("#f2b23a"))
            else:
                disc(image, cx + dx * 7, 16 + dy * 7, 2.5, rgb("#7a8099"))
    return image


def coin() -> Image:
    image = Image.blank(32, 32)
    disc(image, 16, 16, 13, rgb("#7a4a10"))
    disc(image, 16, 16, 11, rgb("#f2c14e"))
    disc(image, 16, 16, 7, rgb("#d9a032"))
    rect(image, 15, 11, 2, 10, rgb("#fff3c4"))
    return image


def heart() -> Image:
    image = Image.blank(32, 32)
    for color, grow in ((rgb("#5a1010"), 1.5), (rgb("#e53935"), 0)):
        disc(image, 11, 12, 6 + grow, color)
        disc(image, 21, 12, 6 + grow, color)
        polygon(image, [(4.5 - grow, 14), (27.5 + grow, 14), (16, 27 + grow)], color)
    disc(image, 10, 10, 2, rgb("#ffffff", 220))
    return image


def star() -> Image:
    image = Image.blank(32, 32)
    for color, outer, inner in ((rgb("#6b4a00"), 15, 7), (rgb("#ffd54f"), 12.5, 5.5)):
        points = [(16 + math.cos(-math.pi / 2 + i * math.pi / 5) * (outer if i % 2 == 0 else inner), 17 + math.sin(-math.pi / 2 + i * math.pi / 5) * (outer if i % 2 == 0 else inner)) for i in range(10)]
        polygon(image, points, color)
    return image
