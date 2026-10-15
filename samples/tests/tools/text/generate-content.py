"""Generates the drawn content of the text tests under `content/text`: Haylen Pixel as a ".fnt" bitmap font in the text format and, with gold glyphs and an outline, in the binary format, and the LCD digits of the grid font. The button prompts and the pictures of rich text are painted for the project with the "codex" command line, as the art guide in the README of the test project describes.

Run it from anywhere with `python3 samples/tests/tools/text/generate-content.py`. The same run always writes the same files.
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
    write(page, CONTENT / "haylen-pixel-0.png")
    bitmap_font.write_text(CONTENT / "haylen-pixel.fnt", "Haylen Pixel", "haylen-pixel-0.png", chars, PAGE_SIZE)

    page, chars = bitmap_font.build(bitmap_font.gold, (59, 26, 0, 255), *PAGE_SIZE)
    write(page, CONTENT / "haylen-pixel-gold-0.png")
    bitmap_font.write_binary(CONTENT / "haylen-pixel-gold.fnt", "Haylen Pixel Gold", "haylen-pixel-gold-0.png", chars, PAGE_SIZE)

    write(art.lcd_digits(), CONTENT / "lcd-digits.png")


if __name__ == "__main__":
    main()
