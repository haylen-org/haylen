"""Reads 8-bit RGBA, RGB and palette PNG images and writes 8-bit RGBA ones with the Python standard library alone, for the tools that draw and check the art of the samples."""

from __future__ import annotations

import struct
import zlib
from dataclasses import dataclass
from pathlib import Path

SIGNATURE = b"\x89PNG\r\n\x1a\n"
# The bytes of one pixel of each color type the reader takes: RGB, palette and RGBA.
BYTES_PER_PIXEL = {2: 3, 3: 1, 6: 4}


@dataclass
class Image:
    width: int
    height: int
    pixels: bytearray

    @staticmethod
    def blank(width: int, height: int) -> Image:
        return Image(width, height, bytearray(width * height * 4))

    def alpha(self, x: int, y: int) -> int:
        return self.pixels[(y * self.width + x) * 4 + 3]

    def crop(self, left: int, top: int, width: int, height: int) -> Image:
        result = Image.blank(width, height)
        for row in range(height):
            start = ((top + row) * self.width + left) * 4
            result.pixels[row * width * 4 : (row + 1) * width * 4] = self.pixels[start : start + width * 4]
        return result

    def paste(self, source: Image, left: int, top: int) -> None:
        for row in range(source.height):
            start = ((top + row) * self.width + left) * 4
            self.pixels[start : start + source.width * 4] = source.pixels[row * source.width * 4 : (row + 1) * source.width * 4]

    def opaque_runs(self, axis: str) -> list[tuple[int, int]]:
        """Returns the `[start, end)` spans of columns or rows that hold at least one visible pixel."""
        count = self.width if axis == "x" else self.height
        other = self.height if axis == "x" else self.width
        filled = []
        for index in range(count):
            filled.append(any(self.alpha(index, cross) if axis == "x" else self.alpha(cross, index) for cross in range(other)))
        runs = []
        start = None
        for index, visible in enumerate(filled + [False]):
            if visible and start is None:
                start = index
            elif not visible and start is not None:
                runs.append((start, index))
                start = None
        return runs


def _paeth(left: int, up: int, corner: int) -> int:
    estimate = left + up - corner
    distances = (abs(estimate - left), abs(estimate - up), abs(estimate - corner))
    if distances[0] <= distances[1] and distances[0] <= distances[2]:
        return left
    return up if distances[1] <= distances[2] else corner


def read(path: Path | bytes) -> Image:
    data = path if isinstance(path, bytes) else Path(path).read_bytes()
    if not data.startswith(SIGNATURE):
        raise ValueError("The file is not a PNG image.")

    offset = len(SIGNATURE)
    compressed = bytearray()
    width = height = color = 0
    palette = b""
    transparency = b""
    while offset < len(data):
        length, kind = struct.unpack(">I4s", data[offset : offset + 8])
        chunk = data[offset + 8 : offset + 8 + length]
        offset += 12 + length
        if kind == b"IHDR":
            width, height, depth, color, _, _, interlace = struct.unpack(">IIBBBBB", chunk)
            if depth != 8 or color not in BYTES_PER_PIXEL or interlace:
                raise ValueError("Only 8-bit RGBA, RGB and palette PNG images without interlacing are supported.")
        elif kind == b"PLTE":
            palette = chunk
        elif kind == b"tRNS":
            transparency = chunk
        elif kind == b"IDAT":
            compressed += chunk
        elif kind == b"IEND":
            break

    raw = zlib.decompress(bytes(compressed))
    step = BYTES_PER_PIXEL[color]
    stride = width * step
    unfiltered = bytearray(height * stride)
    previous = bytearray(stride)
    for row in range(height):
        start = row * (stride + 1)
        kind = raw[start]
        line = bytearray(raw[start + 1 : start + 1 + stride])
        for index in range(stride):
            left = line[index - step] if index >= step else 0
            up = previous[index]
            corner = previous[index - step] if index >= step else 0
            if kind == 1:
                line[index] = (line[index] + left) & 0xFF
            elif kind == 2:
                line[index] = (line[index] + up) & 0xFF
            elif kind == 3:
                line[index] = (line[index] + ((left + up) >> 1)) & 0xFF
            elif kind == 4:
                line[index] = (line[index] + _paeth(left, up, corner)) & 0xFF
        unfiltered[row * stride : (row + 1) * stride] = line
        previous = line
    return Image(width, height, _rgba(unfiltered, color, palette, transparency))


def _rgba(unfiltered: bytearray, color: int, palette: bytes, transparency: bytes) -> bytearray:
    """Expands unfiltered RGB or palette pixels to RGBA, where the transparency chunk gives the alpha of the palette entries it lists."""
    if color == 6:
        return unfiltered
    pixels = bytearray(len(unfiltered) // BYTES_PER_PIXEL[color] * 4)
    if color == 2:
        for index in range(len(unfiltered) // 3):
            pixels[index * 4 : index * 4 + 4] = unfiltered[index * 3 : index * 3 + 3] + b"\xff"
        return pixels
    entries = [palette[index * 3 : index * 3 + 3] + bytes((transparency[index] if index < len(transparency) else 255,)) for index in range(len(palette) // 3)]
    for index, entry in enumerate(unfiltered):
        pixels[index * 4 : index * 4 + 4] = entries[entry]
    return pixels


def _filtered(line: bytes, previous: bytes) -> bytes:
    """Returns the row with the filter that leaves the smallest sum of its bytes as signed values, which is the row that compresses best, after the byte of that filter."""
    left = bytes(4) + line[:-4]
    corner = bytes(4) + previous[:-4]
    candidates = [
        bytes(line),
        bytes((value - before) & 0xFF for value, before in zip(line, left)),
        bytes((value - above) & 0xFF for value, above in zip(line, previous)),
        bytes((value - ((before + above) >> 1)) & 0xFF for value, before, above in zip(line, left, previous)),
        bytes((value - _paeth(before, above, diagonal)) & 0xFF for value, before, above, diagonal in zip(line, left, previous, corner)),
    ]
    costs = [sum(value if value < 128 else 256 - value for value in candidate) for candidate in candidates]
    kind = costs.index(min(costs))
    return bytes((kind,)) + candidates[kind]


def write(image: Image, path: Path) -> None:
    stride = image.width * 4
    rows = [bytes(image.pixels[row * stride : (row + 1) * stride]) for row in range(image.height)]
    raw = b"".join(_filtered(line, rows[index - 1] if index else bytes(stride)) for index, line in enumerate(rows))

    def chunk(kind: bytes, payload: bytes) -> bytes:
        return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF)

    header = struct.pack(">IIBBBBB", image.width, image.height, 8, 6, 0, 0, 0)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(SIGNATURE + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))
