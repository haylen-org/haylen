"""Builds the pages of Haylen Pixel and writes them as BMFont files, in the text format and in the binary format of version 3."""

from __future__ import annotations

import struct
import sys
from dataclasses import dataclass
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[4] / "tools"))

from png_image import Image  # noqa: E402

import glyphs  # noqa: E402

Color = tuple[int, int, int, int]


@dataclass
class Char:
    code: int
    x: int
    y: int
    width: int
    height: int
    xoffset: int
    yoffset: int
    xadvance: int


def put(image: Image, x: int, y: int, color: Color) -> None:
    offset = (y * image.width + x) * 4
    image.pixels[offset : offset + 4] = bytes(color)


def white(row: int) -> Color:
    return 255, 255, 255, 255


def gold(row: int) -> Color:
    top, bottom = (255, 243, 160), (255, 138, 30)
    amount = row / (glyphs.CELL_HEIGHT - 1)
    return tuple(round(a + (b - a) * amount) for a, b in zip(top, bottom)) + (255,)


def build(color, outline: Color | None, page_width: int = 128, page_height: int = 64) -> tuple[Image, list[Char]]:
    """Packs every glyph on one page, cropped to its lit columns. An outline grows each glyph by one pixel on every side."""
    border = 1 if outline else 0
    image = Image.blank(page_width, page_height)
    chars = []
    x, y = 1, 1
    height = glyphs.CELL_HEIGHT + border * 2
    for character in glyphs.GLYPHS:
        first, last = glyphs.columns(character)
        width = last - first + 1
        if width <= 0:
            chars.append(Char(ord(character), 0, 0, 0, 0, 0, 0, 3))
            continue
        if x + width + border * 2 >= page_width:
            x, y = 1, y + height + 1
        lit = {(column - first + border, row + border) for column, row in glyphs.pixels(character)}
        if outline:
            for column, row in lit:
                for dx in (-1, 0, 1):
                    for dy in (-1, 0, 1):
                        if (column + dx, row + dy) not in lit:
                            put(image, x + column + dx, y + row + dy, outline)
        for column, row in lit:
            put(image, x + column, y + row, color(row - border))
        chars.append(Char(ord(character), x, y, width + border * 2, height, -border, -border, width + 1 + border))
        x += width + border * 2 + 1
    return image, chars


def kernings() -> list[tuple[int, int, int]]:
    return [(ord(first), ord(second), amount) for first, second, amount in glyphs.KERNINGS]


def write_text(path: Path, face: str, page: str, chars: list[Char], page_size: tuple[int, int]) -> None:
    lines = [
        f'info face="{face}" size={glyphs.CELL_HEIGHT} bold=0 italic=0 charset="" unicode=1 stretchH=100 smooth=0 aa=1 padding=0,0,0,0 spacing=1,1 outline=0',
        f"common lineHeight={glyphs.LINE_HEIGHT} base={glyphs.BASE} scaleW={page_size[0]} scaleH={page_size[1]} pages=1 packed=0 alphaChnl=0 redChnl=0 greenChnl=0 blueChnl=0",
        f'page id=0 file="{page}"',
        f"chars count={len(chars)}",
    ]
    for char in chars:
        lines.append(f"char id={char.code} x={char.x} y={char.y} width={char.width} height={char.height} xoffset={char.xoffset} yoffset={char.yoffset} xadvance={char.xadvance} page=0 chnl=15")
    pairs = kernings()
    lines.append(f"kernings count={len(pairs)}")
    for first, second, amount in pairs:
        lines.append(f"kerning first={first} second={second} amount={amount}")
    path.write_text("\n".join(lines) + "\n")


def write_binary(path: Path, face: str, page: str, chars: list[Char], page_size: tuple[int, int]) -> None:
    def block(kind: int, payload: bytes) -> bytes:
        return struct.pack("<BI", kind, len(payload)) + payload

    info = struct.pack("<hBBHBBBBBBBB", glyphs.CELL_HEIGHT, 0b10, 0, 100, 1, 0, 0, 0, 0, 1, 1, 1) + face.encode() + b"\0"
    common = struct.pack("<HHHHHBBBBB", glyphs.LINE_HEIGHT, glyphs.BASE, page_size[0], page_size[1], 1, 0, 0, 0, 0, 0)
    pages = page.encode() + b"\0"
    entries = b"".join(struct.pack("<IHHHHhhhBB", c.code, c.x, c.y, c.width, c.height, c.xoffset, c.yoffset, c.xadvance, 0, 15) for c in chars)
    pairs = b"".join(struct.pack("<IIh", first, second, amount) for first, second, amount in kernings())
    path.write_bytes(b"BMF\x03" + block(1, info) + block(2, common) + block(3, pages) + block(4, entries) + block(5, pairs))
