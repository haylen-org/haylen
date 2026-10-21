"""Builds the cutout puppet of the skeleton test from its painting: the atlas of its parts and the skeleton that rigs them.

The painting is `puppet-parts.png`, the nine parts of the puppet in a grid of five columns and two rows on flat magenta, painted with the "codex" command line in the style of the README of the test project: the head, the torso, the upper arm, the forearm, the thigh, the shin with its boot, the cap, the crown and the cape. Every limb is a capsule with round ends, so the parts overlap at round joints and the cuts never show. Run it with `python3 samples/tests/tools/animation/pack-puppet.py <folder of the painting>`.

The tool cuts the parts, scales the head up and the limbs down from the size of the torso so the puppet has the proportions of the characters of the repository, and packs them at `DENSITY` pixels for each unit of the skeleton into `content/animation/puppet.png` with their frames in `puppet.json`. It measures the joints on the parts, the round end of each limb, the neck, the shoulders and the hips of the torso, and writes `puppet.skeleton`: a puppet that stands on its root at the feet, a little over 200 units tall, with its limbs pointing down in a relaxed pose, the forearms under the sleeves and the thighs over the shins, a cape mesh behind the torso, a cap on the head that the skin `royal` swaps for a crown, the inverse kinematics of the front arm and the animations `idle`, `walk`, `wave` and `flap`, whose walk reports the steps of both feet.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import paintings  # noqa: E402

CONTENT = Path(__file__).resolve().parents[2] / "content" / "animation"
NAMES = ("head", "torso", "upperArm", "forearm", "thigh", "shin", "cap", "crown", "cape")
# How much larger or smaller than the torso each part is drawn.
SIZES = {"head": 1.3, "upperArm": 0.85, "forearm": 0.85, "thigh": 0.85, "shin": 0.85, "cap": 1.3, "crown": 1.3}
HEIGHT = 205
DENSITY = 2.5
ATLAS_WIDTH = 1024
GAP = 4
# Places on the parts at the size of the painting: where the neck of the torso meets the head, the shoulders, the hips and the clasp of the cape, the point of each hat that sits on its spot of the head, and how deep the elbow sits inside the sleeve.
NECK = (96, 14)
SHOULDERS = {"front": (156, 62), "back": (26, 62)}
HIPS = {"front": (121, 182), "back": (77, 182)}
CLASP = (68, 34)
HEAD_NECK = (128, 206)
HATS = {"cap": ((100, 118), (122, 44)), "crown": ((69, 96), (122, 22))}
ELBOW_DEPTH = 34
# The relaxed pose in degrees, clockwise from the right, where 90 points straight down.
POSE = {"frontUpper": 82, "frontFore": -14, "backUpper": 98, "backFore": -10, "frontThigh": 86, "frontShin": 4, "backThigh": 95, "backShin": -5}
SHADE = "#FFBDBDCC"


def measure(part) -> dict:
    """Returns the joints of a limb: the middle of its round top end and of its round bottom end, from the columns its top and bottom rows cover."""
    width, height = part.width, part.height

    def middle(y: int) -> tuple[float, float]:
        xs = [x for x in range(width) if part.alpha(x, y) > paintings.VISIBLE]
        return (min(xs) + max(xs)) / 2, max(xs) - min(xs)

    top_x, top_width = middle(round(height * 0.3))
    bottom_x, bottom_width = middle(round(height * 0.7))
    return {"top": (top_x, top_width / 2), "bottom": (bottom_x, height - bottom_width / 2)}


def pack(images: dict) -> tuple[paintings.Image, dict]:
    """Packs the parts in shelves, tallest first, and returns the atlas with the place of each part."""
    order = sorted(images, key=lambda name: -images[name].height)
    places, x, y, shelf = {}, GAP, GAP, 0
    for name in order:
        image = images[name]
        if x + image.width + GAP > ATLAS_WIDTH:
            x, y, shelf = GAP, y + shelf + GAP, 0
        places[name] = (x, y, image.width, image.height)
        x, shelf = x + image.width + GAP, max(shelf, image.height)
    height = y + shelf + GAP
    atlas = paintings.Image.blank(ATLAS_WIDTH, height)
    for name, (left, top, _, _) in places.items():
        atlas.paste(images[name], left, top)
    return atlas, places


def main() -> None:
    if len(sys.argv) != 2:
        sys.exit('Name the folder of the painting, such as "python3 pack-puppet.py build/art/anim".')
    painted = paintings.read(Path(sys.argv[1]) / "puppet-parts.png")
    boxes = dict(zip(NAMES, paintings.parts(paintings.key(painted))))

    # The legs, the torso and the head set the height of the puppet, so one unit of the skeleton is this share of a pixel of the painting.
    raw = {name: paintings.key(painted.crop(box[0], box[1], box[2] - box[0], box[3] - box[1])) for name, box in boxes.items()}
    thigh, shin, upper, fore = (measure(raw[name]) for name in ("thigh", "shin", "upperArm", "forearm"))
    leg = SIZES["thigh"] * (thigh["bottom"][1] - thigh["top"][1]) + SIZES["shin"] * (raw["shin"].height - shin["top"][1])
    stature = leg + HIPS["front"][1] - NECK[1] + SIZES["head"] * HEAD_NECK[1]
    unit = HEIGHT / stature

    images = {name: paintings.trimmed(paintings.cut(painted, box, unit * DENSITY * SIZES.get(name, 1.0), margin=8), margin=1) for name, box in boxes.items()}
    atlas, places = pack(images)
    paintings.write(atlas, CONTENT / "puppet.png")
    frames = {name: {"frame": {"x": x, "y": y, "w": w, "h": h}} for name, (x, y, w, h) in places.items()}
    (CONTENT / "puppet.json").write_text(json.dumps({"frames": frames, "meta": {"image": "puppet.png", "size": {"w": atlas.width, "h": atlas.height}}}, indent=4) + "\n")
    (CONTENT / "puppet.skeleton").write_text(json.dumps(skeleton(raw, images, unit, thigh, shin, upper, fore, leg), indent=4) + "\n")


def skeleton(raw: dict, images: dict, unit: float, thigh: dict, shin: dict, upper: dict, fore: dict, leg: float) -> dict:
    def units(name: str, value: float) -> float:
        return round(value * unit * SIZES.get(name, 1.0), 2)

    def size(name: str) -> dict:
        return {"width": round(images[name].width / DENSITY, 2), "height": round(images[name].height / DENSITY, 2)}

    def limb(name: str, joint: tuple[float, float]) -> dict:
        """A region of a vertical limb on a bone that points down along it, turned back upright, with its top joint on the bone."""
        part = raw[name]
        return {"frame": name, "x": units(name, part.height / 2 - joint[1]), "y": units(name, joint[0] - part.width / 2), "rotation": -90, **size(name)}

    def placed(name: str, anchor: tuple[float, float], at: tuple[float, float] = (0, 0)) -> dict:
        """A region of an upright part whose point `anchor` sits on `at` of its bone."""
        part = raw[name]
        return {"frame": name, "x": round(at[0] + units(name, part.width / 2 - anchor[0]), 2), "y": round(at[1] + units(name, part.height / 2 - anchor[1]), 2), **size(name)}

    hip = ((HIPS["front"][0] + HIPS["back"][0]) / 2, HIPS["front"][1])
    torso_point = lambda point: (units("torso", point[0] - hip[0]), units("torso", point[1] - hip[1]))  # noqa: E731
    thigh_length = units("thigh", thigh["bottom"][1] - thigh["top"][1])
    upper_length = units("upperArm", raw["upperArm"].height - ELBOW_DEPTH - upper["top"][1])
    fist = units("forearm", fore["bottom"][1] - fore["top"][1])
    neck = torso_point(NECK)
    bones = [
        {"name": "root"},
        {"name": "hip", "parent": "root", "y": -round(leg * unit, 2)},
        {"name": "torso", "parent": "hip", "length": round(-neck[1], 2)},
        {"name": "head", "parent": "torso", "x": neck[0], "y": neck[1]},
    ]
    for side in ("back", "front"):
        x, y = torso_point(SHOULDERS[side])
        bones.append({"name": f"{side}Upper", "parent": "torso", "x": x, "y": y, "rotation": POSE[f"{side}Upper"], "length": upper_length})
        bones.append({"name": f"{side}Fore", "parent": f"{side}Upper", "x": upper_length, "rotation": POSE[f"{side}Fore"], "length": fist})
    for side in ("back", "front"):
        x, y = torso_point(HIPS[side])
        bones.append({"name": f"{side}Thigh", "parent": "hip", "x": x, "rotation": POSE[f"{side}Thigh"], "length": thigh_length})
        bones.append({"name": f"{side}Shin", "parent": f"{side}Thigh", "x": thigh_length, "rotation": POSE[f"{side}Shin"], "length": units("shin", raw["shin"].height - shin["top"][1])})
    bones.append({"name": "aim", "parent": "root", "x": 110, "y": -150})

    slots = [
        {"name": "backFore", "bone": "backFore", "attachment": "forearm", "color": SHADE},
        {"name": "backUpper", "bone": "backUpper", "attachment": "upperArm", "color": SHADE},
        {"name": "backShin", "bone": "backShin", "attachment": "shin", "color": SHADE},
        {"name": "backThigh", "bone": "backThigh", "attachment": "thigh", "color": SHADE},
        {"name": "cape", "bone": "torso", "attachment": "cape"},
        {"name": "frontShin", "bone": "frontShin", "attachment": "shin"},
        {"name": "frontThigh", "bone": "frontThigh", "attachment": "thigh"},
        {"name": "torso", "bone": "torso", "attachment": "torso"},
        {"name": "head", "bone": "head", "attachment": "head"},
        {"name": "hat", "bone": "head", "attachment": "hat"},
        {"name": "frontFore", "bone": "frontFore", "attachment": "forearm"},
        {"name": "frontUpper", "bone": "frontUpper", "attachment": "upperArm"},
        {"name": "hand", "bone": "frontFore", "attachment": "hand"},
    ]

    # The cape hangs from its clasp behind the neck as a mesh of 2 by 3 cells, whose lower rows the flap pushes back.
    columns, rows = 2, 3
    cape = raw["cape"]
    clasp = torso_point(CLASP)
    left, top = clasp[0] - units("cape", cape.width / 2), clasp[1] - units("cape", 18)
    width, height = units("cape", cape.width), units("cape", cape.height)
    vertices, uvs, triangles = [], [], []
    for row in range(rows + 1):
        for column in range(columns + 1):
            vertices += [round(left + width * column / columns, 2), round(top + height * row / rows, 2)]
            uvs += [column / columns, row / rows]
    for row in range(rows):
        for column in range(columns):
            first = row * (columns + 1) + column
            below = first + columns + 1
            triangles += [first, first + 1, below + 1, first, below + 1, below]

    def flap(amount: float) -> list[float]:
        return [value for row in range(rows + 1) for column in range(columns + 1) for value in (round(-amount * (row / rows) ** 2, 2), 0)]

    head_neck = (HEAD_NECK[0], HEAD_NECK[1])
    hats = {name: placed(name, anchor, (units("head", spot[0] - head_neck[0]), units("head", spot[1] - head_neck[1]))) for name, (anchor, spot) in HATS.items()}
    default = {
        "backUpper": {"upperArm": limb("upperArm", upper["top"])},
        "frontUpper": {"upperArm": limb("upperArm", upper["top"])},
        "backFore": {"forearm": limb("forearm", fore["top"])},
        "frontFore": {"forearm": limb("forearm", fore["top"])},
        "backThigh": {"thigh": limb("thigh", thigh["top"])},
        "frontThigh": {"thigh": limb("thigh", thigh["top"])},
        "backShin": {"shin": limb("shin", shin["top"])},
        "frontShin": {"shin": limb("shin", shin["top"])},
        "torso": {"torso": placed("torso", hip)},
        "head": {"head": placed("head", head_neck)},
        "hat": {"hat": hats["cap"]},
        "cape": {"cape": {"type": "mesh", "frame": "cape", "vertices": vertices, "uvs": uvs, "triangles": triangles}},
        "hand": {"hand": {"type": "point", "x": fist}},
    }

    def rotations(keys: list[tuple[float, float]]) -> list[dict]:
        return [{"time": time, "value": value, "curve": "sineInOut"} for time, value in keys]

    bob = [{"time": time, "x": 0, "y": y, "curve": "sineInOut"} for time, y in ((0, 0), (0.2, -4), (0.4, 0), (0.6, -4), (0.8, 0))]
    walk = {
        "duration": 0.8,
        "bones": {
            "frontThigh": {"rotate": rotations([(0, 24), (0.4, -24), (0.8, 24)])},
            "backThigh": {"rotate": rotations([(0, -24), (0.4, 24), (0.8, -24)])},
            "frontShin": {"rotate": rotations([(0, 0), (0.2, -36), (0.4, 0), (0.8, 0)])},
            "backShin": {"rotate": rotations([(0, 0), (0.4, 0), (0.6, -36), (0.8, 0)])},
            "frontUpper": {"rotate": rotations([(0, -22), (0.4, 22), (0.8, -22)])},
            "backUpper": {"rotate": rotations([(0, 22), (0.4, -22), (0.8, 22)])},
            "frontFore": {"rotate": rotations([(0, -18), (0.4, 0), (0.8, -18)])},
            "backFore": {"rotate": rotations([(0, 0), (0.4, -18), (0.8, 0)])},
            "head": {"rotate": rotations([(0, 0), (0.2, 2), (0.4, 0), (0.6, 2), (0.8, 0)])},
            "hip": {"translate": bob},
        },
        "events": [{"time": 0.0, "name": "step", "text": "front"}, {"time": 0.4, "name": "step", "text": "back"}],
    }
    idle = {
        "duration": 2.4,
        "bones": {
            "torso": {"translate": [{"time": time, "x": 0, "y": y, "curve": "sineInOut"} for time, y in ((0, 0), (1.2, 1.5), (2.4, 0))], "rotate": rotations([(0, 0), (1.2, -1.5), (2.4, 0)])},
            "head": {"rotate": rotations([(0, 0), (1.2, 3), (2.4, 0)])},
            "frontUpper": {"rotate": rotations([(0, -3), (1.2, 4), (2.4, -3)])},
            "backUpper": {"rotate": rotations([(0, 3), (1.2, -4), (2.4, 3)])},
            "frontFore": {"rotate": rotations([(0, 0), (1.2, -5), (2.4, 0)])},
        },
    }
    wave = {"duration": 0.8, "bones": {"backUpper": {"rotate": rotations([(0, 112), (0.8, 112)])}, "backFore": {"rotate": rotations([(0, 15), (0.4, 60), (0.8, 15)])}}}
    flapping = {"duration": 1.2, "deform": {"cape": {"cape": [{"time": 0, "offsets": flap(0), "curve": "sineInOut"}, {"time": 0.6, "offsets": flap(16), "curve": "sineInOut"}, {"time": 1.2, "offsets": flap(0)}]}}}
    return {
        "atlas": "puppet.json",
        "bones": bones,
        "slots": slots,
        "skins": {"default": default, "royal": {"hat": {"hat": hats["crown"]}}},
        "ik": [{"name": "reach", "bones": ["frontUpper", "frontFore"], "target": "aim", "bendPositive": False, "mix": 0}],
        "animations": {"walk": walk, "idle": idle, "wave": wave, "flap": flapping},
    }


if __name__ == "__main__":
    main()
