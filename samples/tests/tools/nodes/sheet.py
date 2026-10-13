"""The sheet the generators of the Nodes and Animation tests draw on: an RGBA image whose shapes cover pixels by the share of their samples inside, in a flat style with soft edges."""

from __future__ import annotations

import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[4] / "tools"))

from png_image import Image  # noqa: E402

# Every pixel averages a grid of this many samples per side, which gives the edges their soft coverage.
SAMPLES = 4

Color = tuple[int, int, int]


def rgb(text: str) -> Color:
    return int(text[1:3], 16), int(text[3:5], 16), int(text[5:7], 16)


class Sheet:
    """An RGBA image whose shapes cover pixels by the share of their samples inside, painted over what is already drawn."""

    def __init__(self, width: int, height: int) -> None:
        self.image = Image.blank(width, height)

    def paint(self, left: int, top: int, width: int, height: int, inside, color: Color) -> None:
        for y in range(top, top + height):
            for x in range(left, left + width):
                hits = 0
                for row in range(SAMPLES):
                    for column in range(SAMPLES):
                        if inside(x + (column + 0.5) / SAMPLES, y + (row + 0.5) / SAMPLES):
                            hits += 1
                if hits:
                    self.blend(x, y, color, hits / (SAMPLES * SAMPLES))

    def blend(self, x: int, y: int, color: Color, coverage: float) -> None:
        offset = (y * self.image.width + x) * 4
        pixels = self.image.pixels
        below = pixels[offset + 3] / 255
        alpha = coverage + below * (1 - coverage)
        for channel in range(3):
            mixed = color[channel] * coverage + pixels[offset + channel] * below * (1 - coverage)
            pixels[offset + channel] = round(mixed / alpha) if alpha > 0 else 0
        pixels[offset + 3] = round(alpha * 255)

    def rounded(self, left: float, top: float, width: float, height: float, radius: float, color: Color, outline: Color | None = None) -> None:
        def inside(x: float, y: float, grow: float = 0.0) -> bool:
            cx = min(max(x, left + radius), left + width - radius)
            cy = min(max(y, top + radius), top + height - radius)
            return (x - cx) ** 2 + (y - cy) ** 2 <= (radius + grow) ** 2

        box = (math.floor(left) - 2, math.floor(top) - 2, math.ceil(width) + 4, math.ceil(height) + 4)
        if outline is not None:
            self.paint(*box, lambda x, y: inside(x, y, 0.0), outline)
            inner = 2.0
            self.paint(*box, lambda x, y: left + inner <= x <= left + width - inner and top + inner <= y <= top + height - inner and inside(x, y, -inner), color)
            return
        self.paint(*box, inside, color)

    def circle(self, cx: float, cy: float, radius: float, color: Color) -> None:
        self.paint(math.floor(cx - radius) - 1, math.floor(cy - radius) - 1, math.ceil(radius * 2) + 3, math.ceil(radius * 2) + 3, lambda x, y: (x - cx) ** 2 + (y - cy) ** 2 <= radius * radius, color)
