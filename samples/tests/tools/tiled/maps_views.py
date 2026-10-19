"""The maps of the Tiled tests that show how maps look: every orientation, an infinite map, tile animations, image layers, groups and a world."""

from __future__ import annotations

import random
from pathlib import Path

import tilesets as t
import transitions as tr
from tiled_json import MapBuilder, noise, prop, save

TERRAIN = "../tilesets/terrain.tsj"
PROPS = "../tilesets/props.tsj"
PROPS_GID = 33
TRANSITIONS = "../tilesets/transitions.tsj"
TRANSITIONS_GID = 41


def gid(tile: int) -> int:
    return tile + 1


def landscape(width: int, height: int, seed: int, offset: int = 0):
    """Water, sand, grass and flowers from noise, where `offset` shifts negative cells of infinite maps into the noise."""
    heights = noise(width + offset * 2, height + offset * 2, 7, seed)
    details = noise(width + offset * 2, height + offset * 2, 2, seed + 1)

    def cell(column: int, row: int) -> int:
        value = heights(column + offset, row + offset)
        if value < 0.36:
            return gid(t.WATER)
        if value < 0.42:
            return gid(t.SAND)
        return gid(t.FLOWERS if details(column + offset, row + offset) > 0.72 else t.GRASS)

    return cell


def transition(row: int, holds):
    """A cell of a layer of the transitions tileset, whose tile the terrain at the four corners of the cell picks."""

    def cell(column: int, line: int) -> int:
        tile = tr.corner(row, holds, column, line)
        return 0 if tile is None else TRANSITIONS_GID + tile

    return cell


def orthogonal(folder: Path) -> None:
    builder = MapBuilder(30, 18).tileset(1, TERRAIN).tileset(TRANSITIONS_GID, TRANSITIONS)
    heights = noise(31, 19, 7, 11)
    details = noise(30, 18, 2, 12)
    rng = random.Random(12)
    ruins = {(column, 4) for column in range(18, 26)} | {(18, row) for row in range(4, 10)} | {(25, row) for row in range(4, 10)}

    # The water and its sand lie on the corners of the cells, so the transitions blend them into the grass.
    def wet(column: int, line: int) -> int:
        return int(heights(column, line) < 0.36)

    def sandy(column: int, line: int) -> int:
        return int(heights(column, line) < 0.43)

    def ground(column: int, row: int) -> int:
        return gid(t.FLOWERS if details(column, row) > 0.72 and not any(sandy(column + dx, row + dy) for dx in (0, 1) for dy in (0, 1)) else t.GRASS)

    def decor(column: int, row: int) -> int:
        if (column, row) in ruins:
            return gid(t.TORCH if (column, row) in {(20, 4), (23, 4)} else t.WALL)
        if ground(column, row) == gid(t.GRASS) and not any(sandy(column + dx, row + dy) for dx in (0, 1) for dy in (0, 1)) and rng.random() < 0.06:
            return gid(rng.choice([t.BUSH, t.ROCK]))
        return 0

    builder.add(builder.tiles("ground", ground), builder.tiles("sand", transition(tr.SAND, sandy)), builder.tiles("water", transition(tr.WATER, wet)), builder.tiles("decor", decor))
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
    builder = MapBuilder(24, 14, orientation="oblique", skewx=16, skewy=0).tileset(1, TERRAIN)
    ground = landscape(24, 14, 41)
    builder.add(builder.tiles("ground", ground), builder.tiles("decor", lambda column, row: gid(t.WALL) if row == 6 and 4 <= column <= 18 else 0))
    builder.save(folder / "oblique.tmj")


def infinite(folder: Path) -> None:
    builder = MapBuilder(48, 32, infinite=True).tileset(1, TERRAIN)
    ground = landscape(96, 64, 51, offset=32)
    chunks = [(-32, -16), (-16, -16), (0, -16), (-16, 0), (0, 0), (16, 0), (0, 16), (16, 16), (32, 16)]
    rng = random.Random(52)

    def decor(column: int, row: int) -> int:
        return gid(t.BUSH) if ground(column, row) == gid(t.GRASS) and rng.random() < 0.05 else 0

    builder.add(builder.chunked("ground", chunks, 16, ground), builder.chunked("decor", chunks, 16, decor))
    builder.save(folder / "infinite.tmj")


def animations(folder: Path) -> None:
    builder = MapBuilder(26, 15).tileset(1, TERRAIN)

    def ground(column: int, row: int) -> int:
        if 2 <= column <= 9 and 5 <= row <= 11:
            return gid(t.WATER)
        if column in (17, 18) and row >= 3:
            return gid(t.LAVA)
        return gid(t.STONE if row < 3 else t.GRASS)

    def walls(column: int, row: int) -> int:
        if row == 1:
            return gid(t.TORCH if column % 4 == 1 else t.WALL)
        return 0

    objects = []
    for index in range(8):
        objects.append(builder.object(name=f"coin {index + 1}", type="coin", gid=gid(t.COIN), x=360 + index * 48, y=200 + (index % 2) * 64, width=32, height=32))
    for index in range(3):
        objects.append(builder.object(name=f"slime {index + 1}", type="slime", gid=gid(t.SLIME), x=620 + index * 64, y=420, width=32, height=32))
    builder.add(builder.tiles("ground", ground), builder.tiles("walls", walls), builder.objects("animated", objects))
    builder.save(folder / "animations.tmj")


