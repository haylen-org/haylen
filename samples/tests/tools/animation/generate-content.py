"""Writes the atlas of the fighter of the Animation tests, with a tag, a hitbox slice with the pivot of the feet, a point slice on the fist and the events of the frames, and the atlas and the skeleton of the cutout puppet of the skeleton test.

The images `fighter.png`, four frames of a punch, and `puppet.png`, the cutout parts in the cells of PARTS, are painted for the project with the "codex" command line, as the art guide in the README of the test project describes, and the slices below follow the painted frames. Run it from anywhere with `python3 samples/tests/tools/animation/generate-content.py`. The same run always writes the same files into `samples/tests/content/animation`.
"""

from __future__ import annotations

import json
from pathlib import Path

CONTENT = Path(__file__).resolve().parents[2] / "content" / "animation"
FRAME = 96
FRAMES = 4
# The middle of the punching glove on each frame of the punch.
HAND = [(58, 53), (60, 53), (78, 52), (59, 53)]


def atlas() -> dict:
    frames = []
    for index in range(FRAMES):
        frame = {"filename": f"punch {index}", "frame": {"x": index * FRAME, "y": 0, "w": FRAME, "h": FRAME}, "duration": 120}
        if index == 2:
            frame["data"] = "swing"
        frames.append(frame)
    frames[3]["duration"] = 200
    hand = [{"frame": index, "bounds": {"x": x - 2, "y": y - 2, "w": 4, "h": 4}} for index, (x, y) in enumerate(HAND)]
    return {
        "frames": frames,
        "meta": {
            "image": "fighter.png",
            "size": {"w": FRAME * FRAMES, "h": FRAME},
            "frameTags": [{"name": "punch", "from": 0, "to": FRAMES - 1, "direction": "forward"}],
            "slices": [
                {"name": "body", "keys": [{"frame": 0, "bounds": {"x": 18, "y": 6, "w": 40, "h": 88}, "pivot": {"x": 20, "y": 88}}]},
                {"name": "hit", "keys": [{"frame": 2, "bounds": {"x": 70, "y": 42, "w": 20, "h": 20}}, {"frame": 3, "bounds": {"x": 0, "y": 0, "w": 0, "h": 0}}]},
                {"name": "hand", "data": "point", "keys": hand},
            ],
        },
    }


# The cutout parts of the puppet, each in a cell of its own: the place of the cell and the size of the part.
PARTS = {
    "torso": (0, 0, 44, 64),
    "head": (48, 0, 44, 44),
    "upperArm": (96, 0, 34, 14),
    "forearm": (96, 16, 32, 12),
    "thigh": (136, 0, 16, 38),
    "shin": (156, 0, 14, 36),
    "cap": (176, 0, 48, 22),
    "crown": (176, 24, 44, 26),
    "cape": (0, 68, 40, 56),
}


def puppet_atlas() -> dict:
    frames = {name: {"frame": {"x": x, "y": y, "w": w, "h": h}} for name, (x, y, w, h) in PARTS.items()}
    return {"frames": frames, "meta": {"image": "puppet.png", "size": {"w": 256, "h": 128}}}


def region(frame: str, x: float = 0, y: float = 0, rotation: float = 0) -> dict:
    entry = {"frame": frame}
    for key, value in (("x", x), ("y", y), ("rotation", rotation)):
        if value:
            entry[key] = value
    return entry


def rotations(keys: list[tuple[float, float]]) -> list[dict]:
    return [{"time": time, "value": value, "curve": "sineInOut"} for time, value in keys]


