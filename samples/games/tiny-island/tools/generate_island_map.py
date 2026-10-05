"""Generates the Tiny Island map as Tiled JSON files that open and edit in Tiled 1.12.

The island shape comes from seeded noise, so the same seed always gives the same map. The tilesets use the art in `content/tiny_swords` of the sample, which the generator needs.
"""

from __future__ import annotations

import argparse
import json
import math
import random
from pathlib import Path

TILE = 64
WIDTH = 56
HEIGHT = 36
TILESET_COLUMNS = 9
WATER_COLOR = "#47aba9"
ART = "../tiny_swords"

FLAT_ORIGIN = (0, 0)
PLATEAU_ORIGIN = (5, 0)
CLIFF_ROW = 4

FOAM_FRAMES = 16
BUSH_FRAMES = 8
WATER_ROCK_FRAMES = 16


def value_noise(width: int, height: int, step: int, rng: random.Random) -> list[list[float]]:
    columns = width // step + 2
    rows = height // step + 2
    lattice = [[rng.random() for _ in range(columns)] for _ in range(rows)]

    def smooth(t: float) -> float:
        return t * t * (3 - 2 * t)

    field = []
    for y in range(height):
        line = []
        for x in range(width):
            gx, gy = x / step, y / step
            x0, y0 = int(gx), int(gy)
            tx, ty = smooth(gx - x0), smooth(gy - y0)
            top = lattice[y0][x0] * (1 - tx) + lattice[y0][x0 + 1] * tx
            bottom = lattice[y0 + 1][x0] * (1 - tx) + lattice[y0 + 1][x0 + 1] * tx
            line.append(top * (1 - ty) + bottom * ty)
        field.append(line)
    return field


def neighbours(grid: list[list[bool]], x: int, y: int) -> int:
    count = 0
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            if (dx or dy) and 0 <= x + dx < WIDTH and 0 <= y + dy < HEIGHT and grid[y + dy][x + dx]:
                count += 1
    return count


def largest_region(grid: list[list[bool]], value: bool) -> set[tuple[int, int]]:
    seen: set[tuple[int, int]] = set()
    best: set[tuple[int, int]] = set()
    for y in range(HEIGHT):
        for x in range(WIDTH):
            if grid[y][x] != value or (x, y) in seen:
                continue
            region = set()
            stack = [(x, y)]
            seen.add((x, y))
            while stack:
                cx, cy = stack.pop()
                region.add((cx, cy))
                for nx, ny in ((cx + 1, cy), (cx - 1, cy), (cx, cy + 1), (cx, cy - 1)):
                    if 0 <= nx < WIDTH and 0 <= ny < HEIGHT and grid[ny][nx] == value and (nx, ny) not in seen:
                        seen.add((nx, ny))
                        stack.append((nx, ny))
            if len(region) > len(best):
                best = region
    return best


def island_mask(rng: random.Random) -> list[list[bool]]:
    noise = value_noise(WIDTH, HEIGHT, 6, rng)
    cx, cy = WIDTH / 2, HEIGHT / 2
    rx, ry = WIDTH * 0.38, HEIGHT * 0.36
    grid = [[math.hypot((x + 0.5 - cx) / rx, (y + 0.5 - cy) / ry) + (noise[y][x] - 0.5) * 0.5 < 1.0 for x in range(WIDTH)] for y in range(HEIGHT)]

    # Cellular smoothing rounds the coast, then only the largest island stays and lakes inside it fill in.
    for _ in range(3):
        grid = [[neighbours(grid, x, y) >= 5 or (grid[y][x] and neighbours(grid, x, y) >= 4) for x in range(WIDTH)] for y in range(HEIGHT)]
    land = largest_region(grid, True)
    grid = [[(x, y) in land for x in range(WIDTH)] for y in range(HEIGHT)]
    ocean = largest_region(grid, False)
    grid = [[(x, y) not in ocean for x in range(WIDTH)] for y in range(HEIGHT)]
    for y in range(HEIGHT):
        for x in range(WIDTH):
            if x < 4 or y < 3 or x >= WIDTH - 4 or y >= HEIGHT - 3:
                grid[y][x] = False
    return grid


