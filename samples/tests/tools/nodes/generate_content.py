"""Draws the parts of the vehicles of the Nodes tests into one sheet: a hull, a turret, a barrel, a wheel and a flag, in a flat style with soft edges and a darker outline.

Run it from anywhere with `python3 samples/tests/tools/nodes/generate_content.py`. The same run always writes the same file into `samples/tests/content/nodes`.
"""

from __future__ import annotations

import math
import sys
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[4] / "tools"))

from png_image import Image, write  # noqa: E402

CONTENT = Path(__file__).resolve().parents[2] / "content" / "nodes"

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


def parts() -> Image:
    sheet = Sheet(256, 64)
    outline, body, light, dark = rgb("#24331f"), rgb("#5f7f3d"), rgb("#86a85a"), rgb("#425a2b")

    # The hull, 108 by 52 at the top left, with tracks along its bottom.
    sheet.rounded(2, 2, 108, 52, 12, body, outline)
    sheet.rounded(10, 8, 92, 14, 6, light)
    sheet.rounded(6, 38, 100, 14, 6, dark)

    # The turret, 56 by 36, and the barrel, 60 by 12, which turns around its left end.
    sheet.rounded(114, 2, 56, 36, 14, body, outline)
    sheet.rounded(122, 8, 36, 10, 5, light)
    sheet.rounded(174, 2, 60, 12, 5, dark, outline)

    # The wheel, 22 across, with a hub that shows it turning.
    sheet.circle(185, 29, 11, outline)
    sheet.circle(185, 29, 8.5, dark)
    sheet.rounded(183, 20, 4, 18, 2, light)

    # The flag, 40 by 22, on its own.
    sheet.paint(200, 18, 44, 26, lambda x, y: 202 <= x <= 242 and 20 <= y <= 42 - (x - 202) * 0.15, rgb("#e2683c"))
    sheet.rounded(202, 20, 4, 40, 2, outline)
    return sheet.image


def main() -> None:
    CONTENT.mkdir(parents=True, exist_ok=True)
    write(parts(), CONTENT / "parts.png")


if __name__ == "__main__":
    main()
