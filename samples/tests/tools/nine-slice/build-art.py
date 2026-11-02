"""Builds the frames of the Nine-slice tests from the two frames painted for them.

The sheet `frames.png` is painted with the "codex" command line in the style of the README of the test project on flat magenta: a wooden frame around plain parchment in the left half and the same frame in near white in the right half. Run it with `python3 samples/tests/tools/nine-slice/build-art.py <folder of frames.png>`. It keys out the magenta, scales each frame to 256 pixels and repaints the straight sides of its border so they meet the corner blocks and repeat without a seam, with their painted grain fading into a plain bar toward both ends, and writes into `content/nine-slice/`:

- `panel.png`, the wooden frame around parchment with diagonal stripes whose period divides the middle, so the middle tiles without a seam while stretching widens its stripes;
- `parchment.png`, the wooden frame around plain parchment, for the panels that hold text;
- `pieces.png`, the wooden frame at 384 pixels around the same diagonal stripes, nine periods across its middle, cut into nine pieces apart in a sheet with transparent gaps;
- `frame-white.png`, the near white frame, which tints to any color;
- `cards.png` and `cards.json`, an atlas whose frame `card`, the plain frame at 96 pixels, was trimmed out of a canvas of 128 with a margin of 16 and packed next to a checkered frame, with the bounds of the slice `card` in the coordinates of the whole canvas and its center in the coordinates of its bounds.

Scaling the card down blends a few pixels of its outline toward the magenta it was painted on, so run `python3 tools/check-image-edges.py --clean samples/tests/content/nine-slice/cards.png` after it.
"""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from paintings import key, parts, read, scaled, write  # noqa: E402
from png_image import Image  # noqa: E402

CONTENT = Path(__file__).resolve().parents[2] / "content" / "nine-slice"
SIZE = 256
# The border of each frame at 256 pixels, which holds its wood or bevel and the shadow along its inner edge, where the block of each corner ends and how far the bar reaches across the border.
WOOD_BORDER = 50
WOOD_CORNER = 43
WOOD_BAR = 40
WHITE_BORDER = 46
WHITE_CORNER = 42
WHITE_BAR = 38
# The painted grain of each side fades out over this many pixels toward its ends.
FADE = 16
# Diagonal stripes of this period in pixels repeat six times across the middle of the wooden frame and nine times across the middle of the frame cut into pieces.
STRIPE = 26
STRIPE_COLORS = ((243, 230, 196), (232, 211, 166))
PIECES_SIZE = 384
PIECES_GAP = 16
CARD = 96
CARD_CANVAS = 128
CARD_BORDER = 19
CHECKER = (232, 69, 60), (30, 33, 56)


def frame(sheet: Image, cell: int, size: int = SIZE) -> Image:
    """Keys the frame of a cell of the sheet and scales it to `size` pixels on both sides, so its four borders keep the same thickness."""
    half = sheet.height
    painted = sheet.crop(cell * half, 0, half, half)
    boxes = parts(key(painted))
    left, top = min(b[0] for b in boxes), min(b[1] for b in boxes)
    right, bottom = max(b[2] for b in boxes), max(b[3] for b in boxes)
    with tempfile.TemporaryDirectory() as folder:
        source, target = Path(folder) / "frame.png", Path(folder) / "scaled.png"
        write(painted.crop(left, top, right - left, bottom - top), source)
        subprocess.run(["sips", "-z", str(size), str(size), str(source), "--out", str(target)], check=True, capture_output=True)
        return key(read(target))


def pixel(image: Image, x: int, y: int) -> list[int]:
    offset = (y * image.width + x) * 4
    return list(image.pixels[offset : offset + 4])


def put(image: Image, x: int, y: int, value: list[int]) -> None:
    offset = (y * image.width + x) * 4
    image.pixels[offset : offset + 4] = bytes(value)


def place(side: int, size: int, along: int, across: int) -> tuple[int, int]:
    """Maps a point of side `side` of a square border, top, bottom, left or right, from its distance along the side and across it from the outer edge to the image."""
    return [(along, across), (along, size - 1 - across), (across, along), (size - 1 - across, along)][side]


