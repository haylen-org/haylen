"""The transitions of the Tiled tests: water with its sandy shore, sand and dirt paths that blend into the grass, lava in a crust of scorched rock, and grass that ends in a ragged edge over the sand, as corner sets of sixteen tiles.

Every tile of a set is drawn from the painted textures of `tiles/terrain.png` and the four corners it covers: the corners that hold the terrain pull its edge toward them, and a noise that repeats every tile makes the edge wander naturally. The edge along a side of a tile depends only on the two corners of that side and the noise repeats across tiles, so any two tiles that share corners join without a seam. The water and the lava tiles play the four frames of their textures, and the sets are Tiled corner sets, so maps pick their tiles from the terrain at the corners of their cells. The sets keep their rows, so the lava follows the paths.
"""

from __future__ import annotations

import random
import sys
from pathlib import Path

import tilesets as t
from tiled_json import prop, tileset

sys.path.insert(0, str(Path(__file__).resolve().parents[4] / "tools"))
from png_image import Image, read, write  # noqa: E402

TILES = Path(__file__).resolve().parents[2] / "content" / "tiled" / "tiles"
SIZE = 32 * t.SCALE
COLUMNS = 16
# The sets and the row of their first tiles: the water and the lava take a row for each frame of their animation.
WATER, SAND, PATH, LAVA, GRASS = 0, 4, 5, 6, 10
FRAMES = 4


class Field:
    """The terrain of a tile: one where its corners hold it, zero where they do not, with a noise that repeats every tile."""

    def __init__(self, seed: int) -> None:
        rng = random.Random(seed)
        self.octaves = [(cells, weight, [[rng.uniform(-1, 1) for _ in range(cells)] for _ in range(cells)]) for cells, weight in ((4, 0.7), (8, 0.3))]

    def noise(self, x: float, y: float) -> float:
        total = 0.0
        for cells, weight, values in self.octaves:
            u, v = x * cells, y * cells
            column, row = int(u) % cells, int(v) % cells
            fx, fy = u - int(u), v - int(v)
            fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
            top = values[row][column] * (1 - fx) + values[row][(column + 1) % cells] * fx
            bottom = values[(row + 1) % cells][column] * (1 - fx) + values[(row + 1) % cells][(column + 1) % cells] * fx
            total += weight * (top * (1 - fy) + bottom * fy)
        return total

    def value(self, mask: int, x: float, y: float) -> float:
        """Returns how far a point lies inside the terrain, positive inside and negative outside, for the corners of `mask`: 1 the top left, 2 the top right, 4 the bottom right and 8 the bottom left."""
        corners = [(mask >> bit) & 1 for bit in range(4)]
        top = corners[0] * (1 - x) + corners[1] * x
        bottom = corners[3] * (1 - x) + corners[2] * x
        return top * (1 - y) + bottom * y - 0.5 + 0.12 * self.noise(x, y)