def plateau_mask(land: list[list[bool]], rng: random.Random) -> list[list[bool]]:
    """Raises a plateau in the north east, far enough from the coast that its cliff always stands on grass."""
    cx, cy = WIDTH * 0.66, HEIGHT * 0.34
    noise = value_noise(WIDTH, HEIGHT, 3, rng)
    grid = [[False] * WIDTH for _ in range(HEIGHT)]
    for y in range(HEIGHT):
        for x in range(WIDTH):
            inside = math.hypot((x + 0.5 - cx) / 6.0, (y + 0.5 - cy) / 3.5) + (noise[y][x] - 0.5) * 0.4 < 1.0
            surrounded = all(0 <= x + dx < WIDTH and 0 <= y + dy < HEIGHT and land[y + dy][x + dx] for dy in range(-2, 4) for dx in range(-2, 3))
            grid[y][x] = inside and surrounded
    region = largest_region(grid, True)
    return [[(x, y) in region for x in range(WIDTH)] for y in range(HEIGHT)]


def edge_tile(grid: list[list[bool]], x: int, y: int, origin: tuple[int, int]) -> int:
    """Picks the tile of a 4x4 edge block from the four direct neighbours, as a local tile id."""

    def filled(nx: int, ny: int) -> bool:
        return 0 <= nx < WIDTH and 0 <= ny < HEIGHT and grid[ny][nx]

    west, east, north, south = filled(x - 1, y), filled(x + 1, y), filled(x, y - 1), filled(x, y + 1)
    column = {(False, True): 0, (True, True): 1, (True, False): 2, (False, False): 3}[(west, east)]
    row = {(False, True): 0, (True, True): 1, (True, False): 2, (False, False): 3}[(north, south)]
    return (origin[1] + row) * TILESET_COLUMNS + origin[0] + column


def cliff_tile(plateau: list[list[bool]], x: int, y: int) -> int:
    west = x > 0 and plateau[y][x - 1] and not plateau[y + 1][x - 1]
    east = x + 1 < WIDTH and plateau[y][x + 1] and not plateau[y + 1][x + 1]
    column = {(False, True): 0, (True, True): 1, (True, False): 2, (False, False): 3}[(west, east)]
    return CLIFF_ROW * TILESET_COLUMNS + PLATEAU_ORIGIN[0] + column


def merged_rectangles(blocked: list[list[bool]]) -> list[tuple[int, int, int, int]]:
    """Covers the blocked cells with few rectangles: row runs first, then runs that repeat on the next row grow down."""
    open_runs: dict[tuple[int, int], list[int]] = {}
    finished = []
    for y in range(HEIGHT + 1):
        runs = set()
        if y < HEIGHT:
            x = 0
            while x < WIDTH:
                if blocked[y][x]:
                    start = x
                    while x < WIDTH and blocked[y][x]:
                        x += 1
                    runs.add((start, x))
                else:
                    x += 1
        for run in list(open_runs):
            if run not in runs:
                top, height = open_runs.pop(run)
                finished.append((run[0], top, run[1] - run[0], height))
        for run in runs:
            if run in open_runs:
                open_runs[run][1] += 1
            else:
                open_runs[run] = [y, 1]
    return sorted(finished, key=lambda rect: (rect[1], rect[0]))


def edge_wang_set(name: str, origin: tuple[int, int]) -> dict:
    tiles = []
    for row in range(4):
        for column in range(4):
            north = row in (1, 2)
            south = row in (0, 1)
            west = column in (1, 2)
            east = column in (0, 1)
            wang = [int(north), 0, int(east), 0, int(south), 0, int(west), 0]
            tiles.append({"tileid": (origin[1] + row) * TILESET_COLUMNS + origin[0] + column, "wangid": wang})
    return {"name": name, "type": "edge", "tile": (origin[1] + 1) * TILESET_COLUMNS + origin[0] + 1, "colors": [{"name": name, "color": "#ff7cb342", "tile": -1, "probability": 1}], "wangtiles": tiles}


