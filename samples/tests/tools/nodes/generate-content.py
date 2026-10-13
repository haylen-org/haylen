"""Draws the parts of the vehicles of the Nodes tests into one sheet: a hull, a turret, a barrel, a wheel and a flag, in a flat style with soft edges and a darker outline.

Run it from anywhere with `python3 samples/tests/tools/nodes/generate-content.py`. The same run always writes the same file into `samples/tests/content/nodes`.
"""

from __future__ import annotations

import sys
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[4] / "tools"))

from png_image import Image, write  # noqa: E402
from sheet import Sheet, rgb  # noqa: E402

CONTENT = Path(__file__).resolve().parents[2] / "content" / "nodes"


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
