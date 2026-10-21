"""Draws the pixel art of the pixel snap and pixel canvas tests of the Camera category.

The tests show how pixel art behaves when a view moves by parts of a pixel, so their art is real pixel art, one color per art pixel from a small palette, without smoothing: an island of grass with dithered patches and tufts, two ponds with sandy shores and a line of foam, a dirt road with darker edges and pebbles, trees, bushes, rocks and flowers, and a knight of 16 pixels in two frames of a walk. Run it with `python3 samples/tests/tools/camera/draw-pixel-art.py`, which writes `content/camera/pixel-island.png` and `content/camera/knight.png`. The same code always draws the same pictures.
"""

from __future__ import annotations

import math
import sys
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[4] / "tools"))

from png_image import Image, write  # noqa: E402

CONTENT = Path(__file__).resolve().parents[2] / "content" / "camera"
WIDTH, HEIGHT = 640, 320

PALETTE = {
    "o": (30, 27, 51),
    "g": (52, 122, 60), "G": (78, 156, 58), "h": (110, 188, 70), "H": (156, 216, 96),
    "d": (120, 78, 48), "D": (176, 124, 72), "e": (206, 158, 98), "p": (148, 104, 64),
    "s": (214, 182, 112), "S": (242, 216, 142),
    "w": (36, 100, 152), "W": (47, 148, 196), "l": (112, 206, 226), "f": (236, 250, 250),
    "t": (82, 52, 32), "T": (118, 76, 44),
    "r": (92, 86, 102), "R": (142, 134, 150), "q": (202, 198, 208),
    "y": (255, 214, 74), "k": (255, 140, 178), "v": (248, 246, 236),
    "m": (200, 204, 220), "M": (140, 146, 170), "b": (58, 106, 224), "B": (36, 66, 160), "c": (226, 66, 66), "n": (238, 190, 150),
}

TREE = (
    "....oooooo....",
    "..ooHHhhhhoo..",
    ".oHHhhhhhhGGo.",
    "oHhhhhhhhGGGGo",
    "oHhhhhhhGGGGgo",
    "ohhhhhhGGGGggo",
    "ohhhhhGGGGgggo",
    ".ohhGGGGGgggo.",
    ".oGGGGGggggo..",
    "..ooGgggggoo..",
    "....oTTtoo....",
    "....oTTto.....",
    "....oTTto.....",
    "...ooTttoo....",
)

BUSH = (
    "..oooooo..",
    ".oHhhhhGo.",
    "oHhhhhGGGo",
    "ohhhGGGggo",
    ".oGGGggggo",
    "..oooooo..",
)

ROCK = (
    "..oooo..",
    ".oqqRRo.",
    "oqRRRRro",
    "oRRRRrro",
    ".oooooo.",
)

# Two frames of a knight of 16 pixels facing right: a helmet with a plume, a blue tabard with a gold buckle, and legs that step.
KNIGHT = (
    (
        "................",
        "......cc........",
        ".....occo.......",
        "....ommmmo......",
        "...ommmmmMo.....",
        "...ommooooo.....",
        "...ommmmmMo.....",
        "....oMMMMo......",
        "...obbbbbbo.....",
        "..obbbyybbBo....",
        "..onbbbbbbBno...",
        "...obbbbbbBo....",
        "....oBBoBBo.....",
        "....oMo.oMo.....",
        "....oMo..oMo....",
        "....ooo..ooo....",
    ),
    (
        "................",
        "................",
        "......cc........",
        ".....occo.......",
        "....ommmmo......",
        "...ommmmmMo.....",
        "...ommooooo.....",
        "...ommmmmMo.....",
        "....oMMMMo......",
        "...obbbbbbo.....",
        "..obbbyybbBo....",
        "..onbbbbbbBno...",
        "...obbbbbbBo....",
        "....oBBBBBo.....",
        ".....oMoMo......",
        ".....ooooo......",
    ),
)


class Canvas:
    """An image of art pixels, each one color of the palette or transparent."""

    def __init__(self, width: int, height: int) -> None:
        self.width, self.height = width, height
        self.keys = [["."] * width for _ in range(height)]

    def set(self, x: int, y: int, key: str) -> None:
        if 0 <= x < self.width and 0 <= y < self.height and key != ".":
            self.keys[y][x] = key

    def get(self, x: int, y: int) -> str:
        return self.keys[y][x] if 0 <= x < self.width and 0 <= y < self.height else "."

    def stamp(self, rows: tuple[str, ...], left: int, top: int) -> None:
        for y, row in enumerate(rows):
            for x, key in enumerate(row):
                self.set(left + x, top + y, key)

    def image(self) -> Image:
        result = Image.blank(self.width, self.height)
        for y in range(self.height):
            for x in range(self.width):
                key = self.keys[y][x]
                if key != ".":
                    result.pixels[(y * self.width + x) * 4 : (y * self.width + x + 1) * 4] = bytes((*PALETTE[key], 255))
        return result


def hashed(x: int, y: int, seed: int) -> float:
    """Returns a number from 0 to 1 that is always the same for a lattice point and a seed."""
    value = (x * 374761393 + y * 668265263 + seed * 2147483647) & 0xFFFFFFFF
    value = ((value ^ (value >> 13)) * 1274126177) & 0xFFFFFFFF
    return ((value ^ (value >> 16)) & 0xFFFF) / 0xFFFF