def strip_tiles(first_id: int, image: str, frame: int, frames: int, height: int, duration: int) -> list[dict]:
    tiles = []
    for index in range(frames):
        tile = {"id": first_id + index, "image": image, "imagewidth": frame * frames, "imageheight": height, "x": index * frame, "y": 0, "width": frame, "height": height}
        tiles.append(tile)
    tiles[0]["animation"] = [{"tileid": first_id + index, "duration": duration} for index in range(frames)]
    return tiles


def tilesets() -> dict[str, dict]:
    terrain = {
        "type": "tileset", "version": "1.10", "tiledversion": "1.12.2", "name": "terrain",
        "image": f"{ART}/terrain/tileset/tilemap_color1.png", "imagewidth": 576, "imageheight": 384,
        "tilewidth": TILE, "tileheight": TILE, "tilecount": 54, "columns": TILESET_COLUMNS, "margin": 0, "spacing": 0,
        "wangsets": [edge_wang_set("Grass", FLAT_ORIGIN), edge_wang_set("High ground", PLATEAU_ORIGIN)],
    }
    foam = {
        "type": "tileset", "version": "1.10", "tiledversion": "1.12.2", "name": "foam",
        "image": f"{ART}/terrain/tileset/water_foam.png", "imagewidth": 3072, "imageheight": 192,
        "tilewidth": 192, "tileheight": 192, "tilecount": FOAM_FRAMES, "columns": FOAM_FRAMES, "margin": 0, "spacing": 0,
        "tileoffset": {"x": -TILE, "y": TILE},
        "tiles": [{"id": 0, "animation": [{"tileid": index, "duration": 90} for index in range(FOAM_FRAMES)]}],
    }
    shadow = {
        "type": "tileset", "version": "1.10", "tiledversion": "1.12.2", "name": "shadow",
        "image": f"{ART}/terrain/tileset/shadow.png", "imagewidth": 192, "imageheight": 192,
        "tilewidth": 192, "tileheight": 192, "tilecount": 1, "columns": 1, "margin": 0, "spacing": 0,
        "tileoffset": {"x": -TILE, "y": TILE},
    }

    tiles = []
    for bush in range(4):
        tiles += strip_tiles(len(tiles), f"{ART}/terrain/decorations/bushes/bushe{bush + 1}.png", 128, BUSH_FRAMES, 128, 110)
    for rock in range(4):
        tiles.append({"id": len(tiles), "image": f"{ART}/terrain/decorations/rocks/rock{rock + 1}.png", "imagewidth": 64, "imageheight": 64})
    for rock in range(4):
        tiles += strip_tiles(len(tiles), f"{ART}/terrain/decorations/rocks_in_the_water/water_rocks_0{rock + 1}.png", 64, WATER_ROCK_FRAMES, 64, 110)
    for cloud in range(8):
        tiles.append({"id": len(tiles), "image": f"{ART}/terrain/decorations/clouds/clouds_0{cloud + 1}.png", "imagewidth": 576, "imageheight": 256})
    decorations = {
        "type": "tileset", "version": "1.10", "tiledversion": "1.12.2", "name": "decorations",
        "tilewidth": 576, "tileheight": 256, "tilecount": len(tiles), "columns": 0, "margin": 0, "spacing": 0,
        "objectalignment": "bottom", "grid": {"orientation": "orthogonal", "width": 1, "height": 1}, "tiles": tiles,
    }
    return {"terrain.tsj": terrain, "foam.tsj": foam, "shadow.tsj": shadow, "decorations.tsj": decorations}


def decoration_ids() -> dict[str, list[int]]:
    bushes = [index * BUSH_FRAMES for index in range(4)]
    rocks = [4 * BUSH_FRAMES + index for index in range(4)]
    water_rocks = [4 * BUSH_FRAMES + 4 + index * WATER_ROCK_FRAMES for index in range(4)]
    clouds_start = 4 * BUSH_FRAMES + 4 + 4 * WATER_ROCK_FRAMES
    return {"bushes": bushes, "rocks": rocks, "water_rocks": water_rocks, "clouds": [clouds_start + index for index in range(8)]}


