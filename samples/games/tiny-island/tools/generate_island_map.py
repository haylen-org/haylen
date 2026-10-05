"""Generates the Tiny Island map as Tiled JSON files that open and edit in Tiled 1.12.

The island shape comes from seeded noise, so the same seed always gives the same map. The tilesets use the images and atlases in `content/world` of the sample.
"""

from __future__ import annotations

import argparse
import heapq
import json
import math
import random
from pathlib import Path

TILE = 64
WIDTH = 56
HEIGHT = 36
WATER_COLOR = "#2fb3c4"
WORLD = "../world"

TERRAIN_COLUMNS = 16
TERRAIN_ROWS = 21
CORNER_MATERIALS = ("sand", "grass", "high", "path", "rock")
CORNER_NAMES = {"sand": "Sand", "grass": "Grass", "high": "High ground", "path": "Path"}
SHADOW_ROW = 20
FOAM_FRAMES = 4
FOAM_SIZE = 128

DECORATIONS = ("bush", "bush_flowers", "boulder", "stones", "grass", "flowers", "mushrooms", "shell", "driftwood", "water_rock")
CLOUDS = ("cloud_1", "cloud_2", "cloud_3")


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


def filled(grid: list[list[bool]], x: int, y: int) -> bool:
    return 0 <= x < WIDTH and 0 <= y < HEIGHT and grid[y][x]


def neighbours(grid: list[list[bool]], x: int, y: int) -> int:
    return sum(1 for dy in (-1, 0, 1) for dx in (-1, 0, 1) if (dx or dy) and filled(grid, x + dx, y + dy))


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
            surrounded = all(filled(land, x + dx, y + dy) for dy in range(-3, 5) for dx in range(-3, 4))
            grid[y][x] = inside and surrounded
    for _ in range(2):
        grid = [[grid[y][x] and neighbours(grid, x, y) >= 4 for x in range(WIDTH)] for y in range(HEIGHT)]
    region = largest_region(grid, True)

    # Every column hangs down to the lowest row, so the cliff along the south side runs straight.
    bottom = max(y for _, y in region)
    for x in {x for x, _ in region}:
        top = min(y for column, y in region if column == x)
        region |= {(x, y) for y in range(top, bottom + 1)}
    return [[(x, y) in region for x in range(WIDTH)] for y in range(HEIGHT)]


def corner_shape(grid: list[list[bool]], x: int, y: int) -> int:
    """Returns the corner tile shape of the tile between the cells (x, y) and (x + 1, y + 1), one bit per corner cell."""
    return int(filled(grid, x, y)) | int(filled(grid, x + 1, y)) << 1 | int(filled(grid, x, y + 1)) << 2 | int(filled(grid, x + 1, y + 1)) << 3


def corner_layer(grid: list[list[bool]], material: str, firstgid: int, rows_down: int = 0) -> list[int]:
    """Fills a layer that sits half a tile right and down, where every tile takes the four cells around its corners. The texture repeats every two tiles, so the tile picks the copy of the row it shows on, `rows_down` rows below its cell."""
    base = CORNER_MATERIALS.index(material) * 4
    data = [0] * (WIDTH * HEIGHT)
    for y in range(HEIGHT):
        for x in range(WIDTH):
            shape = corner_shape(grid, x, y)
            if shape:
                row = base + (x % 2) + ((y + rows_down) % 2) * 2
                data[y * WIDTH + x] = firstgid + row * TERRAIN_COLUMNS + shape
    return data


def shadow_layer(grid: list[list[bool]], firstgid: int) -> list[int]:
    data = [0] * (WIDTH * HEIGHT)
    for y in range(HEIGHT):
        for x in range(WIDTH):
            shape = corner_shape(grid, x, y)
            if shape:
                data[y * WIDTH + x] = firstgid + SHADOW_ROW * TERRAIN_COLUMNS + shape
    return data


def corner_wang_set(material: str) -> dict:
    base = CORNER_MATERIALS.index(material) * 4
    tiles = []
    for variant in range(4):
        for shape in range(1, 16):
            northwest, northeast, southwest, southeast = shape & 1, shape >> 1 & 1, shape >> 2 & 1, shape >> 3 & 1
            tiles.append({"tileid": (base + variant) * TERRAIN_COLUMNS + shape, "wangid": [0, northeast, 0, southeast, 0, southwest, 0, northwest]})
    name = CORNER_NAMES[material]
    return {"name": name, "type": "corner", "tile": base * TERRAIN_COLUMNS + 15, "colors": [{"name": name, "color": "#ff7cb342", "tile": -1, "probability": 1}], "wangtiles": tiles}


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


