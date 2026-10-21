"""The maps of the Tiled tests that show how maps look: every orientation, an infinite map, tile animations, image layers, groups and a world."""

from __future__ import annotations

import random
from pathlib import Path

import tilesets as t
from nature import PROPS, PROPS_GID, ROOFS, ROOFS_GID, SCENERY, SCENERY_GID, TERRAIN, TRANSITIONS, TRANSITIONS_GID, Ground, Props, gid, scatter
from tiled_json import MapBuilder, noise, prop, save


def landscape(builder: MapBuilder, ground: Ground, seed: int, trees: int, bushes: int, extras: tuple[str, ...] = ()) -> Props:
    """Trees, bushes, rocks and flowering bushes on the open cells of a map, with `extras` placed among them once each."""
    rng = random.Random(seed)
    props = Props(builder)
    # Trees stand four cells tall, so they keep off the top rows and the side columns, where they would leave the map.
    cells = [(column, row) for column, row in ground.open_cells() if 1 <= column <= ground.width - 2]
    planted = scatter([(column, row) for column, row in cells if row >= 4], trees, 2.4, seed)
    for cell in planted:
        props.on(rng.choice(("tree", "tree", "pine")), cell, rng)
    small = scatter(cells, bushes + len(extras), 1.6, seed + 1, planted)
    for index, cell in enumerate(small):
        kind = extras[index] if index < len(extras) else rng.choice(("bush", "bush", "rock", "blossoms", "mushrooms"))
        props.on(kind, cell, rng)
    return props


def orthogonal(folder: Path) -> None:
    """A meadow with a lake, a road that winds around it and woods, with a lamp, a sign post and a well by the road."""
    width, height = 26, 12
    builder = MapBuilder(width, height).tileset(1, TERRAIN).tileset(PROPS_GID, PROPS).tileset(TRANSITIONS_GID, TRANSITIONS).tileset(SCENERY_GID, SCENERY)
    ground = Ground(width, height, 11)
    ground.blob("water", 19.5, 7.5, 5.2, 3.4).blob("water", 23.5, 3.5, 2.5, 2.2)
    ground.road("path", [(0, 4.5), (6, 4), (11, 6.5), (13.5, 10), (13, 12.5)], 0.9)
    rng = random.Random(12)
    props = landscape(builder, ground, 13, 9, 12, ("well", "campfire", "stump"))
    props.at("lamp", 7.4 * 32, 3.2 * 32)
    props.at("signpost", 11.3 * 32, 9.2 * 32)
    for x in (1.2, 4.1):
        props.on("reeds", (int(19 + x), 4), rng)
    builder.add(ground.meadow(builder, 12), ground.layer(builder, "path", "path"), ground.layer(builder, "water", "water"), builder.objects("props", props.objects))
    builder.save(folder / "orthogonal.tmj")


def diamond_map(folder: Path, name: str, width: int, height: int, **extra) -> None:
    builder = MapBuilder(width, height, 64, 32, **extra).tileset(1, "../tilesets/diamonds.tsj")
    heights = noise(width, height, 5, 21)

    def cell(column: int, row: int) -> int:
        value = heights(column, row)
        if column == row or column == row + 1:
            return 4
        return 2 if value < 0.38 else (3 if value < 0.45 else 1)

    builder.add(builder.tiles("ground", cell))
    builder.save(folder / f"{name}.tmj")


def hexagonal(folder: Path) -> None:
    builder = MapBuilder(16, 12, 56, 64, "hexagonal", hexsidelength=32, staggeraxis="y", staggerindex="odd").tileset(1, "../tilesets/hexagons.tsj")
    heights = noise(16, 12, 4, 31)

    def cell(column: int, row: int) -> int:
        value = heights(column, row)
        return 2 if value < 0.38 else (3 if value < 0.45 else (4 if value > 0.62 else 1))

    builder.add(builder.tiles("ground", cell))
    builder.save(folder / "hexagonal.tmj")


