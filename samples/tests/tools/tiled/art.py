"""Draws the pixel art of the Tiled tests with the Python standard library: terrain, props, isometric and hexagonal tiles, backgrounds and the hero."""

from __future__ import annotations

import math
import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[4] / "tools"))

from png_image import Image, write  # noqa: E402

Color = tuple[int, int, int, int]

TILE = 32


def rgb(text: str, alpha: int = 255) -> Color:
    return int(text[1:3], 16), int(text[3:5], 16), int(text[5:7], 16), alpha


def shade(color: Color, amount: float) -> Color:
    return tuple(max(0, min(255, round(channel * amount))) for channel in color[:3]) + (color[3],)


class Canvas:
    """An RGBA image with the few drawing operations the art needs, all blending over what is already drawn."""

    def __init__(self, width: int, height: int) -> None:
        self.image = Image.blank(width, height)

    def put(self, x: int, y: int, color: Color) -> None:
        if not (0 <= x < self.image.width and 0 <= y < self.image.height):
            return
        offset = (y * self.image.width + x) * 4
        alpha = color[3] / 255
        pixels = self.image.pixels
        for channel in range(3):
            pixels[offset + channel] = round(color[channel] * alpha + pixels[offset + channel] * (1 - alpha))
        pixels[offset + 3] = max(pixels[offset + 3], color[3])

    def rect(self, x: int, y: int, width: int, height: int, color: Color) -> None:
        for row in range(y, y + height):
            for column in range(x, x + width):
                self.put(column, row, color)

    def ellipse(self, cx: float, cy: float, rx: float, ry: float, color: Color) -> None:
        for row in range(math.floor(cy - ry), math.ceil(cy + ry) + 1):
            for column in range(math.floor(cx - rx), math.ceil(cx + rx) + 1):
                if ((column + 0.5 - cx) / rx) ** 2 + ((row + 0.5 - cy) / ry) ** 2 <= 1:
                    self.put(column, row, color)

    def polygon(self, points: list[tuple[float, float]], color: Color) -> None:
        xs, ys = [point[0] for point in points], [point[1] for point in points]
        for row in range(math.floor(min(ys)), math.ceil(max(ys)) + 1):
            for column in range(math.floor(min(xs)), math.ceil(max(xs)) + 1):
                if contains(points, column + 0.5, row + 0.5):
                    self.put(column, row, color)

    def speckle(self, x: int, y: int, width: int, height: int, colors: list[Color], count: int, rng: random.Random) -> None:
        for _ in range(count):
            self.put(x + rng.randrange(width), y + rng.randrange(height), rng.choice(colors))

    def save(self, path: Path) -> None:
        write(self.image, path)


def contains(points: list[tuple[float, float]], x: float, y: float) -> bool:
    inside = False
    for index, (ax, ay) in enumerate(points):
        bx, by = points[index - 1]
        if (ay > y) != (by > y) and x < (bx - ax) * (y - ay) / (by - ay) + ax:
            inside = not inside
    return inside


def ground(canvas: Canvas, x: int, y: int, base: str, specks: list[str], seed: int) -> None:
    rng = random.Random(seed)
    canvas.rect(x, y, TILE, TILE, rgb(base))
    canvas.speckle(x, y, TILE, TILE, [rgb(speck) for speck in specks], 60, rng)


def bricks(canvas: Canvas, x: int, y: int, top: int) -> None:
    mortar, brick = rgb("#3b3f4a"), rgb("#7d8597")
    canvas.rect(x, y + top, TILE, TILE - top, mortar)
    for row, line in enumerate(range(y + top, y + TILE, 8)):
        shift = 8 if row % 2 else 0
        for column in range(x - shift, x + TILE, 16):
            left = max(column, x)
            right = min(column + 15, x + TILE)
            canvas.rect(left, line, right - left, 7, shade(brick, 1.0 - 0.08 * (row % 3)))
    canvas.rect(x, y + top, TILE, 2, rgb("#a3abbd"))


