"""The maps of the Tiled tests that games read: objects and templates, properties, collision, spawning, y sorting and ray casts."""

from __future__ import annotations

import random
from pathlib import Path

import tilesets as t
from maps_views import PROPS, PROPS_GID, TERRAIN, gid
from tiled_json import MapBuilder, item, prop


def meadow(seed: int):
    rng = random.Random(seed)
    return lambda column, row: gid(t.FLOWERS if rng.random() < 0.08 else t.GRASS)


def objects(folder: Path) -> None:
    builder = MapBuilder(30, 18).tileset(1, TERRAIN).tileset(PROPS_GID, PROPS)
    o = builder.object
    shapes = [
        o(name="rectangle", type="area", x=64, y=64, width=160, height=96, rotation=15),
        o(name="ellipse", type="area", x=288, y=64, width=160, height=112, ellipse=True),
        o(name="capsule", type="area", x=512, y=80, width=192, height=64, capsule=True),
        o(name="point", type="spawn", x=800, y=112, point=True),
        o(name="polygon", type="area", x=96, y=280, polygon=[{"x": 0, "y": 0}, {"x": 140, "y": -40}, {"x": 190, "y": 60}, {"x": 90, "y": 130}, {"x": -20, "y": 90}], rotation=-10),
        o(name="polyline", type="path", x=360, y=260, polyline=[{"x": 0, "y": 0}, {"x": 80, "y": 90}, {"x": 180, "y": 20}, {"x": 280, "y": 120}]),
        o(name="text", type="label", x=700, y=220, width=200, height=80, text={"text": "Text objects wrap and align", "wrap": True, "halign": "center", "valign": "center", "pixelsize": 22, "color": "#ffffffff"}),
        o(name="chest", type="chest", gid=gid(t.CHEST), x=96, y=520, width=32, height=32),
        o(name="tree", type="tree", gid=PROPS_GID + t.TREE, x=240, y=540, width=32, height=64),
        o(name="big crate", type="crate", gid=gid(t.CRATE), x=300, y=540, width=64, height=64, rotation=10),
    ]
    templates = [
        {"id": builder.next_object, "template": "../templates/lamp.tj", "x": 480, "y": 540},
        {"id": builder.next_object + 1, "template": "../templates/lamp.tj", "x": 560, "y": 540, "properties": [prop("lit", "bool", False)]},
        {"id": builder.next_object + 2, "template": "../templates/sign.tj", "x": 660, "y": 440},
        {"id": builder.next_object + 3, "template": "../templates/sign.tj", "x": 780, "y": 440, "name": "exit sign", "properties": [prop("text", "string", "Exit this way")]},
    ]
    builder.next_object += len(templates)
    builder.add(builder.tiles("ground", meadow(91)), builder.objects("shapes", shapes), builder.objects("templates", templates))
    builder.save(folder / "objects.tmj")


def properties(folder: Path) -> None:
    builder = MapBuilder(30, 18, **{"class": "level"}).tileset(1, TERRAIN)
    treasure = builder.object(name="Treasure", type="chest", gid=gid(t.CHEST), x=640, y=320, width=32, height=32, properties=[
        prop("reward", "class", {"gold": 250, "item": "lantern", "rarity": {"tier": 2, "glow": "#ffffd54f"}}, "Reward"),
        prop("locked", "bool", True),
    ])
    guard = builder.object(name="Guard", type="npc", x=480, y=360, point=True, properties=[
        prop("watches", "object", treasure["id"]),
        prop("mood", "string", "sleepy", "Mood"),
        prop("route", "list", [item("int", 3), item("int", 7), item("int", 12)]),
    ])
    zone = builder.object(name="Haunted zone", type="zone", x=128, y=160, width=256, height=192, properties=[
        prop("fog", "color", "#806a1b9a"),
        prop("difficulty", "int", 2, "Difficulty"),
    ])
    builder.document["properties"] = [
        prop("title", "string", "Every property type"),
        prop("level", "int", 3),
        prop("gravity", "float", 9.81),
        prop("night", "bool", False),
        prop("tint", "color", "#ff4fc3f7"),
        prop("tileset art", "file", "../tiles/terrain.png"),
        prop("boss", "object", guard["id"]),
        prop("stats", "class", {"health": 100, "speed": 2.5, "banner": "#ffe57373", "spawn": {"x": 64, "y": 96}}, "Stats"),
        prop("loot", "list", [item("string", "sword"), item("int", 3), item("float", 0.5), item("bool", True), item("color", "#ffba68c8"), item("list", [item("string", "nested"), item("int", 2)])]),
        prop("weather", "string", "rain", "Weather"),
    ]
    rng = random.Random(95)
    ground = builder.tiles("ground", lambda column, row: gid(t.WATER if 18 <= column <= 22 and 3 <= row <= 7 else (t.FLOWERS if rng.random() < 0.08 else t.GRASS)), properties=[prop("season", "string", "autumn"), prop("walkable", "bool", True)])
    builder.add(ground, builder.objects("things", [treasure, guard, zone], properties=[prop("owner", "string", "the test project")]))
    builder.save(folder / "properties.tmj")


