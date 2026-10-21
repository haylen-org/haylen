"""The landscape of the maps of the Tiled tests: water, lava, sand, paths and grass held by the corners of the cells, which the corner sets of the transitions blend into each other, and the props that stand on the open ground.

A map paints shapes onto a `Ground`, lakes as blobs and roads as lines, and the ground gives back the tile layers of the corner sets and tells which cells are open, so trees, bushes and rocks never stand in the water or on a road.
"""

from __future__ import annotations

import math
import random

import tilesets as t
import transitions as tr
from tiled_json import MapBuilder, noise

TERRAIN = "../tilesets/terrain.tsj"
PROPS = "../tilesets/props.tsj"
TRANSITIONS = "../tilesets/transitions.tsj"
SCENERY = "../tilesets/scenery.tsj"
ROOFS = "../tilesets/roofs.tsj"
PROPS_GID = 33
TRANSITIONS_GID = 41
SCENERY_GID = TRANSITIONS_GID + tr.COLUMNS * (tr.GRASS + 1)
ROOFS_GID = SCENERY_GID + len(t.SCENERY)

# The rows of the corner sets of each kind of ground.
ROWS = {"water": tr.WATER, "sand": tr.SAND, "path": tr.PATH, "lava": tr.LAVA, "grass": tr.GRASS}


def gid(tile: int) -> int:
    """The global id of a tile of the terrain tileset, which every map lists first."""
    return tile + 1


class Ground:
    """The kinds of ground that the corners of the cells of a map hold, from 0 to the width and the height of the map."""

    def __init__(self, width: int, height: int, seed: int) -> None:
        self.width, self.height = width, height
        self.corners: dict[str, set[tuple[int, int]]] = {kind: set() for kind in ROWS}
        self.wobble = noise(width + 4, height + 4, 2.5, seed)

    def paint(self, kind: str, inside) -> Ground:
        for line in range(self.height + 1):
            for column in range(self.width + 1):
                if inside(column, line):
                    self.corners[kind].add((column, line))
        return self

    def blob(self, kind: str, x: float, y: float, rx: float, ry: float, wobble: float = 0.3) -> Ground:
        """Paints an ellipse centered on the corner `x`, `y`, whose edge the noise of the ground bends."""
        return self.paint(kind, lambda column, line: ((column - x) / rx) ** 2 + ((line - y) / ry) ** 2 < 1 + wobble * (self.wobble(column + 1, line + 1) - 0.5))

    def road(self, kind: str, points: list[tuple[float, float]], half: float) -> Ground:
        """Paints a road through the corners `points`, as wide as twice `half` corners, with a gentle wander."""

        def inside(column: int, line: int) -> bool:
            return min(distance(column, line, a, b) for a, b in zip(points, points[1:])) < half + 0.6 * (self.wobble(column + 1, line + 1) - 0.5)

        return self.paint(kind, inside)

    def holds(self, kind: str, column: int, line: int) -> bool:
        return (column, line) in self.corners[kind]

    def touches(self, column: int, row: int, kinds: tuple[str, ...] = ("water", "sand", "path", "lava")) -> bool:
        """Tells whether any corner of a cell holds one of `kinds`, so the cell is not open grass."""
        return any(self.holds(kind, column + dx, row + dy) for kind in kinds for dx in (0, 1) for dy in (0, 1))

    def cells(self, kind: str):
        """The global tile of each cell in the corner set of `kind`, 0 where no corner of the cell holds it."""
        row = ROWS[kind]

        def cell(column: int, line: int) -> int:
            tile = tr.corner(row, lambda x, y: int(self.holds(kind, x, y)), column, line)
            return 0 if tile is None else TRANSITIONS_GID + tile

        return cell

    def layer(self, builder: MapBuilder, name: str, kind: str, **extra) -> dict:
        """The tile layer of the corner set of `kind`."""
        return builder.tiles(name, self.cells(kind), **extra)

    def meadow(self, builder: MapBuilder, seed: int, **extra) -> dict:
        """Grass on every cell, with flowers on a few of the open ones."""
        rng = random.Random(seed)
        return builder.tiles("ground", lambda column, row: gid(t.FLOWERS if not self.touches(column, row) and rng.random() < 0.12 else t.GRASS), **extra)

    def open_cells(self, margin: int = 0) -> list[tuple[int, int]]:
        return [(column, row) for row in range(margin, self.height - margin) for column in range(margin, self.width - margin) if not self.touches(column, row)]