def texture(atlas: Image, tile: int) -> Image:
    return atlas.crop(t.MARGIN + tile % 8 * (SIZE + t.SPACING), t.MARGIN + tile // 8 * (SIZE + t.SPACING), SIZE, SIZE)


def ramp(value: float, edge: float, width: float = 0.012) -> float:
    """Rises from zero to one across the edge, so every border fades over about one and a half pixels."""
    share = min(1.0, max(0.0, (value - edge) / width + 0.5))
    return share * share * (3 - 2 * share)


def mix(first: tuple, second: tuple, share: float) -> tuple:
    return tuple(a * (1 - share) + b * share for a, b in zip(first, second))


def draw(mask: int, field: Field, layers) -> Image:
    """Draws the tile of a corner mask from its layers, each a function of the depth inside the terrain and the pixel that returns a color with its alpha, laid from the bottom up."""
    tile = Image.blank(SIZE, SIZE)
    for y in range(SIZE):
        for x in range(SIZE):
            depth = field.value(mask, (x + 0.5) / SIZE, (y + 0.5) / SIZE)
            color, alpha = (0.0, 0.0, 0.0), 0.0
            for layer in layers:
                over, cover = layer(depth, x, y)
                if cover <= 0:
                    continue
                total = cover + alpha * (1 - cover)
                color = tuple((o * cover + c * alpha * (1 - cover)) / total for o, c in zip(over, color))
                alpha = total
            tile.pixels[(y * SIZE + x) * 4 : (y * SIZE + x) * 4 + 4] = bytes((*(round(value) for value in color), round(alpha * 255)))
    return tile


def pixel(image: Image, x: int, y: int) -> tuple:
    return tuple(image.pixels[(y * SIZE + x) * 4 : (y * SIZE + x) * 4 + 3])


def water_layers(sand: Image, water: Image):
    def shore(depth: float, x: int, y: int):
        wet = ramp(depth, 0.03, 0.03)
        return mix(pixel(sand, x, y), (176, 140, 92), wet * 0.45), ramp(depth, -0.07)

    def lake(depth: float, x: int, y: int):
        return pixel(water, x, y), ramp(depth, 0.06)

    def foam(depth: float, x: int, y: int):
        return (240, 250, 255), 0.75 * ramp(depth, 0.06) * (1 - ramp(depth, 0.095))

    return shore, lake, foam


def lava_layers(crust: Image, lava: Image):
    """The lava sits in a crust of scorched rock, with a glowing seam where it meets the crust."""

    def rock(depth: float, x: int, y: int):
        return mix(pixel(crust, x, y), (58, 34, 28), 0.55 + 0.25 * ramp(depth, 0.02, 0.04)), ramp(depth, -0.06)

    def melt(depth: float, x: int, y: int):
        return pixel(lava, x, y), ramp(depth, 0.07)

    def seam(depth: float, x: int, y: int):
        return (255, 214, 120), 0.8 * ramp(depth, 0.07) * (1 - ramp(depth, 0.1))

    return rock, melt, seam


def solid_layers(surface: Image, rim: tuple):
    def ground(depth: float, x: int, y: int):
        return mix(pixel(surface, x, y), rim, 0.35 * (1 - ramp(depth, 0.03, 0.03))), ramp(depth, 0.0)

    return (ground,)


def extruded(tile: Image) -> Image:
    """Grows a tile by its gutter of its own edge pixels, so the linear filter never reads a neighbour."""
    grown = Image.blank(SIZE + 2 * t.MARGIN, SIZE + 2 * t.MARGIN)
    for y in range(grown.height):
        for x in range(grown.width):
            source = (min(SIZE - 1, max(0, y - t.MARGIN)) * SIZE + min(SIZE - 1, max(0, x - t.MARGIN))) * 4
            grown.pixels[(y * grown.width + x) * 4 : (y * grown.width + x) * 4 + 4] = tile.pixels[source : source + 4]
    return grown


def write_image() -> None:
    atlas = read(TILES / "terrain.png")
    sand, dirt = texture(atlas, t.SAND), texture(atlas, t.DIRT)
    rows = {}
    for frame in range(FRAMES):
        rows[WATER + frame] = (water_layers(sand, texture(atlas, t.WATER + frame)), 31)
    rows[SAND] = (solid_layers(sand, (196, 150, 84)), 47)
    rows[PATH] = (solid_layers(dirt, (92, 52, 28)), 59)
    for frame in range(FRAMES):
        rows[LAVA + frame] = (lava_layers(dirt, texture(atlas, t.LAVA + frame)), 71)
    rows[GRASS] = (solid_layers(texture(atlas, t.GRASS), (64, 128, 40)), 83)
    width, height = t.atlas_size(COLUMNS, len(rows), SIZE, SIZE)
    image = Image.blank(width, height)
    for row, (layers, seed) in rows.items():
        field = Field(seed)
        for mask in range(1, 16):
            image.paste(extruded(draw(mask, field, layers)), mask * (SIZE + t.SPACING), row * (SIZE + t.SPACING))
    write(image, TILES / "transitions.png")


def tile_id(row: int, mask: int) -> int:
    return row * COLUMNS + mask


def corner_set(name: str, color: str, row: int) -> dict:
    """The Tiled corner set of one terrain over the grass, whose wang ids list the corners in the order top, top right, right, bottom right, bottom, bottom left, left and top left."""
    tiles = [{"tileid": tile_id(row, mask), "wangid": [0, (mask >> 1) & 1, 0, (mask >> 2) & 1, 0, (mask >> 3) & 1, 0, mask & 1]} for mask in range(1, 16)]
    return {"name": name, "type": "corner", "tile": -1, "colors": [{"name": name, "color": color, "tile": tile_id(row, 15), "probability": 1}], "wangtiles": tiles}


def transitions() -> dict:
    width, height = t.atlas_size(COLUMNS, GRASS + 1, SIZE, SIZE)
    liquid = [prop("liquid", "bool", True), prop("depth", "float", 1.5)]
    tiles = [{"id": tile_id(WATER, mask), "type": "water", "properties": liquid, "animation": [{"tileid": tile_id(WATER + frame, mask), "duration": 180} for frame in range(FRAMES)]} for mask in range(1, 16)]
    tiles += [{"id": tile_id(LAVA, mask), "type": "lava", "animation": [{"tileid": tile_id(LAVA + frame, mask), "duration": 220} for frame in range(FRAMES)]} for mask in range(1, 16)]
    document = tileset("transitions", "../tiles/transitions.png", width, height, SIZE, SIZE, tiles, **t.GRID)
    document["wangsets"] = [corner_set("water", "#ff2fb3c4", WATER), corner_set("sand", "#fff3d58d", SAND), corner_set("path", "#ff8a5a32", PATH), corner_set("lava", "#ffff7043", LAVA), corner_set("grass", "#ff7bc74d", GRASS)]
    return document


def corner(row: int, holds, column: int, line: int) -> int | None:
    """Returns the local tile of a set for a cell from whether each of its corners holds the terrain, where `holds` tells it for a vertex, or nothing when no corner holds it."""
    mask = holds(column, line) | holds(column + 1, line) << 1 | holds(column + 1, line + 1) << 2 | holds(column, line + 1) << 3
    return tile_id(row, mask) if mask else None
