"""Generates the art of the UI sample: the parchment theme with its atlas, the item icons, the landscapes, the avatars and the wooden sign.

Run it from anywhere with `python3 samples/interface/ui/tools/generate_content.py`. The same run always writes the same files.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

# The tool runs from the sample folder, which keeps no compiled Python files.
sys.dont_write_bytecode = True

import art  # noqa: E402
import theme  # noqa: E402

CONTENT = Path(__file__).resolve().parents[1] / "content"


def main() -> None:
    atlas, document = theme.build()
    atlas.save(CONTENT / "themes" / "parchment.png")
    (CONTENT / "themes" / "parchment.json").write_text(json.dumps(document, indent=4) + "\n")

    for name in art.ICONS:
        art.icon(name).save(CONTENT / "icons" / f"{name}.png")
    for name in art.PALETTES:
        art.landscape(name).save(CONTENT / "images" / f"landscape_{name}.png")
    art.avatar("#f1c27d", "#4e342e", "#90caf9").save(CONTENT / "images" / "avatar_ana.png")
    art.avatar("#8d5524", "#212121", "#ffcc80").save(CONTENT / "images" / "avatar_leo.png")
    art.sign(1.0, 0).save(CONTENT / "images" / "sign.png")
    art.sign(1.15, 0).save(CONTENT / "images" / "sign_hover.png")
    art.sign(0.85, 3).save(CONTENT / "images" / "sign_pressed.png")


if __name__ == "__main__":
    main()