def plain_ends(image: Image, border: int, corner: int, bar: int) -> Image:
    """Repaints each straight side of the border so it meets its corners and itself without a seam.

    Each side becomes the average of its rows across, the plain bar, plus its painted grain, which fades to nothing over `FADE` pixels at both ends of the side. The stubs of bar between the edge of each corner block at `corner` and the border line, across the `bar` pixels of the bar, take the plain bar too. So every corner meets a plain bar, and a side that repeats meets itself plain to plain. Inside each corner, the shadows of its two sides meet without a step.
    """
    result = Image(image.width, image.height, bytearray(image.pixels))
    size = image.width
    length = size - border * 2
    profiles = []
    for side in range(4):
        profile = []
        for across in range(border):
            values = [pixel(image, *place(side, size, border + along, across)) for along in range(length)]
            profile.append([sum(value[channel] for value in values) / length for channel in range(4)])
        profiles.append(profile)
        for across in range(border):
            for along in range(length):
                fade = min(1.0, min(along, length - 1 - along) / FADE)
                painted = pixel(image, *place(side, size, border + along, across))
                put(result, *place(side, size, border + along, across), [round(min(255, max(0, base + (value - base) * fade))) for base, value in zip(profile[across], painted)])
        for across in range(bar):
            for along in list(range(corner, border)) + list(range(size - border, size - corner)):
                put(result, *place(side, size, along, across), [round(value) for value in profile[across]])

    # Inside each corner, where the shadows of two sides meet, the darker of the two keeps both sides going into the corner.
    for horizontal, vertical, mirrored in ((0, 2, False), (0, 3, True), (1, 2, False), (1, 3, True)):
        for across in range(corner, border):
            for along in range(corner, border):
                shade = [round(min(a, b)) for a, b in zip(profiles[horizontal][across][:3], profiles[vertical][along][:3])]
                put(result, *place(horizontal, size, size - 1 - along if mirrored else along, across), shade + [255])
    return result


def striped(image: Image, border: int) -> Image:
    result = Image(image.width, image.height, bytearray(image.pixels))
    for y in range(border, image.height - border):
        for x in range(border, image.width - border):
            phase = ((x - border) + (y - border)) % STRIPE / STRIPE
            # The edges of the stripes blend over one pixel, so they stay smooth when the screen scales them.
            edge = min(1.0, max(0.0, (abs(phase - 0.5) - 0.25) * STRIPE + 0.5))
            light, dark = STRIPE_COLORS
            put(result, x, y, [round(dark[i] + (light[i] - dark[i]) * edge) for i in range(3)] + [255])
    return result


def pieces(image: Image, border: int) -> Image:
    middle = image.width - border * 2
    spans = ((0, border), (border, middle), (border + middle, border))
    width = border * 2 + middle + PIECES_GAP * 2
    result = Image.blank(width, width)
    for row, (top, height) in enumerate(spans):
        for column, (left, length) in enumerate(spans):
            result.paste(image.crop(left, top, length, height), left + column * PIECES_GAP, top + row * PIECES_GAP)
    return result


def cards(plain: Image) -> None:
    card = scaled(plain, CARD / plain.width)
    checker = Image.blank(CARD, CARD)
    for y in range(CARD):
        for x in range(CARD):
            put(checker, x, y, list(CHECKER[(x // 8 + y // 8) % 2]) + [255])
    atlas = Image.blank(200, 100)
    atlas.paste(card, 2, 2)
    atlas.paste(checker, 102, 2)
    write(atlas, CONTENT / "cards.png")
    margin = (CARD_CANVAS - CARD) // 2
    document = {
        "frames": [
            {"filename": "card", "frame": {"x": 2, "y": 2, "w": CARD, "h": CARD}, "rotated": False, "trimmed": True, "spriteSourceSize": {"x": margin, "y": margin, "w": CARD, "h": CARD}, "sourceSize": {"w": CARD_CANVAS, "h": CARD_CANVAS}, "duration": 100},
            {"filename": "checker", "frame": {"x": 102, "y": 2, "w": CARD, "h": CARD}, "rotated": False, "trimmed": False, "spriteSourceSize": {"x": 0, "y": 0, "w": CARD, "h": CARD}, "sourceSize": {"w": CARD, "h": CARD}, "duration": 100},
        ],
        "meta": {
            "image": "cards.png",
            "format": "RGBA8888",
            "size": {"w": 200, "h": 100},
            "scale": "1",
            "slices": [{"name": "card", "color": "#0000ffff", "keys": [{"frame": 0, "bounds": {"x": margin, "y": margin, "w": CARD, "h": CARD}, "center": {"x": CARD_BORDER, "y": CARD_BORDER, "w": CARD - CARD_BORDER * 2, "h": CARD - CARD_BORDER * 2}}]}],
        },
    }
    (CONTENT / "cards.json").write_text(json.dumps(document, indent=1) + "\n")


def main() -> None:
    if len(sys.argv) != 2:
        sys.exit("Usage: build-art.py <folder of frames.png>")
    sheet = read(Path(sys.argv[1]) / "frames.png")
    CONTENT.mkdir(parents=True, exist_ok=True)
    wood = plain_ends(frame(sheet, 0), WOOD_BORDER, WOOD_CORNER, WOOD_BAR)
    write(striped(wood, WOOD_BORDER), CONTENT / "panel.png")
    write(wood, CONTENT / "parchment.png")
    factor = PIECES_SIZE / SIZE
    border = round(WOOD_BORDER * factor)
    write(pieces(striped(plain_ends(frame(sheet, 0, PIECES_SIZE), border, round(WOOD_CORNER * factor), round(WOOD_BAR * factor)), border), border), CONTENT / "pieces.png")
    write(plain_ends(frame(sheet, 1), WHITE_BORDER, WHITE_CORNER, WHITE_BAR), CONTENT / "frame-white.png")
    cards(wood)
    print("Wrote panel.png, parchment.png, pieces.png, frame-white.png, cards.png and cards.json.")


if __name__ == "__main__":
    main()
