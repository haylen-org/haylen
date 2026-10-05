"""Generates the drawn content of the text tests under `content/text`: Haylen Pixel as a ".fnt" bitmap font in the text format and, with gold glyphs and an outline, in the binary format, the LCD digits of the grid font, the button prompts and the pictures of rich text.

Run it from anywhere with `python3 samples/tests/tools/text/generate_content.py`. The same run always writes the same files.
"""

from __future__ import annotations

import sys
from pathlib import Path

# The tool runs from the folder of the test project, which keeps no compiled Python files.
sys.dont_write_bytecode = True

import art  # noqa: E402
import bitmap_font  # noqa: E402
from png_image import write  # noqa: E402

CONTENT = Path(__file__).resolve().parents[2] / "content" / "text"
PAGE_SIZE = (128, 64)


def main() -> None:
    page, chars = bitmap_font.build(bitmap_font.white, None, *PAGE_SIZE)
    write(page, CONTENT / "haylen_pixel_0.png")
    bitmap_font.write_text(CONTENT / "haylen_pixel.fnt", "Haylen Pixel", "haylen_pixel_0.png", chars, PAGE_SIZE)

    page, chars = bitmap_font.build(bitmap_font.gold, (59, 26, 0, 255), *PAGE_SIZE)
    write(page, CONTENT / "haylen_pixel_gold_0.png")
    bitmap_font.write_binary(CONTENT / "haylen_pixel_gold.fnt", "Haylen Pixel Gold", "haylen_pixel_gold_0.png", chars, PAGE_SIZE)

    write(art.lcd_digits(), CONTENT / "lcd_digits.png")
    write(art.prompts(), CONTENT / "prompts.png")
    write(art.coin(), CONTENT / "coin.png")
    write(art.heart(), CONTENT / "heart.png")
    write(art.star(), CONTENT / "star.png")


if __name__ == "__main__":
    main()