def noise(x: float, y: float, scale: float, seed: int) -> float:
    """Returns smooth value noise from 0 to 1 with features about `scale` pixels apart."""
    fx, fy = x / scale, y / scale
    ix, iy = math.floor(fx), math.floor(fy)
    tx, ty = fx - ix, fy - iy
    tx, ty = tx * tx * (3 - 2 * tx), ty * ty * (3 - 2 * ty)
    top = hashed(ix, iy, seed) * (1 - tx) + hashed(ix + 1, iy, seed) * tx
    bottom = hashed(ix, iy + 1, seed) * (1 - tx) + hashed(ix + 1, iy + 1, seed) * tx
    return top * (1 - ty) + bottom * ty


def segment_distance(x: float, y: float, a: tuple[float, float], b: tuple[float, float]) -> float:
    ax, ay = a
    bx, by = b
    dx, dy = bx - ax, by - ay
    t = max(0.0, min(1.0, ((x - ax) * dx + (y - ay) * dy) / (dx * dx + dy * dy)))
    return math.hypot(x - ax - dx * t, y - ay - dy * t)


PONDS = ((150, 90, 70, 44), (500, 230, 84, 50))
ROAD = ((-10, 200), (120, 210), (250, 160), (380, 150), (470, 110), (560, 100), (650, 120))


def ground(x: int, y: int) -> str:
    """Returns the kind of ground of an art pixel: deep or shallow water, shore, road or grass."""
    wobble = noise(x, y, 14, 3) * 0.5 - 0.25
    for cx, cy, rx, ry in PONDS:
        inside = ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 + wobble
        if inside < 0.42:
            return "deep"
        if inside < 1:
            return "water"
        if inside < 1.22:
            return "shore"
    if min(segment_distance(x, y, ROAD[index], ROAD[index + 1]) for index in range(len(ROAD) - 1)) < 7 + wobble * 6:
        return "road"
    return "grass"


def draw_island() -> Canvas:
    canvas = Canvas(WIDTH, HEIGHT)
    kinds = [[ground(x, y) for x in range(WIDTH)] for y in range(HEIGHT)]
    kind = lambda x, y: kinds[min(HEIGHT - 1, max(0, y))][min(WIDTH - 1, max(0, x))]  # noqa: E731
    for y in range(HEIGHT):
        for x in range(WIDTH):
            here = kind(x, y)
            patch = noise(x, y, 26, 7)
            grain = hashed(x, y, 11)
            if here == "grass":
                # Light and dark patches meet in a checkered dither, and single pixels speckle the grass.
                key = "h" if patch > 0.58 or (patch > 0.52 and (x + y) % 2 == 0) else "G"
                if patch < 0.3 or (patch < 0.36 and (x + y) % 2 == 0):
                    key = "g"
                if grain > 0.985:
                    key = "H"
            elif here == "road":
                key = "e" if grain > 0.93 else ("p" if grain < 0.04 else "D")
                if "grass" in (kind(x - 1, y), kind(x + 1, y), kind(x, y - 1), kind(x, y + 1)):
                    key = "d"
            elif here == "shore":
                key = "S" if grain > 0.12 else "s"
            else:
                key = "w" if here == "deep" or (kind(x, y - 1) == "deep" and (x + y) % 2 == 0) else "W"
                if grain > 0.97:
                    key = "l"
                if "shore" in (kind(x - 1, y), kind(x + 1, y), kind(x, y - 1), kind(x, y + 1)):
                    key = "f"
            canvas.set(x, y, key)

    # Tufts of grass: a light blade over a dark one.
    for y in range(2, HEIGHT - 2, 5):
        for x in range(2, WIDTH - 2, 6):
            px, py = x + int(hashed(x, y, 21) * 4), y + int(hashed(x, y, 22) * 3)
            if kind(px, py) == "grass" and hashed(px, py, 23) < 0.35:
                canvas.set(px, py, "H")
                canvas.set(px - 1, py + 1, "g")
                canvas.set(px + 1, py + 1, "g")

    # Flowers in small clusters on the grass.
    for y in range(4, HEIGHT - 4, 9):
        for x in range(4, WIDTH - 4, 11):
            px, py = x + int(hashed(x, y, 31) * 6), y + int(hashed(x, y, 32) * 6)
            if kind(px, py) == "grass" and hashed(px, py, 33) < 0.22:
                color = ("y", "k", "v")[int(hashed(px, py, 34) * 3)]
                canvas.set(px, py, color)
                canvas.set(px + 2, py + 1, color)
                canvas.set(px + 1, py + 2, "y")

    # Props stand on grass away from the road and the ponds, from the top down so the lower ones cover the higher.
    props = []
    for y in range(6, HEIGHT - 6, 22):
        for x in range(4, WIDTH - 4, 26):
            px, py = x + int(hashed(x, y, 41) * 14), y + int(hashed(x, y, 42) * 12)
            roll = hashed(px, py, 43)
            shape = TREE if roll < 0.45 else (BUSH if roll < 0.7 else (ROCK if roll < 0.85 else None))
            if shape is None:
                continue
            width, height = len(shape[0]), len(shape)
            clear = all(kind(px + dx, py + dy) == "grass" for dx in range(-2, width + 2, 2) for dy in range(height - 4, height + 3))
            if clear:
                props.append((py + height, px, py, shape))
    for bottom, px, py, shape in sorted(props):
        # A shadow of dark grass under the foot of the prop.
        for dx in range(1, len(shape[0]) - 1):
            canvas.set(px + dx, bottom, "g")
        canvas.stamp(shape, px, py)
    return canvas


def main() -> None:
    write(draw_island().image(), CONTENT / "pixel-island.png")
    knight = Canvas(32, 16)
    for index, frame in enumerate(KNIGHT):
        knight.stamp(frame, index * 16, 0)
    write(knight.image(), CONTENT / "knight.png")


if __name__ == "__main__":
    main()
