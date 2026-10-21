"""The images of the Tiled tests that come from the paintings instead of being painted: the sky of the parallax map, a gradient of a clear day, and the sea under the island of the group layers test, the first frame of the painted water."""

from __future__ import annotations

import sys
from pathlib import Path

import tilesets as t
import transitions as tr

sys.path.insert(0, str(Path(__file__).resolve().parents[4] / "tools"))
from png_image import Image, read, write  # noqa: E402

IMAGES = Path(__file__).resolve().parents[2] / "content" / "tiled" / "images"
# The sky from its top to the horizon: a deep blue that pales into the haze over the hills.
SKY = ((0.0, (74, 156, 226)), (0.6, (142, 205, 244)), (1.0, (214, 238, 250)))


def write_sky() -> None:
    width, height = 16, 384
    image = Image.blank(width, height)
    for y in range(height):
        share = y / (height - 1)
        for (start, low), (end, high) in zip(SKY, SKY[1:]):
            if start <= share <= end:
                mix = (share - start) / (end - start)
                color = tuple(round(a + (b - a) * mix) for a, b in zip(low, high))
        for x in range(width):
            image.pixels[(y * width + x) * 4 : (y * width + x) * 4 + 4] = bytes((*color, 255))
    write(image, IMAGES / "sky.png")


def write_sea() -> None:
    write(tr.texture(read(tr.TILES / "terrain.png"), t.WATER), IMAGES / "sea.png")
