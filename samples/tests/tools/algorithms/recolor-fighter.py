"""Paints the red team of the overlap boxes test of the Algorithms tests from the fighter of the Animation tests.

The red team wears red where the fighter wears blue, its headband and its top, and blue gloves where the fighter wears red ones, so the two teams read apart at a glance while they keep one drawing. Run it with `python3 samples/tests/tools/algorithms/recolor-fighter.py` whenever the fighter changes. It reads `content/animation/fighter.png` and writes `content/algorithms/fighter-red.png` with the same frames.
"""

from __future__ import annotations

import colorsys
import sys
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[4] / "tools"))

import png_image  # noqa: E402

CONTENT = Path(__file__).resolve().parents[2] / "content"

# The hues of the blue cloth and of the red gloves, in degrees, and the turns that swap them. Skin, hair, shorts and shoes keep their colors.
BLUE = (180.0, 250.0)
RED_BELOW, RED_ABOVE = 14.0, 342.0
BLUE_TO_RED = 150.0
RED_TO_BLUE = 218.0


def recolor(red: int, green: int, blue: int) -> tuple[int, int, int]:
    hue, saturation, value = colorsys.rgb_to_hsv(red / 255.0, green / 255.0, blue / 255.0)
    degrees = hue * 360.0
    if BLUE[0] <= degrees <= BLUE[1] and saturation > 0.3:
        degrees = (degrees + BLUE_TO_RED) % 360.0
    elif (degrees <= RED_BELOW or degrees >= RED_ABOVE) and saturation > 0.45 and value > 0.2:
        degrees = (degrees + RED_TO_BLUE) % 360.0
    else:
        return red, green, blue
    turned = colorsys.hsv_to_rgb(degrees / 360.0, saturation, value)
    return tuple(round(channel * 255.0) for channel in turned)


def main() -> None:
    image = png_image.read(CONTENT / "animation" / "fighter.png")
    pixels = image.pixels
    for index in range(0, len(pixels), 4):
        if pixels[index + 3] > 0:
            pixels[index], pixels[index + 1], pixels[index + 2] = recolor(pixels[index], pixels[index + 1], pixels[index + 2])
    png_image.write(image, CONTENT / "algorithms" / "fighter-red.png")
    print(f"Wrote {CONTENT / 'algorithms' / 'fighter-red.png'} ({image.width}x{image.height}).")


if __name__ == "__main__":
    main()