def trail(walkable: list[list[bool]], start: tuple[int, int], goal: tuple[int, int], rng: random.Random) -> list[tuple[int, int]]:
    """Finds a winding way between two cells, with random step costs so paths never run perfectly straight."""
    costs = {(x, y): 1 + rng.random() * 2.5 for y in range(HEIGHT) for x in range(WIDTH)}
    frontier = [(0.0, start)]
    came = {start: start}
    spent = {start: 0.0}
    while frontier:
        _, cell = heapq.heappop(frontier)
        if cell == goal:
            break
        x, y = cell
        for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
            if not filled(walkable, nx, ny):
                continue
            cost = spent[cell] + costs[(nx, ny)]
            if (nx, ny) not in spent or cost < spent[(nx, ny)]:
                spent[(nx, ny)] = cost
                came[(nx, ny)] = cell
                heapq.heappush(frontier, (cost + abs(goal[0] - nx) + abs(goal[1] - ny), (nx, ny)))
    cells = []
    cell = goal
    while cell != start:
        cells.append(cell)
        cell = came[cell]
    return cells + [start]


def atlas_frames(package: Path, name: str) -> tuple[dict, dict]:
    document = json.loads((package / "content" / "world" / f"{name}.json").read_text())
    return document["frames"], document["meta"]["size"]


