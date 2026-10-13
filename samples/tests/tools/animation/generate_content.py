"""Draws the fighter of the Animation tests, four frames of a punch in a flat style with soft edges, and writes its atlas with a tag, a hitbox slice with the pivot of the feet, a point slice on the fist and the events of the frames.

Run it from anywhere with `python3 samples/tests/tools/animation/generate_content.py`. The same run always writes the same files into `samples/tests/content/animation`.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools" / "nodes"))

from generate_content import Sheet, rgb  # noqa: E402

sys.path.insert(0, str(Path(__file__).resolve().parents[4] / "tools"))

from png_image import write  # noqa: E402

CONTENT = Path(__file__).resolve().parents[2] / "content" / "animation"
FRAME = 96

# How far the fist reaches out of the shoulder on each frame of the punch.
REACH = [4, 10, 26, 16]


def fighter() -> Sheet:
    sheet = Sheet(FRAME * len(REACH), FRAME)
    outline, skin, shirt, trousers, glove = rgb("#1f2433"), rgb("#f0c49a"), rgb("#3d7fc4"), rgb("#2c3e5c"), rgb("#d9483b")
    for index, reach in enumerate(REACH):
        left = index * FRAME
        lean = reach * 0.1

        # The legs stand on the feet at the bottom middle of the frame, which the pivot marks.
        sheet.rounded(left + 34, 60, 11, 32, 5, trousers, outline)
        sheet.rounded(left + 50, 60, 11, 32, 5, trousers, outline)
        sheet.rounded(left + 30 + lean, 32, 34, 34, 10, shirt, outline)
        sheet.circle(left + 47 + lean, 22, 13, outline)
        sheet.circle(left + 47 + lean, 22, 11, skin)

        # The punching arm and its glove, whose middle is the hand point.
        sheet.rounded(left + 44 + lean, 40, 14 + reach, 9, 4, skin, outline)
        sheet.circle(left + 58 + lean + reach, 44, 8, outline)
        sheet.circle(left + 58 + lean + reach, 44, 6.5, glove)
    return sheet


def atlas() -> dict:
    frames = []
    for index in range(len(REACH)):
        frame = {"filename": f"punch {index}", "frame": {"x": index * FRAME, "y": 0, "w": FRAME, "h": FRAME}, "duration": 120}
        if index == 2:
            frame["data"] = "swing"
        frames.append(frame)
    frames[3]["duration"] = 200
    hand = [{"frame": index, "bounds": {"x": round(56 + reach * 1.1), "y": 42, "w": 4, "h": 4}} for index, reach in enumerate(REACH)]
    return {
        "frames": frames,
        "meta": {
            "image": "fighter.png",
            "size": {"w": FRAME * len(REACH), "h": FRAME},
            "frameTags": [{"name": "punch", "from": 0, "to": len(REACH) - 1, "direction": "forward"}],
            "slices": [
                {"name": "body", "keys": [{"frame": 0, "bounds": {"x": 30, "y": 8, "w": 34, "h": 84}, "pivot": {"x": 18, "y": 84}}]},
                {"name": "hit", "keys": [{"frame": 2, "bounds": {"x": 76, "y": 34, "w": 20, "h": 20}}, {"frame": 3, "bounds": {"x": 0, "y": 0, "w": 0, "h": 0}}]},
                {"name": "hand", "data": "point", "keys": hand},
            ],
        },
    }


def main() -> None:
    CONTENT.mkdir(parents=True, exist_ok=True)
    write(fighter().image, CONTENT / "fighter.png")
    (CONTENT / "fighter.json").write_text(json.dumps(atlas(), indent=4) + "\n")


if __name__ == "__main__":
    main()