def oblique(folder: Path) -> None:
    """A meadow leaning by its skew, with a pond and a road whose corner sets lean with it."""
    width, height = 24, 12
    builder = MapBuilder(width, height, orientation="oblique", skewx=16, skewy=0).tileset(1, TERRAIN).tileset(TRANSITIONS_GID, TRANSITIONS)
    ground = Ground(width, height, 41)
    ground.blob("water", 16, 6, 4.5, 3.2)
    ground.road("path", [(0, 3), (8, 4), (10, 12)], 0.9)
    rng = random.Random(42)
    builder.add(ground.meadow(builder, 41), ground.layer(builder, "path", "path"), ground.layer(builder, "water", "water"), builder.tiles("decor", lambda column, row: gid(rng.choice((t.BUSH, t.ROCK))) if not ground.touches(column, row) and rng.random() < 0.07 else 0))
    builder.save(folder / "oblique.tmj")


def infinite(folder: Path) -> None:
    """Chunks of an endless coast, some at negative cells, with lakes, beaches and a road that cross the edges of the chunks."""
    builder = MapBuilder(48, 32, infinite=True).tileset(1, TERRAIN).tileset(TRANSITIONS_GID, TRANSITIONS)
    chunks = [(-32, -16), (-16, -16), (0, -16), (-16, 0), (0, 0), (16, 0), (0, 16), (16, 16), (32, 16)]
    offset = 32
    ground = Ground(96, 64, 51)
    heights = noise(97, 65, 7, 52)
    ground.paint("water", lambda column, line: heights(column, line) < 0.34)
    ground.paint("sand", lambda column, line: heights(column, line) < 0.4)
    ground.road("path", [(4, 30), (30, 26), (52, 38), (80, 34), (95, 46)], 0.9)
    rng = random.Random(53)

    # The chunks reach negative cells, which the ground holds from 0.
    def shifted(cell):
        return lambda column, row: cell(column + offset, row + offset)

    def grass(column: int, row: int) -> int:
        return gid(t.FLOWERS if not ground.touches(column, row) and rng.random() < 0.1 else t.GRASS)

    def decor(column: int, row: int) -> int:
        return gid(rng.choice((t.BUSH, t.BUSH, t.ROCK))) if not ground.touches(column, row) and rng.random() < 0.06 else 0

    builder.add(
        builder.chunked("ground", chunks, 16, shifted(grass)),
        builder.chunked("sand", chunks, 16, shifted(ground.cells("sand"))),
        builder.chunked("path", chunks, 16, shifted(ground.cells("path"))),
        builder.chunked("water", chunks, 16, shifted(ground.cells("water"))),
        builder.chunked("decor", chunks, 16, shifted(decor)),
    )
    builder.save(folder / "infinite.tmj")


def animations(folder: Path) -> None:
    """A courtyard under a castle wall with torches, a pond and a lava pit whose tiles play their frames, and coins and slimes that are animated tile objects."""
    width, height = 24, 11
    builder = MapBuilder(width, height).tileset(1, TERRAIN).tileset(TRANSITIONS_GID, TRANSITIONS).tileset(SCENERY_GID, SCENERY)
    ground = Ground(width, height, 61)
    ground.blob("water", 5.5, 7.6, 3.8, 2.3)
    ground.blob("lava", 18.5, 7.8, 3.2, 2.1)
    ground.paint("path", lambda column, line: line <= 2)

    def walls(column: int, row: int) -> int:
        if row == 0:
            return gid(t.TORCH if column % 5 == 2 else t.WALL)
        return 0

    props = Props(builder)
    for index in range(8):
        props.at("coin", (4.2 + index * 2.2) * 32, (4.3 - 0.35 * (index % 2)) * 32, f"coin {index + 1}", type="coin")
    for index, (x, y) in enumerate(((11.2, 7.6), (12.8, 9.9), (11.0, 10.6))):
        props.at("slime", x * 32, y * 32, f"slime {index + 1}", type="slime")
    rng = random.Random(62)
    decor = Props(builder)
    for cell in ((1, 3), (22, 3)):
        decor.on("barrel", cell, rng)
    builder.add(
        ground.meadow(builder, 63),
        ground.layer(builder, "path", "path"),
        ground.layer(builder, "water", "water"),
        ground.layer(builder, "lava", "lava"),
        builder.tiles("walls", walls),
        builder.objects("decor", decor.objects),
        builder.objects("animated", props.objects),
    )
    builder.save(folder / "animations.tmj")