def tilesets(package: Path) -> dict[str, dict]:
    terrain = {
        "type": "tileset", "version": "1.10", "tiledversion": "1.12.2", "name": "terrain",
        "image": f"{WORLD}/terrain.png", "imagewidth": TERRAIN_COLUMNS * TILE, "imageheight": TERRAIN_ROWS * TILE,
        "tilewidth": TILE, "tileheight": TILE, "tilecount": TERRAIN_COLUMNS * TERRAIN_ROWS, "columns": TERRAIN_COLUMNS, "margin": 0, "spacing": 0,
        "wangsets": [corner_wang_set(material) for material in CORNER_NAMES],
    }
    foam = {
        "type": "tileset", "version": "1.10", "tiledversion": "1.12.2", "name": "foam",
        "image": f"{WORLD}/foam.png", "imagewidth": FOAM_SIZE * FOAM_FRAMES, "imageheight": FOAM_SIZE,
        "tilewidth": FOAM_SIZE, "tileheight": FOAM_SIZE, "tilecount": FOAM_FRAMES, "columns": FOAM_FRAMES, "margin": 0, "spacing": 0,
        "tileoffset": {"x": -(FOAM_SIZE - TILE) // 2, "y": (FOAM_SIZE - TILE) // 2},
        "tiles": [{"id": 0, "animation": [{"tileid": index, "duration": 260} for index in range(FOAM_FRAMES)]}],
    }

    tiles = []
    for atlas, names in (("props", DECORATIONS), ("clouds", CLOUDS)):
        frames, size = atlas_frames(package, atlas)
        for name in names:
            rect = frames[name]["frame"]
            tiles.append({"id": len(tiles), "type": name, "image": f"{WORLD}/{atlas}.png", "imagewidth": size["w"], "imageheight": size["h"], "x": rect["x"], "y": rect["y"], "width": rect["w"], "height": rect["h"]})
    decorations = {
        "type": "tileset", "version": "1.10", "tiledversion": "1.12.2", "name": "decorations",
        "tilewidth": max(tile["width"] for tile in tiles), "tileheight": max(tile["height"] for tile in tiles), "tilecount": len(tiles), "columns": 0, "margin": 0, "spacing": 0,
        "objectalignment": "bottom", "grid": {"orientation": "orthogonal", "width": 1, "height": 1}, "tiles": tiles,
    }
    return {"terrain.tsj": terrain, "foam.tsj": foam, "decorations.tsj": decorations}


def build_map(seed: int, package: Path) -> dict:
    rng = random.Random(seed)
    land = island_mask(rng)
    grass = [[land[y][x] and neighbours(land, x, y) == 8 for x in range(WIDTH)] for y in range(HEIGHT)]
    plateau = plateau_mask(grass, rng)
    cliff = [[y > 0 and plateau[y - 1][x] and not plateau[y][x] for x in range(WIDTH)] for y in range(HEIGHT)]
    walkable = [[land[y][x] and not plateau[y][x] and not cliff[y][x] for x in range(WIDTH)] for y in range(HEIGHT)]

    center = (WIDTH // 2, HEIGHT // 2)
    while not all(filled(walkable, center[0] + dx, center[1] + dy) and grass[center[1] + dy][center[0] + dx] for dy in (-1, 0, 1) for dx in (-1, 0, 1)):
        center = (center[0] - 1, center[1] + 1)

    # Enemies wade ashore at walkable coast cells spread evenly around the island.
    coast = [(x, y) for y in range(HEIGHT) for x in range(WIDTH) if walkable[y][x] and not grass[y][x]]

    def bearing_gap(cell: tuple[int, int], angle: float) -> float:
        difference = math.atan2(cell[1] - center[1], cell[0] - center[0]) - angle
        return abs(math.atan2(math.sin(difference), math.cos(difference)))

    spawns = [min(coast, key=lambda cell: bearing_gap(cell, step / 12 * math.tau)) for step in range(12)]

    # Worn paths lead from a clearing around the fire to three of the beaches.
    path = [[False] * WIDTH for _ in range(HEIGHT)]
    for y in range(HEIGHT):
        for x in range(WIDTH):
            path[y][x] = grass[y][x] and walkable[y][x] and math.hypot(x - center[0], y - center[1]) <= 1.6
    grassy = [[walkable[y][x] and grass[y][x] for x in range(WIDTH)] for y in range(HEIGHT)]
    meadow = [(x, y) for y in range(HEIGHT) for x in range(WIDTH) if grassy[y][x]]
    for spawn in (spawns[1], spawns[5], spawns[9]):
        inland = min(meadow, key=lambda cell: (cell[0] - spawn[0]) ** 2 + (cell[1] - spawn[1]) ** 2)
        for x, y in trail(grassy, center, inland, rng):
            path[y][x] = True

    firstgid = {"terrain.tsj": 1, "foam.tsj": 1 + TERRAIN_COLUMNS * TERRAIN_ROWS}
    firstgid["decorations.tsj"] = firstgid["foam.tsj"] + FOAM_FRAMES

    foam = [0] * (WIDTH * HEIGHT)
    for y in range(HEIGHT):
        for x in range(WIDTH):
            if land[y][x] and neighbours(land, x, y) < 8:
                foam[y * WIDTH + x] = firstgid["foam.tsj"]

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

    # Trees and sheep live in four regions around the fire, each the land bounds of one quarter of the island.
    for qx, qy in ((0, 0), (1, 0), (0, 1), (1, 1)):
        cells = [(x, y) for y in range(HEIGHT) for x in range(WIDTH) if walkable[y][x] and (x >= center[0]) == bool(qx) and (y >= center[1]) == bool(qy)]
        if cells:
            left = min(x for x, _ in cells) * TILE
            top = min(y for _, y in cells) * TILE
            right = (max(x for x, _ in cells) + 1) * TILE
            bottom = (max(y for _, y in cells) + 1) * TILE
            gameplay.append(new_object(name=f"wilds_{qx}{qy}", type="wilds", x=left, y=top, width=right - left, height=bottom - top))
    for step, (x, y) in enumerate(spawns):
        gameplay.append(new_object(name=f"spawn_{step}", type="enemy_spawn", x=(x + 0.5) * TILE, y=(y + 0.5) * TILE, point=True))

    frames, _ = atlas_frames(package, "props")
    clouds_frames, _ = atlas_frames(package, "clouds")

    def decoration(name: str, x: float, y: float) -> dict:
        index = DECORATIONS.index(name) if name in DECORATIONS else len(DECORATIONS) + CLOUDS.index(name)
        rect = (frames.get(name) or clouds_frames[name])["frame"]
        kind = "cloud" if name in CLOUDS else "decoration"
        return new_object(type=kind, gid=firstgid["decorations.tsj"] + index, x=x, y=y, width=rect["w"], height=rect["h"])

    taken: set[tuple[int, int]] = set()
    decorations = []
    inland = ("bush", "bush_flowers", "boulder", "stones", "grass", "grass", "flowers", "flowers", "mushrooms")
    for _ in range(220):
        x, y = rng.randrange(WIDTH), rng.randrange(HEIGHT)
        if grassy[y][x] and not path[y][x] and (x, y) not in taken and math.hypot(x - center[0], y - center[1]) > 4:
            taken.add((x, y))
            decorations.append(decoration(rng.choice(inland), (x + rng.uniform(0.25, 0.75)) * TILE, (y + rng.uniform(0.6, 0.95)) * TILE))
    for _ in range(34):
        x, y = rng.choice(coast)
        if (x, y) not in taken:
            taken.add((x, y))
            decorations.append(decoration(rng.choice(("shell", "shell", "driftwood", "stones")), (x + rng.uniform(0.3, 0.7)) * TILE, (y + rng.uniform(0.55, 0.85)) * TILE))
    for _ in range(40):
        x, y = rng.randrange(WIDTH), rng.randrange(HEIGHT)
        near_land = any(filled(land, x + dx, y + dy) for dy in range(-3, 4) for dx in range(-3, 4))
        if not land[y][x] and neighbours(land, x, y) == 0 and near_land and (x, y) not in taken:
            taken.add((x, y))
            decorations.append(decoration("water_rock", (x + 0.5) * TILE, (y + 0.8) * TILE))
    clouds = [decoration(rng.choice(CLOUDS), rng.uniform(0, WIDTH * TILE), rng.uniform(300, HEIGHT * TILE)) for _ in range(7)]

    blocked = [[not walkable[y][x] for x in range(WIDTH)] for y in range(HEIGHT)]
    collision = [new_object(type="collision", x=x * TILE, y=y * TILE, width=w * TILE, height=h * TILE) for x, y, w, h in merged_rectangles(blocked)]

    def tile_layer(layer_id: int, name: str, data: list[int], offset: tuple[int, int] | None = None) -> dict:
        layer = {"id": layer_id, "name": name, "type": "tilelayer", "x": 0, "y": 0, "width": WIDTH, "height": HEIGHT, "opacity": 1, "visible": True, "data": data}
        if offset:
            layer["offsetx"], layer["offsety"] = offset
        layer["properties"] = [{"name": "collision", "type": "bool", "value": False}]
        return layer

    def object_layer(layer_id: int, name: str, objects: list[dict], **extra) -> dict:
        return {"id": layer_id, "name": name, "type": "objectgroup", "x": 0, "y": 0, "opacity": 1, "visible": True, "draworder": "topdown", "objects": objects, **extra}

    terrain = firstgid["terrain.tsj"]
    # Corner layers sit half a tile right and down, and the cliff hangs one cell below the high ground it belongs to.
    corner = (TILE // 2, TILE // 2)
    water = {"id": 1, "name": "water", "type": "imagelayer", "x": 0, "y": 0, "opacity": 1, "visible": True, "image": f"{WORLD}/water.png", "imagewidth": 256, "imageheight": 256, "repeatx": True, "repeaty": True}
    layers = [
        water,
        tile_layer(2, "foam", foam),
        tile_layer(3, "sand", corner_layer(land, "sand", terrain), corner),
        tile_layer(4, "grass", corner_layer(grass, "grass", terrain), corner),
        tile_layer(5, "path", corner_layer(path, "path", terrain), corner),
        tile_layer(6, "shadow", shadow_layer(plateau, terrain), (corner[0] + 8, corner[1] + TILE + 16)),
        tile_layer(7, "cliffs", corner_layer(plateau, "rock", terrain, 1), (corner[0], corner[1] + TILE)),
        tile_layer(8, "plateau", corner_layer(plateau, "high", terrain), corner),
        object_layer(9, "decorations", decorations),
        object_layer(10, "gameplay", gameplay),
        object_layer(11, "collision", collision, **{"class": "collision", "visible": False}),
        object_layer(12, "clouds", clouds, opacity=0.45, parallaxx=1.25, parallaxy=1.25),
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

    world = args.package / "content" / "world"
    for name in ("terrain.png", "foam.png", "water.png", "props.json", "clouds.json"):
        if not (world / name).is_file():
            raise SystemExit(f'The tileset image or atlas "{world / name}" is missing.')
    maps = args.package / "content" / "maps"
    maps.mkdir(parents=True, exist_ok=True)
    for name, tileset in tilesets(args.package).items():
        (maps / name).write_text(json.dumps(tileset, indent=2) + "\n")
    (maps / "island.tmj").write_text(json.dumps(build_map(args.seed, args.package), indent=2) + "\n")
    print(f'Wrote the island map and its tilesets to "{maps}".')


if __name__ == "__main__":
    main()
