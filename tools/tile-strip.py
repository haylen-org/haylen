"""Makes a painted strip of scenery repeat side by side without a seam, such as the hills or the ground of a parallax layer.

The strip overlaps its own two ends: in the overlap, every row takes the end of the strip up to a path and its start after it, and the path runs from top to bottom where the two ends differ least, so it passes through the sky, plain grass or plain earth instead of cutting a shape, and a few pixels around it blend. The result is the strip without the overlap, so its last column leads into its first. Run it with `uv run tools/tile-strip.py <painting> <strip> <trim> <overlap>`, where `trim` drops that many columns from both ends of the painting first, such as an edge that the painting smeared, and `overlap` is the width the ends share.
"""

import sys
from pathlib import Path

from png_image import Image


class TileStrip:
    """Joins the two ends of a painted strip along the path where they differ least."""

    # The pixels on each side of the path that blend the two ends.
    FEATHER = 2

    @staticmethod
    def difference(first: bytes, second: bytes) -> int:
        alpha = abs(first[3] - second[3])
        color = sum(abs(first[channel] - second[channel]) for channel in range(3)) * max(first[3], second[3]) // 255
        return color + alpha * 2

    @staticmethod
    def pixel(source: Image, x: int, y: int) -> bytes:
        start = (y * source.width + x) * 4
        return bytes(source.pixels[start : start + 4])

    @staticmethod
    def seam(source: Image, overlap: int) -> list[int]:
        """The column of the overlap where each row switches from the end of the strip to its start: the cheapest path from the top row to the bottom row of the differences of the two ends."""
        width, height = source.width, source.height
        cost = [[TileStrip.difference(TileStrip.pixel(source, width - overlap + x, y), TileStrip.pixel(source, x, y)) for x in range(overlap)] for y in range(height)]
        total = [row[:] for row in cost]
        for y in range(1, height):
            for x in range(overlap):
                total[y][x] += min(total[y - 1][max(0, x - 1) : x + 2])
        path = [0] * height
        path[-1] = min(range(overlap), key=lambda x: total[-1][x])
        for y in range(height - 2, -1, -1):
            below = path[y + 1]
            path[y] = min(range(max(0, below - 1), min(overlap, below + 2)), key=lambda x: total[y][x])
        return path

    @staticmethod
    def tile(painting: Image, trim: int, overlap: int) -> Image:
        width = painting.width - 2 * trim
        if overlap * 2 > width:
            sys.exit("The overlap needs to be at most half of the width that is left after the trim.")
        source = painting.crop(trim, 0, width, painting.height)
        path = TileStrip.seam(source, overlap)

        result = Image.blank(width - overlap, painting.height)
        for y in range(painting.height):
            for x in range(result.width):
                if x >= overlap:
                    color = TileStrip.pixel(source, x, y)
                else:
                    share = min(1.0, max(0.0, (x - path[y] + TileStrip.FEATHER) / (2 * TileStrip.FEATHER)))
                    end, start = TileStrip.pixel(source, width - overlap + x, y), TileStrip.pixel(source, x, y)
                    color = bytes(round(end[channel] * (1 - share) + start[channel] * share) for channel in range(4))
                result.pixels[(y * result.width + x) * 4 : (y * result.width + x) * 4 + 4] = color
        return result

    @staticmethod
    def main() -> None:
        if len(sys.argv) != 5:
            sys.exit('Name the painting, the strip, the trim and the overlap, such as "uv run tools/tile-strip.py hills.png content/camera/hills.png 140 160".')
        TileStrip.tile(Image.read(Path(sys.argv[1])), int(sys.argv[3]), int(sys.argv[4])).write(Path(sys.argv[2]))


if __name__ == "__main__":
    TileStrip.main()