def distance(x: float, y: float, a: tuple[float, float], b: tuple[float, float]) -> float:
    """The distance from a point to the segment from `a` to `b`."""
    dx, dy = b[0] - a[0], b[1] - a[1]
    share = 0.0 if dx == dy == 0 else max(0.0, min(1.0, ((x - a[0]) * dx + (y - a[1]) * dy) / (dx * dx + dy * dy)))
    return math.hypot(x - a[0] - share * dx, y - a[1] - share * dy)


def scatter(cells: list[tuple[int, int]], count: int, spacing: float, seed: int, taken: list[tuple[float, float]] | None = None) -> list[tuple[int, int]]:
    """Picks up to `count` of `cells` at random, each at least `spacing` cells from the others and from `taken`."""
    rng = random.Random(seed)
    pool = list(cells)
    rng.shuffle(pool)
    picked: list[tuple[int, int]] = []
    others = list(taken or [])
    for cell in pool:
        if len(picked) == count:
            break
        if all(math.hypot(cell[0] - x, cell[1] - y) >= spacing for x, y in others):
            picked.append(cell)
            others.append(cell)
    return picked


class Props:
    """Tile objects that stand on the cells of a map: the trees and the lamp of the props tileset, the bushes, rocks and items of the terrain tileset and the props of the scenery tileset, at sizes in the units of the grid."""

    # The size of each kind of prop, and the tileset tile and alignment it draws with: props and scenery stand on the middle of their bottom edge, terrain tiles on their bottom-left corner.
    KINDS = {
        "tree": (PROPS_GID + t.TREE, 64, 128, "middle"),
        "pine": (PROPS_GID + t.PINE, 56, 112, "middle"),
        "lamp": (PROPS_GID + t.LAMP, 40, 80, "middle"),
        "bush": (gid(t.BUSH), 44, 44, "left"),
        "rock": (gid(t.ROCK), 44, 44, "left"),
        "crate": (gid(t.CRATE), 40, 40, "left"),
        "chest": (gid(t.CHEST), 44, 44, "left"),
        "sign": (gid(t.SIGN), 48, 48, "left"),
        "flag": (gid(t.FLAG), 44, 44, "left"),
        "coin": (gid(t.COIN), 36, 36, "left"),
        "slime": (gid(t.SLIME), 48, 48, "left"),
        "stump": (SCENERY_GID + t.STUMP, 64, 46, "middle"),
        "log": (SCENERY_GID + t.LOG, 72, 47, "middle"),
        "blossoms": (SCENERY_GID + t.BLOSSOMS, 54, 50, "middle"),
        "mushrooms": (SCENERY_GID + t.MUSHROOMS, 46, 40, "middle"),
        "reeds": (SCENERY_GID + t.REEDS, 44, 50, "middle"),
        "barrel": (SCENERY_GID + t.BARREL, 34, 42, "middle"),
        "signpost": (SCENERY_GID + t.SIGNPOST, 48, 60, "middle"),
        "well": (SCENERY_GID + t.WELL, 64, 80, "middle"),
        "campfire": (SCENERY_GID + t.CAMPFIRE, 60, 59, "middle"),
    }

    def __init__(self, builder: MapBuilder, tile: int = 32) -> None:
        self.builder, self.tile = builder, tile
        self.objects: list[dict] = []

    def at(self, kind: str, x: float, y: float, name: str | None = None, scale: float = 1.0, **fields) -> dict:
        """Places a prop with the middle of its bottom edge on the point `x`, `y` in units of the grid."""
        tile, width, height, alignment = Props.KINDS[kind]
        width, height = round(width * scale), round(height * scale)
        left = x if alignment == "middle" else x - width / 2
        placed = self.builder.object(name=name or kind, type=fields.pop("type", kind), gid=tile, x=round(left), y=round(y), width=width, height=height, **fields)
        self.objects.append(placed)
        return placed

    def on(self, kind: str, cell: tuple[int, int], rng: random.Random, name: str | None = None, scale: float = 1.0) -> dict:
        """Places a prop on a cell, a little off its middle so rows of props never look planted."""
        column, row = cell
        return self.at(kind, (column + 0.5 + rng.uniform(-0.2, 0.2)) * self.tile, (row + 0.85 + rng.uniform(-0.1, 0.1)) * self.tile, name, scale)
