"""An RGBA canvas with the few drawing operations the art of the UI sample needs, all blending over what is already drawn."""

from __future__ import annotations

import math
import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[4] / "tools"))

from png_image import Image, write  # noqa: E402

Color = tuple[int, int, int, int]


def rgb(text: str, alpha: int = 255) -> Color:
    return int(text[1:3], 16), int(text[3:5], 16), int(text[5:7], 16), alpha


def shade(color: Color, amount: float) -> Color:
    return tuple(max(0, min(255, round(channel * amount))) for channel in color[:3]) + (color[3],)


def mix(first: Color, second: Color, amount: float) -> Color:
    return tuple(round(a + (b - a) * amount) for a, b in zip(first, second))


class Canvas:
    def __init__(self, width: int, height: int) -> None:
        self.image = Image.blank(width, height)

    def put(self, x: int, y: int, color: Color) -> None:
        if not (0 <= x < self.image.width and 0 <= y < self.image.height):
            return
        offset = (y * self.image.width + x) * 4
        pixels = self.image.pixels
        alpha = color[3] / 255
        below = pixels[offset + 3] / 255
        result = alpha + below * (1 - alpha)
        if result == 0:
            return
        for channel in range(3):
            pixels[offset + channel] = round((color[channel] * alpha + pixels[offset + channel] * below * (1 - alpha)) / result)
        pixels[offset + 3] = round(result * 255)

    def rect(self, x: int, y: int, width: int, height: int, color: Color) -> None:
        for row in range(y, y + height):
            for column in range(x, x + width):
                self.put(column, row, color)

    def rounded(self, x: int, y: int, width: int, height: int, radius: int, color: Color) -> None:
        """Fills a rectangle whose corners are cut in quarter circles of the radius."""
        for row in range(height):
            for column in range(width):
                dx = max(radius - column - 0.5, column + 0.5 - (width - radius), 0)
                dy = max(radius - row - 0.5, row + 0.5 - (height - radius), 0)
                if dx * dx + dy * dy <= radius * radius:
                    self.put(x + column, y + row, color)

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

    def line(self, x1: float, y1: float, x2: float, y2: float, width: float, color: Color) -> None:
        steps = max(1, round(math.hypot(x2 - x1, y2 - y1) * 2))
        visited = set()
        for step in range(steps + 1):
            t = step / steps
            x, y = x1 + (x2 - x1) * t, y1 + (y2 - y1) * t
            for row in range(math.floor(y - width / 2), math.ceil(y + width / 2)):
                for column in range(math.floor(x - width / 2), math.ceil(x + width / 2)):
                    if (column, row) not in visited and math.hypot(column + 0.5 - x, row + 0.5 - y) <= width / 2:
                        visited.add((column, row))
                        self.put(column, row, color)

    def gradient(self, x: int, y: int, width: int, height: int, top: Color, bottom: Color) -> None:
        for row in range(height):
            self.rect(x, y + row, width, 1, mix(top, bottom, row / max(1, height - 1)))

    def speckle(self, x: int, y: int, width: int, height: int, colors: list[Color], count: int, seed: int) -> None:
        rng = random.Random(seed)
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