def water(canvas: Canvas, x: int, y: int, frame: int, deep: str, crest: str) -> None:
    canvas.rect(x, y, TILE, TILE, rgb(deep))
    for band in range(4):
        for column in range(TILE):
            offset = round(math.sin((column + frame * 8) / TILE * math.tau + band) * 1.5)
            canvas.put(x + column, y + 4 + band * 8 + offset, rgb(crest, 170))


def flame(canvas: Canvas, cx: float, cy: float, size: float) -> None:
    canvas.ellipse(cx, cy, 4 * size, 7 * size, rgb("#ff7043"))
    canvas.ellipse(cx, cy + 1.5 * size, 2.5 * size, 4.5 * size, rgb("#ffd54f"))


def terrain() -> Canvas:
    """Eight columns by four rows of 32 pixel tiles, in the order the maps number them from 0."""
    canvas = Canvas(8 * TILE, 4 * TILE)

    def origin(tile: int) -> tuple[int, int]:
        return (tile % 8) * TILE, (tile // 8) * TILE

    ground(canvas, *origin(0), "#5a9e3c", ["#4c8a33", "#6db547"], 1)
    ground(canvas, *origin(1), "#5a9e3c", ["#4c8a33", "#6db547"], 2)
    rng = random.Random(3)
    for _ in range(7):
        fx, fy = origin(1)
        canvas.ellipse(fx + rng.randrange(4, 28), fy + rng.randrange(4, 28), 1.6, 1.6, rgb(rng.choice(["#f48fb1", "#fff59d", "#ffffff"])))
    ground(canvas, *origin(2), "#8d6e4c", ["#7a5d3f", "#9f7f5c"], 4)
    ground(canvas, *origin(3), "#e3cf8f", ["#d4bf7d", "#efe0aa"], 5)
    sx, sy = origin(4)
    canvas.rect(sx, sy, TILE, TILE, rgb("#8e939e"))
    for line in (0, 16):
        canvas.rect(sx, sy + line, TILE, 1, rgb("#6f7480"))
        canvas.rect(sx + line, sy, 1, TILE, rgb("#6f7480"))
    bricks(canvas, *origin(5), 0)
    hx, hy = origin(6)
    bricks(canvas, hx, hy, 16)
    fx, fy = origin(7)
    for post in (3, 25):
        canvas.rect(fx + post, fy + 6, 4, 20, rgb("#8d6e63"))
    for rail in (10, 18):
        canvas.rect(fx, fy + rail, TILE, 3, rgb("#a1887f"))

    for frame in range(4):
        water(canvas, *origin(8 + frame), frame, "#1e6091", "#90caf9")
        lx, ly = origin(12 + frame)
        canvas.rect(lx, ly, TILE, TILE, rgb("#b23b12"))
        bubbles = random.Random(20 + frame)
        for _ in range(6):
            canvas.ellipse(lx + bubbles.randrange(4, 28), ly + bubbles.randrange(4, 28), 2.5, 2.5, rgb("#ffb300"))

    for frame in range(2):
        tx, ty = origin(16 + frame)
        bricks(canvas, tx, ty, 0)
        canvas.rect(tx + 14, ty + 16, 4, 12, rgb("#5d4037"))
        flame(canvas, tx + 16, ty + 11 - frame, 1.0 + 0.2 * frame)

    bx, by = origin(18)
    canvas.ellipse(bx + 16, by + 17, 13, 12, rgb("#2e7d32"))
    canvas.ellipse(bx + 12, by + 13, 6, 5, rgb("#43a047"))
    rx, ry = origin(19)
    canvas.polygon([(rx + 4, ry + 26), (rx + 8, ry + 10), (rx + 18, ry + 5), (rx + 28, ry + 12), (rx + 29, ry + 27)], rgb("#9e9e9e"))
    canvas.polygon([(rx + 8, ry + 12), (rx + 18, ry + 7), (rx + 22, ry + 12), (rx + 12, ry + 16)], rgb("#bdbdbd"))
    cx, cy = origin(20)
    canvas.rect(cx + 2, cy + 2, 28, 28, rgb("#a1662f"))
    canvas.rect(cx + 4, cy + 4, 24, 24, rgb("#c68642"))
    canvas.polygon([(cx + 4, cy + 7), (cx + 7, cy + 4), (cx + 28, cy + 25), (cx + 25, cy + 28)], rgb("#a1662f"))
    kx, ky = origin(21)
    canvas.rect(kx + 3, ky + 10, 26, 18, rgb("#8d5524"))
    canvas.rect(kx + 3, ky + 6, 26, 8, rgb("#a0522d"))
    canvas.rect(kx + 14, ky + 12, 4, 6, rgb("#ffd54f"))
    px, py = origin(22)
    for spike in range(4):
        canvas.polygon([(px + spike * 8, py + 30), (px + spike * 8 + 4, py + 14), (px + spike * 8 + 8, py + 30)], rgb("#cfd8dc"))
    gx, gy = origin(23)
    canvas.rect(gx + 8, gy + 4, 3, 26, rgb("#6d4c41"))
    canvas.polygon([(gx + 11, gy + 4), (gx + 27, gy + 9), (gx + 11, gy + 15)], rgb("#e53935"))

    for frame in range(4):
        ox, oy = origin(24 + frame)
        width = [10, 7, 3, 7][frame]
        canvas.ellipse(ox + 16, oy + 16, width, 10, rgb("#f9a825"))
        canvas.ellipse(ox + 16, oy + 16, max(1, width - 3), 7, rgb("#ffd54f"))
    for frame in range(2):
        mx, my = origin(28 + frame)
        squash = 2 * frame
        canvas.ellipse(mx + 16, my + 20 + squash / 2, 12 + squash, 10 - squash, rgb("#66bb6a"))
        canvas.ellipse(mx + 12, my + 17 + squash, 2, 2, rgb("#1b1e2b"))
        canvas.ellipse(mx + 20, my + 17 + squash, 2, 2, rgb("#1b1e2b"))
    sx, sy = origin(30)
    canvas.rect(sx + 14, sy + 16, 4, 14, rgb("#6d4c41"))
    canvas.rect(sx + 4, sy + 5, 24, 13, rgb("#a1887f"))
    canvas.rect(sx + 7, sy + 9, 18, 2, rgb("#5d4037"))
    kx, ky = origin(31)
    canvas.ellipse(kx + 10, ky + 16, 6, 6, rgb("#ffca28"))
    canvas.ellipse(kx + 10, ky + 16, 3, 3, rgb("#b8860b"))
    canvas.rect(kx + 15, ky + 15, 13, 3, rgb("#ffca28"))
    canvas.rect(kx + 23, ky + 18, 3, 5, rgb("#ffca28"))
    return canvas


def props() -> Canvas:
    """Three props of 32 by 64 pixels: a round tree, a pine and a lamp post."""
    canvas = Canvas(3 * TILE, 2 * TILE)
    canvas.rect(13, 44, 6, 20, rgb("#6d4c41"))
    canvas.ellipse(16, 26, 15, 18, rgb("#2e7d32"))
    canvas.ellipse(11, 20, 7, 7, rgb("#43a047"))
    canvas.rect(32 + 14, 50, 5, 14, rgb("#5d4037"))
    for layer in range(3):
        top = 4 + layer * 14
        canvas.polygon([(32 + 16, top), (32 + 30 - layer, top + 22), (32 + 2 + layer, top + 22)], rgb("#1b5e20" if layer % 2 else "#2e7d32"))
    canvas.rect(64 + 14, 16, 4, 48, rgb("#455a64"))
    canvas.rect(64 + 9, 8, 14, 10, rgb("#37474f"))
    canvas.ellipse(64 + 16, 13, 5, 4, rgb("#fff59d"))
    return canvas


def diamond_tiles() -> Canvas:
    """Four flat isometric tiles of 64 by 32 pixels: grass, water, sand and stone."""
    canvas = Canvas(4 * 64, 32)
    colors = [("#5a9e3c", "#4c8a33"), ("#1e6091", "#2b7bb9"), ("#e3cf8f", "#cdb877"), ("#8e939e", "#6f7480")]
    for index, (fill, edge) in enumerate(colors):
        x = index * 64
        canvas.polygon([(x + 32, 0), (x + 64, 16), (x + 32, 32), (x, 16)], rgb(edge))
        canvas.polygon([(x + 32, 2), (x + 60, 16), (x + 32, 30), (x + 4, 16)], rgb(fill))
    return canvas


def hex_tiles() -> Canvas:
    """Four pointy hexagons of 56 by 64 pixels, whose flat sides are 32 pixels long: grass, water, sand and forest."""
    canvas = Canvas(4 * 56, 64)
    colors = [("#5a9e3c", "#4c8a33"), ("#1e6091", "#2b7bb9"), ("#e3cf8f", "#cdb877"), ("#2e7d32", "#1b5e20")]
    for index, (fill, edge) in enumerate(colors):
        x = index * 56
        canvas.polygon([(x + 28, 0), (x + 56, 16), (x + 56, 48), (x + 28, 64), (x, 48), (x, 16)], rgb(edge))
        canvas.polygon([(x + 28, 3), (x + 53, 17), (x + 53, 47), (x + 28, 61), (x + 3, 47), (x + 3, 17)], rgb(fill))
        if index == 3:
            canvas.polygon([(x + 28, 14), (x + 40, 40), (x + 16, 40)], rgb("#1b5e20"))
    return canvas


def sky() -> Canvas:
    canvas = Canvas(16, 360)
    top, bottom = rgb("#1e3a5f"), rgb("#f6b26b")
    for row in range(360):
        t = row / 359
        canvas.rect(0, row, 16, 1, tuple(round(top[i] + (bottom[i] - top[i]) * t) for i in range(3)) + (255,))
    return canvas


def clouds() -> Canvas:
    canvas = Canvas(320, 160)
    rng = random.Random(9)
    for _ in range(3):
        cx, cy = rng.randrange(40, 280), rng.randrange(30, 120)
        for _ in range(5):
            canvas.ellipse(cx + rng.randrange(-30, 30), cy + rng.randrange(-8, 8), rng.randrange(14, 26), rng.randrange(8, 14), rgb("#ffffff", 200))
    return canvas


def hills(width: int, height: int, color: str, seed: int, bumps: int) -> Canvas:
    canvas = Canvas(width, height)
    rng = random.Random(seed)
    phases = [rng.random() * math.tau for _ in range(3)]
    fill = rgb(color)
    for column in range(width):
        t = column / width * math.tau
        top = height * (0.45 + 0.2 * math.sin(t * bumps + phases[0]) + 0.1 * math.sin(t * bumps * 2 + phases[1]))
        canvas.rect(column, round(top), 1, height - round(top), fill)
    return canvas


def hero() -> Canvas:
    """A small adventurer of 32 by 48 pixels whose feet touch the bottom edge."""
    canvas = Canvas(32, 48)
    canvas.ellipse(16, 46, 10, 2.5, rgb("#000000", 90))
    canvas.rect(10, 34, 5, 12, rgb("#37474f"))
    canvas.rect(17, 34, 5, 12, rgb("#37474f"))
    canvas.rect(8, 18, 16, 18, rgb("#1976d2"))
    canvas.rect(8, 30, 16, 3, rgb("#5d4037"))
    canvas.ellipse(16, 11, 7, 7.5, rgb("#ffcc80"))
    canvas.rect(9, 3, 14, 5, rgb("#6d4c41"))
    canvas.put(13, 11, rgb("#1b1e2b"))
    canvas.put(19, 11, rgb("#1b1e2b"))
    return canvas