def collision(folder: Path) -> None:
    builder = MapBuilder(30, 18).tileset(1, TERRAIN)

    def walls(column: int, row: int) -> int:
        if column in (0, 29) or row in (0, 17):
            return gid(t.WALL)
        if row == 9 and 3 <= column <= 10:
            return gid(t.HALF_WALL)
        if column == 15 and 3 <= row <= 12:
            return gid(t.WALL)
        if 20 <= column <= 26 and 12 <= row <= 15:
            return gid(t.WATER)
        if (column, row) in {(5, 13), (8, 14), (22, 5), (25, 7)}:
            return gid(t.ROCK)
        if (column, row) in {(4, 5), (11, 4), (19, 9), (27, 3)}:
            return gid(t.BUSH)
        return 0

    o = builder.object
    shapes = [
        o(name="ramp", type="collision", x=512, y=544, polygon=[{"x": 0, "y": 0}, {"x": 128, "y": -96}, {"x": 128, "y": 0}]),
        o(name="boulder", type="collision", x=656, y=96, width=96, height=64, ellipse=True),
        o(name="rail", type="collision", x=736, y=320, polyline=[{"x": 0, "y": 0}, {"x": 64, "y": 40}, {"x": 160, "y": 20}]),
        o(name="pipe", type="collision", x=96, y=96, width=160, height=48, capsule=True),
        o(name="pit", type="collision", x=224, y=448, width=96, height=64, properties=[prop("sensor", "bool", True)]),
    ]
    builder.add(
        builder.tiles("ground", lambda column, row: gid(t.STONE if column % 10 == 0 else t.GRASS)),
        builder.tiles("walls", walls),
        builder.tiles("decor", lambda column, row: gid(t.BUSH) if (column, row) in {(7, 3), (12, 14), (24, 9)} else 0, properties=[prop("collision", "bool", False)]),
        builder.tiles("fences", lambda column, row: gid(t.FENCE) if row == 5 and 18 <= column <= 24 else 0, properties=[prop("category", "int", 2)]),
        builder.objects("shapes", shapes, **{"class": "collision"}),
    )
    builder.save(folder / "collision.tmj")


def spawning(folder: Path) -> None:
    builder = MapBuilder(30, 18).tileset(1, TERRAIN)
    rng = random.Random(97)
    o = builder.object
    entities = [o(name="start", type="player_start", x=96, y=256, point=True)]
    for index in range(10):
        entities.append(o(name=f"coin {index + 1}", type="coin", gid=gid(t.COIN), x=rng.randrange(64, 840), y=rng.randrange(96, 520), width=32, height=32, properties=[prop("value", "int", rng.choice([5, 10, 25]))]))
    for index in range(4):
        entities.append(o(name=f"slime {index + 1}", type="slime", gid=gid(t.SLIME), x=rng.randrange(160, 840), y=rng.randrange(128, 520), width=32, height=32))
    for index, loot in enumerate(["A key", "A map", "Gold"]):
        entities.append(o(name=f"chest {index + 1}", type="chest", gid=gid(t.CHEST), x=200 + index * 260, y=160, width=32, height=32, properties=[prop("loot", "string", loot)]))
    entities.append(o(name="welcome", type="sign", x=160, y=400, width=64, height=32, properties=[prop("text", "string", "Welcome to the spawn test")]))
    entities.append(o(name="note", type="marker", x=600, y=420, point=True))
    zone = builder.group("zone", [builder.objects("entities", entities, visible=False)], offsetx=48, offsety=24)
    builder.add(builder.tiles("ground", meadow(98)), zone)
    builder.save(folder / "spawning.tmj")


def ysort(folder: Path) -> None:
    builder = MapBuilder(36, 22).tileset(1, TERRAIN).tileset(PROPS_GID, PROPS)
    rng = random.Random(99)

    def ground(column: int, row: int) -> int:
        return gid(t.DIRT if row in (10, 11) or column in (17, 18) else (t.FLOWERS if rng.random() < 0.06 else t.GRASS))

    def fences(column: int, row: int) -> int:
        return gid(t.FENCE) if row in (4, 16) and 3 <= column <= 13 else 0

    trees = []
    for _ in range(46):
        column, row = rng.randrange(1, 35), rng.randrange(2, 22)
        if row in (10, 11, 12) or column in (17, 18):
            continue
        kind = rng.choice([t.TREE, t.TREE, t.PINE, t.LAMP])
        trees.append(builder.object(name="tree" if kind != t.LAMP else "lamp", type="tree", gid=PROPS_GID + kind, x=column * 32 + rng.randrange(-8, 8), y=row * 32, width=32, height=64))
    builder.add(builder.tiles("ground", ground), builder.tiles("fences", fences), builder.objects("props", trees))
    builder.save(folder / "ysort.tmj")


def raycasts(folder: Path) -> None:
    builder = MapBuilder(30, 18).tileset(1, TERRAIN)

    def walls(column: int, row: int) -> int:
        if column in (0, 29) or row in (0, 17):
            return gid(t.WALL)
        if 8 <= column <= 12 and 4 <= row <= 7:
            return gid(t.WATER)
        if column == 18 and 3 <= row <= 9:
            return gid(t.WALL)
        if row == 12 and 4 <= column <= 13:
            return gid(t.FENCE)
        if (column, row) in {(22, 13), (23, 13), (22, 14), (5, 3), (25, 4)}:
            return gid(t.CRATE)
        return 0

    o = builder.object
    obstacles = [
        o(name="pillar", type="stone", x=640, y=384, width=64, height=64),
        o(name="pond", type="water", x=736, y=160, width=96, height=64, ellipse=True),
        o(name="spire", type="stone", x=448, y=520, polygon=[{"x": 0, "y": 0}, {"x": 40, "y": -80}, {"x": 80, "y": 0}]),
        o(name="rope", type="rope", x=96, y=448, polyline=[{"x": 0, "y": 0}, {"x": 90, "y": 40}, {"x": 180, "y": 0}]),
        o(name="log", type="wood", x=256, y=256, width=112, height=32, capsule=True),
        o(name="crate", type="crate", gid=gid(t.CRATE), x=800, y=480, width=32, height=32),
    ]
    builder.add(builder.tiles("ground", meadow(101)), builder.tiles("walls", walls), builder.objects("obstacles", obstacles))
    builder.save(folder / "raycasts.tmj")
