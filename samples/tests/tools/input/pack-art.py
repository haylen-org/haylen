"""Cuts the pictures of the Input tests out of the sheets painted for them.

The sheets are painted with the "codex" command line in the style of the README of the test project on flat magenta: `devices.png`, a near white gamepad and a near white keyboard with its mouse side by side, which the tests tint, and `platform.png`, a floating platform of grass and earth whose middle stretches. Run it with `python3 samples/tests/tools/input/pack-art.py <folder of the sheets>`. It keys out the magenta, crops every picture to what it shows with a margin of a few transparent pixels and scales it so its longer side has the size of its entry, in `content/input/<name>.png`.
"""

from __future__ import annotations

import sys
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from paintings import cut, key, parts, read, trimmed, write  # noqa: E402

CONTENT = Path(__file__).resolve().parents[2] / "content" / "input"
# Each sheet with the pictures it holds in equal cells from the left, and the longer side each one is scaled to.
SHEETS = (("devices.png", (("gamepad", 384), ("keyboard", 384))), ("platform.png", (("platform", 768),)))


def main() -> None:
    if len(sys.argv) != 2:
        sys.exit("Usage: pack-art.py <folder of devices.png and platform.png>")
    folder = Path(sys.argv[1])
    CONTENT.mkdir(parents=True, exist_ok=True)
    for name, pictures in SHEETS:
        sheet = read(folder / name)
        width = sheet.width // len(pictures)
        for index, (picture, longest) in enumerate(pictures):
            cell = sheet.crop(index * width, 0, width, sheet.height)
            boxes = parts(key(cell))
            box = (min(box[0] for box in boxes), min(box[1] for box in boxes), max(box[2] for box in boxes), max(box[3] for box in boxes))
            factor = longest / max(box[2] - box[0], box[3] - box[1])
            write(trimmed(cut(cell, box, factor), 4), CONTENT / f"{picture}.png")
            print(f"Wrote {picture}.png.")


if __name__ == "__main__":
    main()
