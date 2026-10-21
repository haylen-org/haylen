"""Builds the fighter atlas and the training dummy of the Animation tests from their paintings.

The paintings are `fighter-sheet.png`, the four frames of a punch in a grid of two columns and two rows of cells of 768 by 512 pixels, and `dummy.png`, the training dummy alone, both on flat magenta and painted with the "codex" command line in the style of the README of the test project. Run it with `python3 samples/tests/tools/animation/pack-fighter.py <folder of the paintings>`.

The tool scales the frames so the fighter stands `HEIGHT` pixels tall, puts the feet of every frame on one line and the head and torso of the guard, the wind-up and the recovery over one point, while the punch keeps its feet where the guard has them and leans forward, and packs the frames side by side into `content/animation/fighter.png`. It writes `fighter.json` with the tag `punch`, the event `swing` of the third frame and the slices it measures on the frames: `body`, the box of the head and the legs of every frame with the pivot of the feet, `hit`, the box of the glove on the third frame, and `hand`, the point in the middle of the front glove. The dummy is scaled so the middle of its target sits at the height of the glove of the punch, into `content/animation/dummy.png`.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import paintings  # noqa: E402

CONTENT = Path(__file__).resolve().parents[2] / "content" / "animation"
CELL = (768, 512)
FRAMES = 4
PUNCH = 2
HEIGHT = 400
MARGIN = 4
DURATIONS = (120, 120, 120, 200)
# The outline and the cuff around the red of a glove, in pixels of a frame.
GLOVE_RIM = 16
# The height of the middle of the target of the painted dummy, as a share of the dummy from its foot.
TARGET = 0.59


def visible(frame) -> list[tuple[int, int]]:
    return [(x, y) for y in range(frame.height) for x in range(frame.width) if frame.alpha(x, y) > paintings.VISIBLE]


def anchor(frame, index: int) -> tuple[float, int]:
    """Returns the point of a frame that lines it up with the others: the middle of the head and torso above the feet, or for the punch, which leans forward, the middle of the feet."""
    lit = visible(frame)
    top, bottom = min(y for _, y in lit), max(y for _, y in lit)
    if index == PUNCH:
        xs = [x for x, y in lit if y >= bottom - (bottom - top) * 0.15]
    else:
        xs = [x for x, y in lit if y <= top + (bottom - top) * 0.55]
    return sum(xs) / len(xs), bottom


def gloves(frame) -> list[tuple[int, int, int, int]]:
    """Returns the boxes of the red gloves of a frame, from the back glove to the front one."""
    red = {(x, y) for x, y in visible(frame) if (lambda r, g, b: r > 170 and g < 90 and b < 90)(*frame.pixels[(y * frame.width + x) * 4 : (y * frame.width + x) * 4 + 3])}
    boxes = []
    while red:
        stack = [red.pop()]
        group = []
        while stack:
            x, y = stack.pop()
            group.append((x, y))
            for neighbour in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
                if neighbour in red:
                    red.remove(neighbour)
                    stack.append(neighbour)
        if len(group) > 200:
            xs, ys = [x for x, _ in group], [y for _, y in group]
            boxes.append((min(xs), min(ys), max(xs), max(ys)))
    return sorted(boxes, key=lambda box: box[0] + box[2])


def body(frame, glove: tuple[int, int, int, int]) -> tuple[int, int, int, int]:
    """Returns the box of the head and the legs of a frame, measured on the rows above and below the front glove, so the arm that reaches past them stays out, and without the teal tails of the headband that fly behind the head."""
    lit = visible(frame)
    top, bottom = min(y for _, y in lit), max(y for _, y in lit)
    xs = [x for x, y in lit if (y < glove[1] - GLOVE_RIM or y > glove[3] + GLOVE_RIM) and not (lambda r, g, b: g > 140 and b > 150 and r < 120)(*frame.pixels[(y * frame.width + x) * 4 : (y * frame.width + x) * 4 + 3])]
    return min(xs), top, max(xs), bottom


def main() -> None:
    if len(sys.argv) != 2:
        sys.exit('Name the folder of the paintings, such as "python3 pack-fighter.py build/art/anim".')
    folder = Path(sys.argv[1])
    painted = paintings.read(folder / "fighter-sheet.png")
    cells = [painted.crop(index % 2 * CELL[0], index // 2 * CELL[1], *CELL) for index in range(FRAMES)]
    standing = paintings.bounds(paintings.key(cells[0]))
    factor = HEIGHT / (standing[3] - standing[1] + 1)
    frames = [paintings.cut(cell, (0, 0, cell.width, cell.height), factor, margin=0) for cell in cells]

    # Every frame moves so its anchor lands on the anchor of the guard, and the frames share the smallest box that holds them all.
    guard_x, ground = anchor(frames[0], 0)
    offsets = []
    for index, frame in enumerate(frames):
        x, bottom = anchor(frame, index)
        if index == PUNCH:
            x += guard_x - anchor(frames[0], PUNCH)[0]
        offsets.append((round(guard_x - x), ground - bottom))
    boxes = [paintings.bounds(frame) for frame in frames]
    left = min(box[0] + dx for box, (dx, _) in zip(boxes, offsets)) - MARGIN
    top = min(box[1] + dy for box, (_, dy) in zip(boxes, offsets)) - MARGIN
    right = max(box[2] + dx for box, (dx, _) in zip(boxes, offsets)) + MARGIN
    width, height = right - left + 1, ground - top + 1 + MARGIN
    pivot = (round(anchor(frames[0], PUNCH)[0]) - left, ground - top)

    sheet = paintings.Image.blank(width * FRAMES, height)
    frame_entries, body_keys, hand_keys = [], [], []
    hit = None
    for index, (frame, (dx, dy)) in enumerate(zip(frames, offsets)):
        placed = paintings.Image.blank(width, height)
        for y in range(frame.height):
            for x in range(frame.width):
                tx, ty = x + dx - left, y + dy - top
                offset = (y * frame.width + x) * 4
                if frame.pixels[offset + 3] and 0 <= tx < width and 0 <= ty < height:
                    target = (ty * width + tx) * 4
                    placed.pixels[target : target + 4] = frame.pixels[offset : offset + 4]
        sheet.paste(placed, index * width, 0)

        front = gloves(placed)[-1]
        bx0, by0, bx1, by1 = body(placed, front)
        body_keys.append({"frame": index, "bounds": {"x": bx0, "y": by0, "w": bx1 - bx0 + 1, "h": by1 - by0 + 1}, "pivot": {"x": pivot[0] - bx0, "y": pivot[1] - by0}})
        middle = ((front[0] + front[2]) // 2, (front[1] + front[3]) // 2)
        hand_keys.append({"frame": index, "bounds": {"x": middle[0] - 2, "y": middle[1] - 2, "w": 4, "h": 4}})
        if index == PUNCH:
            hit = {"x": front[0], "y": front[1], "w": front[2] - front[0] + 1, "h": front[3] - front[1] + 1}
            glove_height = pivot[1] - middle[1]
        frame_entries.append({"filename": f"punch {index}", "frame": {"x": index * width, "y": 0, "w": width, "h": height}, "duration": DURATIONS[index]})
    frame_entries[PUNCH]["data"] = "swing"
    paintings.write(sheet, CONTENT / "fighter.png")

    atlas = {
        "frames": frame_entries,
        "meta": {
            "image": "fighter.png",
            "size": {"w": sheet.width, "h": sheet.height},
            "frameTags": [{"name": "punch", "from": 0, "to": FRAMES - 1, "direction": "forward"}],
            "slices": [
                {"name": "body", "keys": body_keys},
                {"name": "hit", "keys": [{"frame": PUNCH, "bounds": hit}, {"frame": PUNCH + 1, "bounds": {"x": 0, "y": 0, "w": 0, "h": 0}}]},
                {"name": "hand", "data": "point", "keys": hand_keys},
            ],
        },
    }
    (CONTENT / "fighter.json").write_text(json.dumps(atlas, indent=4) + "\n")

    dummy = paintings.read(folder / "dummy.png")
    standing = paintings.bounds(paintings.key(dummy))
    scale = glove_height / TARGET / (standing[3] - standing[1] + 1)
    paintings.write(paintings.trimmed(paintings.cut(dummy, standing, scale)), CONTENT / "dummy.png")


if __name__ == "__main__":
    main()