def parallax(folder: Path) -> None:
    # Parallax is measured from the middle of the map height, so layers that follow the camera line up with the map when the camera shows its middle. The grid is 64 units, so the bushes of the front layer draw large.
    builder = MapBuilder(40, 6, 64, 64, backgroundcolor="#ff8fd3f4", parallaxoriginx=0, parallaxoriginy=192).tileset(1, TERRAIN)
    rng = random.Random(61)

    # The bushes of the front layer pass nearer than the ground, so they stand on the grass at the bottom of the view.
    def front(column: int, row: int) -> int:
        return gid(t.BUSH) if row == 5 and rng.random() < 0.35 else 0

    # The ground is a strip seen from the side, whose grass covers the foot of the near hills, so the trees stand on it.
    builder.add(
        builder.image("sky", "../images/sky.png", 16, 384, repeatx=True, parallaxx=0, parallaxy=0),
        builder.image("clouds", "../../camera/clouds.png", 1024, 322, repeatx=True, parallaxx=0.15, parallaxy=0.15, offsety=-20, opacity=0.9),
        builder.image("far hills", "../images/hills-far.png", 640, 200, repeatx=True, parallaxx=0.3, parallaxy=1, offsety=110, tintcolor="#ffb8c8dc"),
        builder.image("near hills", "../images/hills-near.png", 640, 160, repeatx=True, parallaxx=0.6, parallaxy=1, offsety=140),
        builder.image("ground", "../images/ground.png", 640, 112, repeatx=True, offsety=272),
        builder.tiles("front", front, parallaxx=1.4, parallaxy=1, offsety=-64),
    )
    builder.save(folder / "parallax.tmj")


def groups(folder: Path) -> None:
    """A village on an island that floats over the sea: the sea and its reefs far below in a group that lags behind the camera, the island, and a town group whose walls move and tint together, with an upstairs group of roofs that leads the camera as if nearer."""
    # With the parallax origin in the middle of the map, every group lines up when the camera centers the map and drifts as it moves away.
    width, height = 28, 15
    builder = MapBuilder(width, height, parallaxoriginx=width * 16, parallaxoriginy=height * 16).tileset(1, TERRAIN).tileset(PROPS_GID, PROPS).tileset(TRANSITIONS_GID, TRANSITIONS).tileset(SCENERY_GID, SCENERY).tileset(ROOFS_GID, ROOFS)
    island = Ground(width, height, 71)
    island.blob("sand", 14, 7.5, 13.6, 7.2, 0.3)
    island.blob("grass", 14, 7.5, 12.5, 6.3, 0.3)
    island.road("path", [(1.5, 7.6), (10, 7.4), (19, 7.6), (26.5, 7.3)], 0.7)
    houses = [(7, 3, 5, 4, t.CLAY), (16, 3, 6, 4, t.SLATE), (8, 8, 5, 4, t.THATCH), (17, 8, 5, 4, t.CLAY)]
    reef = random.Random(72)

    def walls(column: int, row: int) -> int:
        for x, y, w, h, _ in houses:
            if x <= column < x + w and y <= row < y + h:
                return gid(t.WALL if row == y + h - 1 or column in (x, x + w - 1) or row == y else t.STONE)
        return 0

    roofs = []
    for index, (x, y, w, h, roof) in enumerate(houses):
        roofs.append(builder.object(name=f"roof {index + 1}", type="roof", gid=ROOFS_GID + roof, x=x * 32 - 6, y=y * 32 - 10, width=w * 32 + 12, height=h * 32 + 6))
    rng = random.Random(73)
    props = Props(builder)
    for cell in ((4, 5), (24, 5), (13, 12), (4, 9), (24, 10), (13, 2)):
        props.on(rng.choice(("tree", "pine")), cell, rng, scale=0.9)
    props.on("well", (14, 6), rng, scale=0.8)
    props.on("blossoms", (14, 10), rng)
    props.on("barrel", (12, 6), rng)
    sea = builder.image("sea", "../images/sea.png", 128, 128, repeatx=True, repeaty=True)
    background = builder.group("background", [sea, builder.tiles("reefs", lambda column, row: gid(t.ROCK) if reef.random() < 0.05 else 0)], parallaxx=0.5, parallaxy=0.5, tintcolor="#ff9fb6d6")
    upstairs = builder.group("upstairs", [builder.objects("roofs", roofs)], offsety=-16, opacity=0.95, parallaxx=1.2, parallaxy=1.2)
    town = builder.group("town", [builder.tiles("walls", walls), upstairs], offsetx=0, offsety=0, tintcolor="#ffffe8c8")
    builder.add(background, island.layer(builder, "beach", "sand"), island.layer(builder, "ground", "grass"), island.layer(builder, "path", "path"), builder.objects("trees", props.objects), town)
    builder.save(folder / "groups.tmj")