def puppet_skeleton() -> dict:
    """The puppet stands on its root at the feet, with limbs that point down and a cape mesh of 2 by 3 cells behind the torso."""
    bones = [
        {"name": "root"},
        {"name": "hip", "parent": "root", "y": -74},
        {"name": "torso", "parent": "hip", "length": 64},
        {"name": "head", "parent": "torso", "y": -62},
        {"name": "backUpper", "parent": "torso", "x": -2, "y": -52, "rotation": 90, "length": 30},
        {"name": "backFore", "parent": "backUpper", "x": 30, "length": 28},
        {"name": "frontUpper", "parent": "torso", "x": 2, "y": -52, "rotation": 90, "length": 30},
        {"name": "frontFore", "parent": "frontUpper", "x": 30, "length": 28},
        {"name": "backThigh", "parent": "hip", "x": -4, "rotation": 90, "length": 36},
        {"name": "backShin", "parent": "backThigh", "x": 36, "length": 34},
        {"name": "frontThigh", "parent": "hip", "x": 4, "rotation": 90, "length": 36},
        {"name": "frontShin", "parent": "frontThigh", "x": 36, "length": 34},
        {"name": "aim", "parent": "root", "x": 80, "y": -60},
    ]
    shade = "#FFC8C8C8"
    slots = [
        {"name": "backUpper", "bone": "backUpper", "attachment": "upperArm", "color": shade},
        {"name": "backFore", "bone": "backFore", "attachment": "forearm", "color": shade},
        {"name": "backThigh", "bone": "backThigh", "attachment": "thigh", "color": shade},
        {"name": "backShin", "bone": "backShin", "attachment": "shin", "color": shade},
        {"name": "cape", "bone": "torso", "attachment": "cape"},
        {"name": "frontThigh", "bone": "frontThigh", "attachment": "thigh"},
        {"name": "frontShin", "bone": "frontShin", "attachment": "shin"},
        {"name": "torso", "bone": "torso", "attachment": "torso"},
        {"name": "head", "bone": "head", "attachment": "head"},
        {"name": "hat", "bone": "head", "attachment": "hat"},
        {"name": "frontUpper", "bone": "frontUpper", "attachment": "upperArm"},
        {"name": "frontFore", "bone": "frontFore", "attachment": "forearm"},
        {"name": "hand", "bone": "frontFore", "attachment": "hand"},
    ]
    columns, rows = 2, 3
    vertices, uvs, triangles = [], [], []
    for row in range(rows + 1):
        for column in range(columns + 1):
            vertices += [-20 + 40 * column / columns, -60 + 58 * row / rows]
            uvs += [column / columns, row / rows]
    for row in range(rows):
        for column in range(columns):
            first = row * (columns + 1) + column
            below = first + columns + 1
            triangles += [first, first + 1, below + 1, first, below + 1, below]

    def flap(amount: float) -> list[float]:
        return [value for row in range(rows + 1) for column in range(columns + 1) for value in (-amount * (row / rows) ** 2, 0)]

    default = {
        "backUpper": {"upperArm": region("upperArm", x=15)},
        "backFore": {"forearm": region("forearm", x=14)},
        "frontUpper": {"upperArm": region("upperArm", x=15)},
        "frontFore": {"forearm": region("forearm", x=14)},
        "backThigh": {"thigh": region("thigh", x=18, rotation=90)},
        "backShin": {"shin": region("shin", x=17, rotation=90)},
        "frontThigh": {"thigh": region("thigh", x=18, rotation=90)},
        "frontShin": {"shin": region("shin", x=17, rotation=90)},
        "torso": {"torso": region("torso", y=-30)},
        "head": {"head": region("head", y=-18)},
        "hat": {"hat": region("cap", x=2, y=-38)},
        "cape": {"cape": {"type": "mesh", "frame": "cape", "vertices": vertices, "uvs": uvs, "triangles": triangles}},
        "hand": {"hand": {"type": "point", "x": 28}},
    }
    bob = [{"time": time, "x": 0, "y": y, "curve": "sineInOut"} for time, y in ((0, 0), (0.2, -4), (0.4, 0), (0.6, -4), (0.8, 0))]
    walk = {
        "duration": 0.8,
        "bones": {
            "frontThigh": {"rotate": rotations([(0, 25), (0.4, -25), (0.8, 25)])},
            "backThigh": {"rotate": rotations([(0, -25), (0.4, 25), (0.8, -25)])},
            "frontShin": {"rotate": rotations([(0, 0), (0.2, -35), (0.4, 0), (0.8, 0)])},
            "backShin": {"rotate": rotations([(0, 0), (0.4, 0), (0.6, -35), (0.8, 0)])},
            "frontUpper": {"rotate": rotations([(0, -20), (0.4, 20), (0.8, -20)])},
            "backUpper": {"rotate": rotations([(0, 20), (0.4, -20), (0.8, 20)])},
            "hip": {"translate": bob},
        },
        "events": [{"time": 0.0, "name": "step", "text": "front"}, {"time": 0.4, "name": "step", "text": "back"}],
    }
    idle = {
        "duration": 1.6,
        "bones": {
            "torso": {"translate": [{"time": time, "x": 0, "y": y, "curve": "sineInOut"} for time, y in ((0, 0), (0.8, 2), (1.6, 0))]},
            "frontUpper": {"rotate": rotations([(0, -5), (0.8, 5), (1.6, -5)])},
            "backUpper": {"rotate": rotations([(0, 5), (0.8, -5), (1.6, 5)])},
        },
    }
    wave = {"duration": 0.8, "bones": {"backUpper": {"rotate": rotations([(0, -160), (0.8, -160)])}, "backFore": {"rotate": rotations([(0, -30), (0.4, 30), (0.8, -30)])}}}
    cape = {"duration": 1.2, "deform": {"cape": {"cape": [{"time": 0, "offsets": flap(0), "curve": "sineInOut"}, {"time": 0.6, "offsets": flap(18), "curve": "sineInOut"}, {"time": 1.2, "offsets": flap(0)}]}}}
    return {
        "atlas": "puppet.json",
        "bones": bones,
        "slots": slots,
        "skins": {"default": default, "royal": {"hat": {"hat": region("crown", y=-40)}}},
        "ik": [{"name": "reach", "bones": ["frontUpper", "frontFore"], "target": "aim", "bendPositive": False, "mix": 0}],
        "animations": {"walk": walk, "idle": idle, "wave": wave, "flap": cape},
    }


def main() -> None:
    CONTENT.mkdir(parents=True, exist_ok=True)
    (CONTENT / "fighter.json").write_text(json.dumps(atlas(), indent=4) + "\n")
    (CONTENT / "puppet.json").write_text(json.dumps(puppet_atlas(), indent=4) + "\n")
    (CONTENT / "puppet.skeleton").write_text(json.dumps(puppet_skeleton(), indent=4) + "\n")


if __name__ == "__main__":
    main()
