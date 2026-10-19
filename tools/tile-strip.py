"""Makes a painted strip of scenery repeat side by side without a seam, such as the hills or the ground of a parallax layer.

The strip overlaps its own two ends: in the overlap, every row takes the end of the strip up to a path and its start after it, and the path runs from top to bottom where the two ends differ least, so it passes through the sky, plain grass or plain earth instead of cutting a shape, and a few pixels around it blend. The result is the strip without the overlap, so its last column leads into its first. Run it with `python3 tools/tile-strip.py <painting> <strip> <trim> <overlap>`, where `trim` drops that many columns from both ends of the painting first, such as an edge that the painting smeared, and `overlap` is the width the ends share.
"""

from __future__ import annotations

import sys
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))

from png_image import Image, read, write  # noqa: E402

# The pixels on each side of the path that blend the two ends.
FEATHER = 2


def difference(first: bytearray, second: bytearray) -> int:
    alpha = abs(first[3] - second[3])
    color = sum(abs(first[channel] - second[channel]) for channel in range(3)) * max(first[3], second[3]) // 255
    return color + alpha * 2


def tile(painting: Image, trim: int, overlap: int) -> Image:
    width = painting.width - 2 * trim
    if overlap * 2 > width:
        sys.exit("The overlap needs to be at most half of the width that is left after the trim.")
    source = painting.crop(trim, 0, width, painting.height)

    def pixel(x: int, y: int) -> bytearray:
        return source.pixels[(y * width + x) * 4 : (y * width + x) * 4 + 4]

    # Each cell of the overlap compares the end of the strip with its start, and the cheapest path from the top row to the bottom row picks where the rows switch.
    cost = [[difference(pixel(width - overlap + x, y), pixel(x, y)) for x in range(overlap)] for y in range(painting.height)]
    total = [row[:] for row in cost]
    for y in range(1, painting.height):
        for x in range(overlap):
            total[y][x] += min(total[y - 1][max(0, x - 1) : x + 2])
    path = [0] * painting.height
    path[-1] = min(range(overlap), key=lambda x: total[-1][x])
    for y in range(painting.height - 2, -1, -1):
        below = path[y + 1]
        path[y] = min(range(max(0, below - 1), min(overlap, below + 2)), key=lambda x: total[y][x])

    result = Image.blank(width - overlap, painting.height)
    for y in range(painting.height):
        for x in range(result.width):
            if x >= overlap:
                color = pixel(x, y)
            else:
                share = min(1.0, max(0.0, (x - path[y] + FEATHER) / (2 * FEATHER)))
                end, start = pixel(width - overlap + x, y), pixel(x, y)
                color = bytes(round(end[channel] * (1 - share) + start[channel] * share) for channel in range(4))
            result.pixels[(y * result.width + x) * 4 : (y * result.width + x) * 4 + 4] = color
    return result


def main() -> None:
    if len(sys.argv) != 5:
        sys.exit("Name the painting, the strip, the trim and the overlap, such as \"python3 tools/tile-strip.py hills.png content/camera/hills.png 140 160\".")
    write(tile(read(Path(sys.argv[1])), int(sys.argv[3]), int(sys.argv[4])), Path(sys.argv[2]))


if __name__ == "__main__":
    main()