def world(folder: Path) -> None:
    """Five maps of 20 by 12 cells: a castle listed in the world file and four islands its pattern finds, whose beaches and seas meet at the edges of the maps."""
    castle = MapBuilder(20, 12, properties=[prop("title", "string", "Castle")]).tileset(1, "../" + TERRAIN)
    keep = {(column, row) for column in range(6, 14) for row in range(3, 9)}

    def castle_ground(column: int, row: int) -> int:
        return gid(t.STONE if 1 <= column <= 18 and 1 <= row <= 10 else t.GRASS)

    def castle_walls(column: int, row: int) -> int:
        if column in (0, 19) or row in (0, 11):
            return gid(t.TORCH if (column + row) % 6 == 3 and row in (0, 11) else t.WALL)
        if (column, row) in keep and (column in (6, 13) or row in (3, 8)) and not (row == 8 and column in (9, 10)):
            return gid(t.WALL)
        return 0

    castle.add(castle.tiles("ground", castle_ground), castle.tiles("walls", castle_walls))
    castle.save(folder / "castle.tmj")
    for x in range(2):
        for y in range(2):
            index = x * 2 + y
            island = MapBuilder(20, 12, properties=[prop("title", "string", f"Island {x}, {y}")]).tileset(1, "../" + TERRAIN).tileset(PROPS_GID, "../" + PROPS).tileset(TRANSITIONS_GID, "../" + TRANSITIONS)
            center = (10 + (x - 0.5) * 1.5, 6 + (y - 0.5))
            ground = Ground(20, 12, 80 + index).blob("sand", center[0], center[1], 7.4, 4.4, 0.45).blob("grass", center[0], center[1], 5.4, 2.8, 0.45)
            ground.paint("water", lambda column, line: not ground.holds("sand", column, line))
            ground.corners["grass"] &= ground.corners["sand"]
            rng = random.Random(110 + index)
            props = Props(island)
            meadow = [(c, r) for r in range(12) for c in range(20) if all(ground.holds("grass", c + dx, r + dy) for dx in (0, 1) for dy in (0, 1))]
            for cell in scatter(meadow, 4, 2.5, 120 + index):
                props.on(rng.choice(("tree", "pine")), cell, rng, scale=0.8)
            island.add(
                island.tiles("sand", lambda column, row: gid(t.SAND)),
                ground.layer(island, "ground", "grass"),
                ground.layer(island, "water", "water"),
                island.objects("props", props.objects),
            )
            island.save(folder / f"island-{x}-{y}.tmj")
    save({
        "type": "world",
        "maps": [{"fileName": "castle.tmj", "x": -640, "y": 0, "width": 640, "height": 384}],
        "patterns": [{"regexp": "island-(\\d+)-(\\d+)\\.tmj", "multiplierX": 640, "multiplierY": 384, "offsetX": 0, "offsetY": 0, "mapWidth": 640, "mapHeight": 384}],
        "onlyShowAdjacentMaps": False,
    }, folder / "overworld.world")