def parallax(folder: Path) -> None:
    # Parallax is measured from the middle of the map height, so layers that follow the camera line up with the map when the camera shows its middle.
    builder = MapBuilder(80, 12, backgroundcolor="#ff1e3a5f", parallaxoriginx=0, parallaxoriginy=192).tileset(1, TERRAIN)
    rng = random.Random(61)

    # The bushes of the front layer pass nearer than the ground, so they stand at the bottom edge of the view, partly below it.
    def front(column: int, row: int) -> int:
        return gid(t.BUSH) if row == 11 and rng.random() < 0.25 else 0

    # The ground is a strip seen from the side, whose grass covers the foot of the near hills, so the trees stand on it.
    builder.add(
        builder.image("sky", "../images/sky.png", 16, 384, repeatx=True, parallaxx=0, parallaxy=0),
        builder.image("clouds", "../images/clouds.png", 320, 160, repeatx=True, repeaty=True, parallaxx=0.15, parallaxy=0.15, opacity=0.8),
        builder.image("far hills", "../images/hills-far.png", 640, 200, repeatx=True, parallaxx=0.3, parallaxy=1, offsety=110, tintcolor="#ff9fb0c8"),
        builder.image("near hills", "../images/hills-near.png", 640, 160, repeatx=True, parallaxx=0.6, parallaxy=1, offsety=140),
        builder.image("ground", "../images/ground.png", 640, 112, repeatx=True, offsety=272),
        builder.tiles("front", front, parallaxx=1.4, parallaxy=1, offsety=14),
    )
    builder.save(folder / "parallax.tmj")


def groups(folder: Path) -> None:
    # With the parallax origin in the middle of the map, every group lines up when the camera centers the map and drifts as it moves away.
    builder = MapBuilder(30, 18, parallaxoriginx=480, parallaxoriginy=288).tileset(1, TERRAIN)
    houses = [(4, 4, 6, 5), (13, 3, 7, 6), (5, 11, 8, 4), (18, 11, 6, 4)]

    def inside(column: int, row: int, border: bool) -> bool:
        for x, y, width, height in houses:
            if x <= column < x + width and y <= row < y + height:
                edge = column in (x, x + width - 1) or row in (y, y + height - 1)
                return edge if border else True
        return False

    rng = random.Random(71)
    background = builder.group("background", [builder.tiles("rocks", lambda column, row: gid(t.ROCK) if rng.random() < 0.08 else 0)], parallaxx=0.5, parallaxy=0.5, tintcolor="#ff8090a8")
    upstairs = builder.group("upstairs", [builder.tiles("roofs", lambda column, row: gid(t.STONE) if inside(column, row, False) else 0)], offsety=-16, opacity=0.75, parallaxx=1.2, parallaxy=1.2, tintcolor="#ffa0c0ff")
    town = builder.group("town", [builder.tiles("walls", lambda column, row: gid(t.WALL) if inside(column, row, True) else 0), upstairs], offsetx=64, offsety=32, tintcolor="#ffffd8b0")
    builder.add(builder.tiles("ground", lambda column, row: gid(t.GRASS if (column + row) % 7 else t.FLOWERS)), background, town)
    builder.save(folder / "groups.tmj")


def world(folder: Path) -> None:
    """Five maps of 20 by 12 cells: a castle listed in the world file and four islands its pattern finds."""
    castle = MapBuilder(20, 12, properties=[prop("title", "string", "Castle")]).tileset(1, "../" + TERRAIN)
    castle.add(castle.tiles("ground", lambda column, row: gid(t.STONE)), castle.tiles("walls", lambda column, row: gid(t.WALL) if column in (0, 19) or row in (0, 11) or (column == 10 and row % 3) else 0))
    castle.save(folder / "castle.tmj")
    for x in range(2):
        for y in range(2):
            island = MapBuilder(20, 12, properties=[prop("title", "string", f"Island {x}, {y}")]).tileset(1, "../" + TERRAIN)
            shape = noise(20, 12, 4, 80 + x * 2 + y)

            def cell(column: int, row: int) -> int:
                edge = min(column, row, 19 - column, 11 - row)
                value = shape(column, row) + edge * 0.08
                return gid(t.WATER if value < 0.5 else (t.SAND if value < 0.58 else t.GRASS))

            island.add(island.tiles("ground", cell))
            island.save(folder / f"island-{x}-{y}.tmj")
    save({
        "type": "world",
        "maps": [{"fileName": "castle.tmj", "x": -640, "y": 0, "width": 640, "height": 384}],
        "patterns": [{"regexp": "island-(\\d+)-(\\d+)\\.tmj", "multiplierX": 640, "multiplierY": 384, "offsetX": 0, "offsetY": 0, "mapWidth": 640, "mapHeight": 384}],
        "onlyShowAdjacentMaps": False,
    }, folder / "overworld.world")