def build_map(seed: int) -> dict:
    rng = random.Random(seed)
    land = island_mask(rng)
    plateau = plateau_mask(land, rng)
    cliff = [[y > 0 and plateau[y - 1][x] and not plateau[y][x] for x in range(WIDTH)] for y in range(HEIGHT)]

    firstgid = {"terrain.tsj": 1, "foam.tsj": 55, "shadow.tsj": 55 + FOAM_FRAMES}
    firstgid["decorations.tsj"] = firstgid["shadow.tsj"] + 1
    empty = [0] * (WIDTH * HEIGHT)

    foam = list(empty)
    ground = list(empty)
    shadow = list(empty)
    cliffs = list(empty)
    high = list(empty)
    for y in range(HEIGHT):
        for x in range(WIDTH):
            index = y * WIDTH + x
            if land[y][x]:
                ground[index] = firstgid["terrain.tsj"] + edge_tile(land, x, y, FLAT_ORIGIN)
                if neighbours(land, x, y) < 8:
                    foam[index] = firstgid["foam.tsj"]
            if plateau[y][x]:
                high[index] = firstgid["terrain.tsj"] + edge_tile(plateau, x, y, PLATEAU_ORIGIN)
            if cliff[y][x]:
                cliffs[index] = firstgid["terrain.tsj"] + cliff_tile(plateau, x, y - 1)
                shadow[index] = firstgid["shadow.tsj"]

    walkable = [[land[y][x] and not plateau[y][x] and not cliff[y][x] for x in range(WIDTH)] for y in range(HEIGHT)]
    center = (WIDTH // 2, HEIGHT // 2)
    while not walkable[center[1]][center[0]]:
        center = (center[0] - 1, center[1] + 1)

    next_object = [1]

    def new_object(**fields) -> dict:
        fields.setdefault("name", "")
        fields.setdefault("type", "")
        fields.update({"id": next_object[0], "rotation": 0, "visible": True})
        fields.setdefault("width", 0)
        fields.setdefault("height", 0)
        next_object[0] += 1
        return fields

    gameplay = [
        new_object(name="campfire", type="campfire", x=(center[0] + 0.5) * TILE, y=(center[1] + 0.5) * TILE, point=True),
        new_object(name="player_start", type="player_start", x=(center[0] + 0.5) * TILE, y=(center[1] + 2.5) * TILE, point=True),
    ]

    # Trees grow in four regions around the fire, each the land bounds of one quarter of the island.
    for qx, qy in ((0, 0), (1, 0), (0, 1), (1, 1)):
        cells = [(x, y) for y in range(HEIGHT) for x in range(WIDTH) if walkable[y][x] and (x >= center[0]) == bool(qx) and (y >= center[1]) == bool(qy)]
        if cells:
            left = min(x for x, _ in cells) * TILE
            top = min(y for _, y in cells) * TILE
            right = (max(x for x, _ in cells) + 1) * TILE
            bottom = (max(y for _, y in cells) + 1) * TILE
            gameplay.append(new_object(name=f"trees_{qx}{qy}", type="tree_region", x=left, y=top, width=right - left, height=bottom - top))

    # Enemies wade ashore at walkable coast cells spread evenly around the island.
    coast = [(x, y) for y in range(HEIGHT) for x in range(WIDTH) if walkable[y][x] and neighbours(land, x, y) < 8]
    def bearing_gap(cell: tuple[int, int], angle: float) -> float:
        difference = math.atan2(cell[1] - center[1], cell[0] - center[0]) - angle
        return abs(math.atan2(math.sin(difference), math.cos(difference)))

    for step in range(12):
        angle = step / 12 * math.tau
        x, y = min(coast, key=lambda cell: bearing_gap(cell, angle))
        gameplay.append(new_object(name=f"spawn_{step}", type="enemy_spawn", x=(x + 0.5) * TILE, y=(y + 0.5) * TILE, point=True))

    ids = decoration_ids()
    decorations = []
    for _ in range(26):
        x, y = rng.randrange(WIDTH), rng.randrange(HEIGHT)
        if walkable[y][x] and math.hypot(x - center[0], y - center[1]) > 5:
            gid = firstgid["decorations.tsj"] + rng.choice(ids["bushes"] + ids["rocks"])
            size = 128 if gid - firstgid["decorations.tsj"] in ids["bushes"] else 64
            decorations.append(new_object(type="decoration", gid=gid, x=(x + 0.5) * TILE, y=(y + 1) * TILE, width=size, height=size))
    for _ in range(18):
        x, y = rng.randrange(WIDTH), rng.randrange(HEIGHT)
        if not land[y][x] and neighbours(land, x, y) == 0 and any(land[y + dy][x + dx] for dy in range(-3, 4) for dx in range(-3, 4) if 0 <= x + dx < WIDTH and 0 <= y + dy < HEIGHT):
            decorations.append(new_object(type="decoration", gid=firstgid["decorations.tsj"] + rng.choice(ids["water_rocks"]), x=(x + 0.5) * TILE, y=(y + 1) * TILE, width=64, height=64))
    clouds = [new_object(type="cloud", gid=firstgid["decorations.tsj"] + rng.choice(ids["clouds"]), x=rng.uniform(0, WIDTH * TILE), y=rng.uniform(256, HEIGHT * TILE), width=576, height=256) for _ in range(9)]

    blocked = [[not walkable[y][x] for x in range(WIDTH)] for y in range(HEIGHT)]
    collision = [new_object(type="collision", x=x * TILE, y=y * TILE, width=w * TILE, height=h * TILE) for x, y, w, h in merged_rectangles(blocked)]

    def tile_layer(layer_id: int, name: str, data: list[int], properties: list[dict] | None = None) -> dict:
        layer = {"id": layer_id, "name": name, "type": "tilelayer", "x": 0, "y": 0, "width": WIDTH, "height": HEIGHT, "opacity": 1, "visible": True, "data": data}
        if properties:
            layer["properties"] = properties
        return layer

    def object_layer(layer_id: int, name: str, objects: list[dict], **extra) -> dict:
        return {"id": layer_id, "name": name, "type": "objectgroup", "x": 0, "y": 0, "opacity": 1, "visible": True, "draworder": "topdown", "objects": objects, **extra}

    no_collision = [{"name": "collision", "type": "bool", "value": False}]
    layers = [
        tile_layer(1, "foam", foam, no_collision),
        tile_layer(2, "ground", ground, no_collision),
        tile_layer(3, "shadow", shadow, no_collision),
        tile_layer(4, "cliffs", cliffs, no_collision),
        tile_layer(5, "plateau", high, no_collision),
        object_layer(6, "decorations", decorations),
        object_layer(7, "gameplay", gameplay),
        object_layer(8, "collision", collision, **{"class": "collision", "visible": False}),
        object_layer(9, "clouds", clouds, opacity=0.55, parallaxx=1.25, parallaxy=1.25),
    ]
    return {
        "type": "map", "version": "1.10", "tiledversion": "1.12.2", "orientation": "orthogonal", "renderorder": "right-down",
        "width": WIDTH, "height": HEIGHT, "tilewidth": TILE, "tileheight": TILE, "infinite": False, "backgroundcolor": WATER_COLOR,
        "compressionlevel": -1, "nextlayerid": len(layers) + 1, "nextobjectid": next_object[0],
        "properties": [{"name": "seed", "type": "int", "value": seed}],
        "tilesets": [{"firstgid": gid, "source": name} for name, gid in firstgid.items()],
        "layers": layers,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--package", type=Path, default=Path(__file__).resolve().parents[1], help="The Tiny Island package folder, the sample that holds this tool by default.")
    parser.add_argument("--seed", type=int, default=20260927)
    args = parser.parse_args()

    maps = args.package / "content" / "maps"
    if not (args.package / "content" / "tiny_swords").is_dir():
        raise SystemExit(f'The art of the tilesets is missing from "{args.package / "content" / "tiny_swords"}".')
    maps.mkdir(parents=True, exist_ok=True)
    for name, tileset in tilesets().items():
        (maps / name).write_text(json.dumps(tileset, indent=2) + "\n")
    (maps / "island.tmj").write_text(json.dumps(build_map(args.seed), indent=2) + "\n")
    print(f'Wrote the island map and its tilesets to "{maps}".')


if __name__ == "__main__":
    main()
