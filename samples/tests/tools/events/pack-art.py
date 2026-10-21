"""Cuts the icons and the creatures of the Events tests out of the sheets painted for them.

The sheets are painted with the "codex" command line in the style of the README of the test project on flat magenta: `icons.png`, twelve icons in a grid of four columns and three rows, and `creatures.png`, the bat flapping in the top row and the dragon walking in the bottom one, two frames each. Run it with `python3 samples/tests/tools/events/pack-art.py <folder of the sheets>`. The icons land in cells of 128 pixels in `content/events/icons.png`, and every creature in cells of 256 in `content/events/<name>.png`, where both of its frames keep the place and the scale they were painted at, so the frames hold one pivot.
"""

from __future__ import annotations

import sys
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from paintings import bounds, cut, key, parts, read, write  # noqa: E402
from png_image import Image  # noqa: E402

CONTENT = Path(__file__).resolve().parents[2] / "content" / "events"
ICON = 128
ICON_ROOM = 118
ICON_GRID = (4, 3)
CREATURE = 256
CREATURE_ROOM = 236
# Each creature with its row of the sheet and where its frames rest in their cells: on the ground line or in the middle.
CREATURES = (("bat", 0, "middle"), ("dragon", 1, "ground"))


def inside(keyed: Image) -> tuple[int, int, int, int]:
    """Returns the box around the parts of a cell, leaving out the small parts at its edges, which are the tips of the icons painted in the cells around it."""
    found = parts(keyed)
    largest = max((box[2] - box[0]) * (box[3] - box[1]) for box in found)
    boxes = [box for box in found if (box[2] - box[0]) * (box[3] - box[1]) * 2 > largest or (box[0] > 2 and box[1] > 2 and box[2] < keyed.width - 2 and box[3] < keyed.height - 2)]
    return min(box[0] for box in boxes), min(box[1] for box in boxes), max(box[2] for box in boxes), max(box[3] for box in boxes)


def pack_icons(sheet: Image) -> None:
    columns, rows = ICON_GRID
    width, height = sheet.width // columns, sheet.height // rows
    result = Image.blank(ICON * columns, ICON * rows)
    for index in range(columns * rows):
        cell = sheet.crop(index % columns * width, index // columns * height, width, height)
        x0, y0, x1, y1 = inside(key(cell))
        factor = ICON_ROOM / max(x1 - x0, y1 - y0)
        icon = cut(cell, (x0, y0, x1, y1), factor)
        result.paste(icon, index % columns * ICON + (ICON - icon.width) // 2, index // columns * ICON + (ICON - icon.height) // 2)
    write(result, CONTENT / "icons.png")
    print("Wrote icons.png.")


def pack_creatures(sheet: Image) -> None:
    half = sheet.width // 2
    for name, row, rest in CREATURES:
        boxes = [bounds(key(sheet.crop(column * half, row * half, half, half)), 24) for column in range(2)]
        x0, y0 = min(box[0] for box in boxes), min(box[1] for box in boxes)
        x1, y1 = max(box[2] for box in boxes) + 1, max(box[3] for box in boxes) + 1
        factor = CREATURE_ROOM / max(x1 - x0, y1 - y0)
        result = Image.blank(CREATURE * 2, CREATURE)
        for column in range(2):
            frame = cut(sheet, (column * half + x0, row * half + y0, column * half + x1, row * half + y1), factor)
            top = CREATURE - 8 - frame.height if rest == "ground" else (CREATURE - frame.height) // 2
            result.paste(frame, column * CREATURE + (CREATURE - frame.width) // 2, max(0, top))
        write(result, CONTENT / f"{name}.png")
        print(f"Wrote {name}.png.")


def main() -> None:
    if len(sys.argv) != 2:
        sys.exit("Usage: pack-art.py <folder of icons.png and creatures.png>")
    folder = Path(sys.argv[1])
    CONTENT.mkdir(parents=True, exist_ok=True)
    pack_icons(read(folder / "icons.png"))
    pack_creatures(read(folder / "creatures.png"))


if __name__ == "__main__":
    main()
