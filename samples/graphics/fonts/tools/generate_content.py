"""Generates the drawn content of the fonts sample: Haylen Pixel as a BMFont in the text format and, with gold glyphs and an outline, in the binary format, the LCD digits of the grid font, the button prompts and the pictures of rich text.

Run it from anywhere with `python3 samples/graphics/fonts/tools/generate_content.py`. The same run always writes the same files.
"""

from __future__ import annotations

import sys
from pathlib import Path

# The tool runs from the sample folder, which keeps no compiled Python files.
sys.dont_write_bytecode = True

import art  # noqa: E402
import bmfont  # noqa: E402
from png_image import write  # noqa: E402

CONTENT = Path(__file__).resolve().parents[1] / "content"
PAGE_SIZE = (128, 64)


def main() -> None:
    fonts = CONTENT / "fonts"
    page, chars = bmfont.build(bmfont.white, None, *PAGE_SIZE)
    write(page, fonts / "haylen_pixel_0.png")
    bmfont.write_text(fonts / "haylen_pixel.fnt", "Haylen Pixel", "haylen_pixel_0.png", chars, PAGE_SIZE)

    page, chars = bmfont.build(bmfont.gold, (59, 26, 0, 255), *PAGE_SIZE)
    write(page, fonts / "haylen_pixel_gold_0.png")
    bmfont.write_binary(fonts / "haylen_pixel_gold.fnt", "Haylen Pixel Gold", "haylen_pixel_gold_0.png", chars, PAGE_SIZE)

    write(art.lcd_digits(), fonts / "lcd_digits.png")
    write(art.prompts(), CONTENT / "images" / "prompts.png")
    write(art.coin(), CONTENT / "images" / "coin.png")
    write(art.heart(), CONTENT / "images" / "heart.png")
    write(art.star(), CONTENT / "images" / "star.png")


if __name__ == "__main__":
    main()
