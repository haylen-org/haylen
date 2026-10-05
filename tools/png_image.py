"""Reads and writes 8-bit RGBA PNG images with the Python standard library alone, for the tools that draw the art of the samples."""

from __future__ import annotations

import struct
import zlib
from dataclasses import dataclass
from pathlib import Path

SIGNATURE = b"\x89PNG\r\n\x1a\n"


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
    width = height = 0
    while offset < len(data):
        length, kind = struct.unpack(">I4s", data[offset : offset + 8])
        chunk = data[offset + 8 : offset + 8 + length]
        offset += 12 + length
        if kind == b"IHDR":
            width, height, depth, color, _, _, interlace = struct.unpack(">IIBBBBB", chunk)
            if (depth, color, interlace) != (8, 6, 0):
                raise ValueError("Only 8-bit RGBA PNG images without interlacing are supported.")
        elif kind == b"IDAT":
            compressed += chunk
        elif kind == b"IEND":
            break

    raw = zlib.decompress(bytes(compressed))
    stride = width * 4
    pixels = bytearray(height * stride)
    previous = bytearray(stride)
    for row in range(height):
        start = row * (stride + 1)
        kind = raw[start]
        line = bytearray(raw[start + 1 : start + 1 + stride])
        for index in range(stride):
            left = line[index - 4] if index >= 4 else 0
            up = previous[index]
            corner = previous[index - 4] if index >= 4 else 0
            if kind == 1:
                line[index] = (line[index] + left) & 0xFF
            elif kind == 2:
                line[index] = (line[index] + up) & 0xFF
            elif kind == 3:
                line[index] = (line[index] + ((left + up) >> 1)) & 0xFF
            elif kind == 4:
                line[index] = (line[index] + _paeth(left, up, corner)) & 0xFF
        pixels[row * stride : (row + 1) * stride] = line
        previous = line
    return Image(width, height, pixels)


def write(image: Image, path: Path) -> None:
    stride = image.width * 4
    raw = b"".join(b"\x00" + bytes(image.pixels[row * stride : (row + 1) * stride]) for row in range(image.height))

    def chunk(kind: bytes, payload: bytes) -> bytes:
        return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF)

    header = struct.pack(">IIBBBBB", image.width, image.height, 8, 6, 0, 0, 0)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(SIGNATURE + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))
