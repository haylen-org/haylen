"""Builds the parts of the tank of the Nodes test and its prefab from their painting.

The painting is `tank-parts.png`, the hull with its track, the turret, the gun barrel, a road wheel and the pennant of one toy tank on flat magenta, painted with the "codex" command line in the style of the README of the test project. The inside of the track of the hull is an empty band where the road wheels turn. Run it with `python3 samples/tests/tools/nodes/pack-tank.py <folder of the painting>`.

The tool cuts the parts, scales them so the hull is `HULL` units long at `DENSITY` pixels a unit, draws the pill of the name tag, and packs them into `content/nodes/parts.png`. It finds the band of the track by its color and writes `content/nodes/tank.prefab`: the hull with five road wheels spread along the band, the turret on the deck with the barrel and the pennant behind it, and above them the tag, a pill with the name of the tank, which stays readable when the tank faces left.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import paintings  # noqa: E402

CONTENT = Path(__file__).resolve().parents[2] / "content" / "nodes"
HULL = 132
DENSITY = 2.5
WHEELS = 5
GAP = 4
# The color of the band inside the track, and how much of its height a road wheel fills.
BAND = (66, 80, 83)
WHEEL_FILL = 0.92
# Places on the painting: the middle of the bottom of the turret on the deck of the hull, the socket of the barrel on the turret, the collar of the barrel, the foot of the pennant pole on the turret and the foot of the pole on the pennant.
SEAT = (405, 412)
SOCKET = (1338, 316)
COLLAR = (984, 505)
MAST = (1084, 250)
POLE = (1310, 834)
# The pill of the tag in units: its size, its border and the gap between it and the top of the pennant.
PILL = (100, 34)
PILL_BORDER = 2.5
TAG_GAP = 10
PILL_FILL = (47, 51, 82, 235)
PILL_RIM = (21, 23, 42, 255)
LABEL = {"size": 22, "color": "#FFF2F3FF", "anchor": [0.5, 0.5], "layer": 1}


def identify(boxes: list[tuple[int, int, int, int]]) -> dict:
    """Names the parts by their shapes: the largest is the hull, the longest the barrel, the tallest the pennant, and of the other two the smaller is the wheel."""
    area = lambda box: (box[2] - box[0]) * (box[3] - box[1])  # noqa: E731
    rest = sorted(boxes, key=area, reverse=True)
    names = {"hull": rest.pop(0)}
    names["barrel"] = max(rest, key=lambda box: (box[2] - box[0]) / (box[3] - box[1]))
    rest.remove(names["barrel"])
    names["flag"] = max(rest, key=lambda box: (box[3] - box[1]) / (box[2] - box[0]))
    rest.remove(names["flag"])
    names["wheel"], names["turret"] = sorted(rest, key=area)
    return names


def band(painted, hull: tuple[int, int, int, int]) -> tuple[int, int, int, int]:
    """Returns the box of the band inside the track, the pixels of the hull of its color."""
    xs, ys = [], []
    for y in range(hull[1], hull[3]):
        for x in range(hull[0], hull[2], 2):
            color = painted.pixels[(y * painted.width + x) * 4 : (y * painted.width + x) * 4 + 3]
            if all(abs(value - target) <= 6 for value, target in zip(color, BAND)):
                xs.append(x)
                ys.append(y)
    return min(xs), min(ys), max(xs), max(ys)


def pill(width: int, height: int, border: float) -> paintings.Image:
    """Draws a pill with round ends, its rim inside its edge, sampled four times on each axis so its edges are smooth."""
    image = paintings.Image.blank(width, height)
    radius = height / 2
    for y in range(height):
        for x in range(width):
            fill = rim = 0
            for sy in range(4):
                for sx in range(4):
                    px, py = x + (sx + 0.5) / 4, y + (sy + 0.5) / 4
                    cx = min(max(px, radius), width - radius)
                    distance = ((px - cx) ** 2 + (py - radius) ** 2) ** 0.5
                    if distance <= radius:
                        if distance >= radius - border:
                            rim += 1
                        else:
                            fill += 1
            cover = (fill + rim) / 16
            if cover:
                color = [(PILL_FILL[channel] * fill + PILL_RIM[channel] * rim) / (fill + rim) for channel in range(3)]
                alpha = (PILL_FILL[3] * fill + PILL_RIM[3] * rim) / (fill + rim) * cover
                image.pixels[(y * width + x) * 4 : (y * width + x) * 4 + 4] = bytes((*(round(value) for value in color), round(alpha)))
    return image


def main() -> None:
    if len(sys.argv) != 2:
        sys.exit('Name the folder of the painting, such as "python3 pack-tank.py build/art/anim".')
    painted = paintings.read(Path(sys.argv[1]) / "tank-parts.png")
    boxes = identify(paintings.parts(paintings.key(painted)))
    hull = boxes["hull"]
    unit = HULL / (hull[2] - hull[0])
    track = band(painted, hull)
    wheel_size = (track[3] - track[1]) * WHEEL_FILL
    wheel_box = boxes["wheel"]
    scales = {name: unit * DENSITY for name in boxes}
    scales["wheel"] = wheel_size / (wheel_box[2] - wheel_box[0]) * unit * DENSITY
    images = {name: paintings.trimmed(paintings.cut(painted, box, scales[name], margin=8), margin=1) for name, box in boxes.items()}
    images["pill"] = pill(round(PILL[0] * DENSITY), round(PILL[1] * DENSITY), PILL_BORDER * DENSITY)

    # The parts sit in one row, tallest first, in an atlas as wide as they need.
    order = ["hull", "turret", "flag", "barrel", "wheel", "pill"]
    places, x = {}, GAP
    for name in order:
        places[name] = (x, GAP, images[name].width, images[name].height)
        x += images[name].width + GAP
    atlas = paintings.Image.blank(x, max(image.height for image in images.values()) + 2 * GAP)
    for name, (left, top, _, _) in places.items():
        atlas.paste(images[name], left, top)
    paintings.write(atlas, CONTENT / "parts.png")

    def sprite(name: str, **extra) -> dict:
        left, top, width, height = places[name]
        return {"texture": "nodes/parts.png", "filter": "linear", "source": [left, top, width, height], "width": round(width / DENSITY, 2), "height": round(height / DENSITY, 2), **extra}

    def middle(box: tuple[int, int, int, int]) -> tuple[float, float]:
        return (box[0] + box[2]) / 2, (box[1] + box[3]) / 2

    def offset(point: tuple[float, float], origin: tuple[float, float]) -> tuple[float, float]:
        return round((point[0] - origin[0]) * unit, 2), round((point[1] - origin[1]) * unit, 2)

    hull_middle, turret_box = middle(hull), boxes["turret"]
    turret_middle = middle(turret_box)
    turret_x, turret_y = offset((SEAT[0], SEAT[1] - (turret_box[3] - turret_middle[1])), hull_middle)
    barrel_box, flag_box = boxes["barrel"], boxes["flag"]
    barrel_x, barrel_y = offset(SOCKET, turret_middle)
    flag_x, flag_y = offset(MAST, turret_middle)
    flag_top = turret_y + flag_y - (flag_box[3] - flag_box[1]) * unit
    wheel_y = round(((track[1] + track[3]) / 2 - hull_middle[1]) * unit, 2)
    first, last = track[0] + wheel_size / 2, track[2] - wheel_size / 2
    wheels = [{"name": f"wheel{index + 1}", "x": round((first + (last - first) * index / (WHEELS - 1) - hull_middle[0]) * unit, 2), "y": wheel_y, "sprite": sprite("wheel")} for index in range(WHEELS)]
    prefab = {
        "name": "tank",
        "sprite": sprite("hull"),
        "children": [
            *wheels,
            {"name": "turret", "x": turret_x, "y": turret_y, "sprite": sprite("turret"), "children": [
                {"name": "barrel", "x": barrel_x, "y": barrel_y, "layer": -1, "sprite": sprite("barrel", pivotX=round((COLLAR[0] - barrel_box[0]) / (barrel_box[2] - barrel_box[0]), 3))},
                {"name": "flag", "x": flag_x, "y": flag_y, "layer": -1, "sprite": sprite("flag", pivotX=round((POLE[0] - flag_box[0]) / (flag_box[2] - flag_box[0]), 3), pivotY=1)},
            ]},
            {"name": "tag", "y": round(flag_top - TAG_GAP - PILL[1] / 2, 2), "layer": 2, "children": [
                {"name": "pill", "frame": {"texture": "nodes/parts.png", "filter": "linear", "source": list(places["pill"]), "borders": [PILL[1] * DENSITY / 2] * 4, "width": PILL[0], "height": PILL[1], "borderScale": round(1 / DENSITY, 3)}},
                {"name": "label", "text": {"text": "Tank", **LABEL}},
            ]},
        ],
    }
    (CONTENT / "tank.prefab").write_text(json.dumps(prefab, indent=4) + "\n")


if __name__ == "__main__":
    main()
