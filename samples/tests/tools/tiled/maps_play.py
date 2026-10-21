"""The maps of the Tiled tests that games read: objects and templates, properties, collision, spawning, y sorting, ray casts and the garden that the automapping rules finish."""

from __future__ import annotations

import random
from pathlib import Path

import tilesets as t
import transitions as tr
from nature import PROPS, PROPS_GID, SCENERY, SCENERY_GID, TERRAIN, TRANSITIONS, TRANSITIONS_GID, Ground, Props, gid, scatter
from tiled_json import MapBuilder, item, prop


def objects(folder: Path) -> None:
    """A meadow with every object shape of Tiled marking places of it: a fenced plot, a pond, a log bridge, a spawn point, a flower bed, a trail and a label, with tile objects and the lamps and signs of the templates."""
    width, height = 24, 12
    builder = MapBuilder(width, height).tileset(1, TERRAIN).tileset(PROPS_GID, PROPS).tileset(TRANSITIONS_GID, TRANSITIONS).tileset(SCENERY_GID, SCENERY)
    ground = Ground(width, height, 91)
    ground.blob("water", 9.5, 3.2, 2.6, 1.8)
    ground.road("path", [(0, 8.5), (8, 8), (14, 9.5), (24, 9)], 0.8)
    o = builder.object
    shapes = [
        o(name="plot", type="area", x=40, y=40, width=160, height=96, rotation=8),
        o(name="pond", type="area", x=232, y=40, width=148, height=112, ellipse=True),
        o(name="bridge", type="area", x=440, y=72, width=176, height=56, capsule=True),
        o(name="spawn", type="spawn", x=684, y=96, point=True),
        o(name="flower bed", type="area", x=72, y=200, polygon=[{"x": 0, "y": 0}, {"x": 104, "y": -28}, {"x": 136, "y": 40}, {"x": 64, "y": 92}, {"x": -16, "y": 64}], rotation=-6),
        o(name="trail", type="path", x=248, y=216, polyline=[{"x": 0, "y": 0}, {"x": 56, "y": 48}, {"x": 128, "y": 8}, {"x": 200, "y": 56}]),
        o(name="label", type="label", x=504, y=168, width=200, height=64, text={"text": "Text objects wrap and align", "wrap": True, "halign": "center", "valign": "center", "pixelsize": 20, "color": "#ff1e1b33", "bold": True}),
    ]
    props = Props(builder)
    props.at("chest", 3.2 * 32, 11.4 * 32, "chest", type="chest")
    props.at("tree", 6.2 * 32, 11.6 * 32, "tree", type="tree")
    props.at("crate", 8.6 * 32, 11.4 * 32, "big crate", type="crate", scale=1.3, rotation=8)
    props.at("log", 16.6 * 32, 3.95 * 32, "log", type="log")
    templates = [
        {"id": builder.next_object, "template": "../templates/lamp.tj", "x": 352, "y": 376},
        {"id": builder.next_object + 1, "template": "../templates/lamp.tj", "x": 432, "y": 376, "properties": [prop("lit", "bool", False)]},
        {"id": builder.next_object + 2, "template": "../templates/sign.tj", "x": 560, "y": 376},
        {"id": builder.next_object + 3, "template": "../templates/sign.tj", "x": 664, "y": 376, "name": "exit sign", "properties": [prop("text", "string", "Exit this way")]},
    ]
    builder.next_object += len(templates)
    builder.add(ground.meadow(builder, 92), ground.layer(builder, "path", "path"), ground.layer(builder, "water", "water"), builder.objects("shapes", shapes), builder.objects("props", props.objects), builder.objects("templates", templates))
    builder.save(folder / "objects.tmj")


def properties(folder: Path) -> None:
    """A clearing whose map, layers, objects and tiles hold custom properties of every type: a treasure, its guard and a haunted corner of the woods."""
    width, height = 24, 12
    builder = MapBuilder(width, height, **{"class": "level"}).tileset(1, TERRAIN).tileset(PROPS_GID, PROPS).tileset(TRANSITIONS_GID, TRANSITIONS).tileset(SCENERY_GID, SCENERY)
    ground = Ground(width, height, 95)
    ground.blob("water", 18.5, 4.5, 3.2, 2.4)
    treasure = builder.object(name="Treasure", type="chest", gid=gid(t.CHEST), x=448, y=304, width=48, height=48, properties=[
        prop("reward", "class", {"gold": 250, "item": "lantern", "rarity": {"tier": 2, "glow": "#ffffd54f"}}, "Reward"),
        prop("locked", "bool", True),
    ])
    guard = builder.object(name="Guard", type="npc", x=352, y=320, point=True, properties=[
        prop("watches", "object", treasure["id"]),
        prop("mood", "string", "sleepy", "Mood"),
        prop("route", "list", [item("int", 3), item("int", 7), item("int", 12)]),
    ])
    zone = builder.object(name="Haunted zone", type="zone", x=48, y=48, width=224, height=160, properties=[
        prop("fog", "color", "#a0a070e0"),
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
    rng = random.Random(96)
    trees = Props(builder)
    for cell in ((1, 4), (3, 7), (6, 6), (7, 4), (2, 10), (6, 10), (22, 10), (12, 4)):
        trees.on(rng.choice(("pine", "pine", "tree")), cell, rng, scale=0.9)
    for cell in ((4, 4), (8, 8), (17, 9)):
        trees.on(rng.choice(("mushrooms", "stump", "bush")), cell, rng)
    meadow = ground.meadow(builder, 97, properties=[prop("season", "string", "autumn"), prop("walkable", "bool", True)])
    builder.add(meadow, ground.layer(builder, "water", "water"), builder.objects("woods", trees.objects), builder.objects("things", [treasure, guard, zone], properties=[prop("owner", "string", "the test project")]))
    builder.save(folder / "properties.tmj")


def collision(folder: Path) -> None:
    """A walled yard whose walls, rocks, bushes and fences collide through the shapes of their tiles, with a pond and a lava pit as sensors and a log, a rock pile and a rope as collision objects."""
    width, height = 24, 13
    builder = MapBuilder(width, height).tileset(1, TERRAIN).tileset(TRANSITIONS_GID, TRANSITIONS).tileset(SCENERY_GID, SCENERY)
    ground = Ground(width, height, 98)
    ground.blob("water", 18, 9.5, 3.2, 2.2)
    ground.blob("lava", 7.6, 10.4, 1.8, 1.3)

    def walls(column: int, row: int) -> int:
        if column in (0, width - 1) or row in (0, height - 1):
            return gid(t.WALL)
        if row == 6 and 2 <= column <= 8:
            return gid(t.HALF_WALL)
        if column == 12 and 2 <= row <= 8:
            return gid(t.WALL)
        if (column, row) in {(4, 3), (9, 9), (20, 3), (16, 5)}:
            return gid(t.ROCK)
        if (column, row) in {(3, 2), (9, 3), (11, 10), (21, 6)}:
            return gid(t.BUSH)
        return 0

    o = builder.object
    shapes = [
        o(name="rock pile", type="collision", x=400, y=352, polygon=[{"x": 0, "y": 0}, {"x": 20, "y": -44}, {"x": 64, "y": -56}, {"x": 96, "y": -20}, {"x": 88, "y": 0}]),
        o(name="pond", type="collision", x=488, y=256, width=184, height=128, ellipse=True, properties=[prop("sensor", "bool", True)]),
        o(name="rope", type="collision", x=416, y=64, polyline=[{"x": 0, "y": 0}, {"x": 56, "y": 24}, {"x": 120, "y": 10}, {"x": 176, "y": 34}]),
        o(name="log", type="collision", x=48, y=264, width=128, height=40, capsule=True),
        o(name="pit", type="collision", x=190, y=298, width=108, height=74, ellipse=True, properties=[prop("sensor", "bool", True)]),
    ]
    rng = random.Random(99)
    art = Props(builder)
    for x, y in ((428, 352), (464, 336), (454, 312)):
        art.at("rock", x, y)
    art.at("log", 112, 304, scale=1.8)
    builder.add(
        ground.meadow(builder, 99),
        ground.layer(builder, "water", "water", properties=[prop("collision", "bool", False)]),
        ground.layer(builder, "lava", "lava", properties=[prop("collision", "bool", False)]),
        builder.tiles("walls", walls),
        builder.tiles("decor", lambda column, row: gid(t.FLOWERS) if (column, row) in {(7, 3), (15, 11), (20, 8)} else 0, properties=[prop("collision", "bool", False)]),
        builder.tiles("fences", lambda column, row: gid(t.FENCE) if row == 4 and 14 <= column <= 20 else 0, properties=[prop("category", "int", 2)]),
        builder.objects("art", art.objects),
        builder.objects("shapes", shapes, **{"class": "collision"}),
    )
    builder.save(folder / "collision.tmj")


def spawning(folder: Path) -> None:
    """A meadow in a group that moves its objects, whose hidden layer of objects becomes coins, slimes, chests, a sign and the start of the player through factories by class."""
    width, height = 22, 11
    builder = MapBuilder(width, height).tileset(1, TERRAIN).tileset(PROPS_GID, PROPS).tileset(TRANSITIONS_GID, TRANSITIONS).tileset(SCENERY_GID, SCENERY)
    ground = Ground(width, height, 100)
    ground.road("path", [(0, 5.5), (7, 5), (14, 6), (22, 5.5)], 0.8)
    rng = random.Random(97)
    o = builder.object
    entities = [o(name="start", type="player_start", x=48, y=144, point=True)]
    coins = [(9, 2), (11, 3), (13, 2), (16, 3), (15, 8), (17, 9), (11, 8), (19, 2), (6, 9)]
    for index, (column, row) in enumerate(coins):
        entities.append(o(name=f"coin {index + 1}", type="coin", gid=gid(t.COIN), x=column * 32, y=row * 32 + 28, width=40, height=40, properties=[prop("value", "int", rng.choice([5, 10, 25]))]))
    for index, (column, row) in enumerate(((7, 7), (12, 4), (16, 6), (4, 9))):
        entities.append(o(name=f"slime {index + 1}", type="slime", gid=gid(t.SLIME), x=column * 32, y=row * 32 + 28, width=48, height=48))
    for index, (loot, column) in enumerate((("A key", 2), ("A map", 9), ("Gold", 18))):
        entities.append(o(name=f"chest {index + 1}", type="chest", gid=gid(t.CHEST), x=column * 32, y=10 * 32 - 2, width=48, height=48, properties=[prop("loot", "string", loot)]))
    entities.append(o(name="welcome", type="sign", gid=gid(t.SIGN), x=4 * 32, y=5 * 32 - 2, width=48, height=48, properties=[prop("text", "string", "Welcome to the meadow")]))
    entities.append(o(name="note", type="marker", x=600, y=60, point=True))
    zone = builder.group("zone", [builder.objects("entities", entities, visible=False)], offsetx=24, offsety=8)
    props = Props(builder)
    for cell in ((1, 3), (20, 3), (20, 9), (13, 10)):
        props.on(rng.choice(("tree", "pine")), cell, rng, scale=0.85)
    for cell in ((3, 7), (14, 3), (18, 6)):
        props.on(rng.choice(("bush", "blossoms", "rock")), cell, rng)
    builder.add(ground.meadow(builder, 98), ground.layer(builder, "path", "path"), builder.objects("props", props.objects), zone)
    builder.save(folder / "spawning.tmj")


def ysort(folder: Path) -> None:
    """A village green crossed by two roads, with woods, lamps and fences to walk behind and in front of."""
    width, height = 36, 22
    builder = MapBuilder(width, height).tileset(1, TERRAIN).tileset(PROPS_GID, PROPS).tileset(TRANSITIONS_GID, TRANSITIONS)
    rng = random.Random(99)

    # The roads cross on the corners of the cells from edge to edge, so the transitions wear their edges into the grass.
    def worn(column: int, line: int) -> int:
        return int(line in (10, 11, 12) or column in (17, 18, 19))

    def ground(column: int, row: int) -> int:
        return gid(t.FLOWERS if rng.random() < 0.06 else t.GRASS)

    def fences(column: int, row: int) -> int:
        return gid(t.FENCE) if row in (4, 16) and 3 <= column <= 13 else 0

    trees = []
    # A tree stands just above where the hero starts, so the hero starts half behind it.
    trees.append(builder.object(name="tree", type="tree", gid=PROPS_GID + t.TREE, x=18 * 32 + 26, y=11 * 32 - 2, width=48, height=96))
    for _ in range(46):
        column, row = rng.randrange(1, 35), rng.randrange(2, 22)
        if row in (9, 10, 11, 12, 13) or column in (16, 17, 18, 19):
            continue
        kind = rng.choice([t.TREE, t.TREE, t.PINE, t.LAMP])
        size = (32, 64) if kind == t.LAMP else (48, 96)
        trees.append(builder.object(name="tree" if kind != t.LAMP else "lamp", type="tree", gid=PROPS_GID + kind, x=column * 32 + rng.randrange(-8, 8), y=row * 32, width=size[0], height=size[1]))
    builder.add(builder.tiles("ground", ground), builder.tiles("paths", lambda column, line: 0 if tr.corner(tr.PATH, worn, column, line) is None else TRANSITIONS_GID + tr.corner(tr.PATH, worn, column, line)), builder.tiles("fences", fences), builder.objects("props", trees))
    builder.save(folder / "ysort.tmj")


def raycasts(folder: Path) -> None:
    """A walled garden whose wall cells, crates, fence and pond stop rays through the walls layer, and whose stone pillar, pine, rope, log and crate stop rays as objects."""
    width, height = 24, 12
    builder = MapBuilder(width, height).tileset(1, TERRAIN).tileset(PROPS_GID, PROPS).tileset(TRANSITIONS_GID, TRANSITIONS).tileset(SCENERY_GID, SCENERY)
    ground = Ground(width, height, 101)
    ground.blob("water", 8, 3.6, 2.6, 1.8)

    def walls(column: int, row: int) -> int:
        if column in (0, width - 1) or row in (0, height - 1):
            return gid(t.WALL)
        water = ground.cells("water")(column, row)
        if water:
            return water
        if column == 15 and 2 <= row <= 6:
            return gid(t.WALL)
        if row == 8 and 3 <= column <= 10:
            return gid(t.FENCE)
        if (column, row) in {(19, 8), (20, 8), (19, 9), (4, 2), (20, 2)}:
            return gid(t.CRATE)
        return 0

    o = builder.object
    obstacles = [
        o(name="pillar", type="stone", gid=SCENERY_GID + t.SHRINE, x=556, y=328, width=72, height=90),
        o(name="pine", type="wood", gid=PROPS_GID + t.PINE, x=404, y=344, width=56, height=112),
        o(name="rope", type="rope", x=72, y=328, polyline=[{"x": 0, "y": 0}, {"x": 72, "y": 28}, {"x": 144, "y": 0}]),
        o(name="log", type="wood", gid=SCENERY_GID + t.LOG, x=224, y=232, width=96, height=62),
        o(name="crate", type="crate", gid=gid(t.CRATE), x=640, y=168, width=40, height=40),
    ]
    builder.add(ground.meadow(builder, 102), builder.tiles("walls", walls), builder.objects("obstacles", obstacles))
    builder.save(folder / "raycasts.tmj")


def garden(folder: Path) -> None:
    """The garden that the automapping test finishes: ponds, sand and a winding path painted with plain tiles only, and the rule map that turns them into the corner sets of the transitions and scatters bushes, rocks, flowers and slimes."""
    width, height = 26, 13
    builder = MapBuilder(width, height).tileset(1, TERRAIN)
    plain = Ground(width, height, 103)
    plain.blob("water", 6, 4, 3.4, 2.4).blob("water", 19, 8.5, 4.2, 2.8).blob("water", 21.5, 2.5, 2.2, 1.6)
    plain.road("path", [(0, 9.5), (6, 9), (11, 6.5), (15, 5), (26, 5.5)], 0.6)

    def cell(column: int, row: int) -> int:
        if plain.holds("water", column, row):
            return gid(t.WATER)
        if plain.holds("path", column, row):
            return gid(t.DIRT)
        return gid(t.GRASS)

    builder.add(builder.tiles("ground", cell), builder.objects("actors", []))
    builder.save(folder / "maps" / "garden.tmj")
    rules(folder / "rules" / "garden.tmj")


def rules(path: Path) -> None:
    """The rule map of the garden. Each corner set has a rule for every mask of corners, whose input is the plain tile on the four cells of the corners that hold it and any other tile on the rest, and whose output is the tile of that mask on the cell of the top-left corner, so the shapes of the plain tiles become the corners of the transitions half a cell away. The plain tiles under them become grass, and open grass gets a bush, a rock or flowers now and then, and the path a slime."""
    sets = [("water", gid(t.WATER), tr.WATER), ("path", gid(t.DIRT), tr.PATH)]
    columns = 16
    rows_needed = len(sets) * 2 + 2
    width, height = columns * 3, rows_needed * 3
    builder = MapBuilder(width, height).tileset(1, "../tilesets/terrain.tsj").tileset(TRANSITIONS_GID, "../tilesets/transitions.tsj")
    inputs: dict[str, dict[tuple[int, int], int]] = {}
    inputnots: dict[str, dict[tuple[int, int], int]] = {}
    outputs: dict[str, dict[tuple[int, int], int]] = {}

    # The corner rules, one row of fifteen for each set.
    for index, (name, tile, row) in enumerate(sets):
        for mask in range(1, 16):
            x, y = (mask - 1) * 3, index * 3
            for bit, (dx, dy) in enumerate(((0, 0), (1, 0), (1, 1), (0, 1))):
                (inputs if mask >> bit & 1 else inputnots).setdefault(name, {})[(x + dx, y + dy)] = tile
            outputs.setdefault(name, {})[(x, y)] = TRANSITIONS_GID + tr.tile_id(row, mask)

    # The plain water and path become grass under the transitions.
    base = len(sets) * 3
    for index, (_, tile, _) in enumerate(sets):
        inputs.setdefault("plain", {})[(index * 3, base)] = tile
        outputs.setdefault("ground", {})[(index * 3, base)] = gid(t.GRASS)

    # Open grass, three by three cells, gets a decoration in its middle now and then.
    decor_y = base + 3
    for dx in range(3):
        for dy in range(3):
            inputs.setdefault("open", {})[(dx, decor_y + dy)] = gid(t.GRASS)

    def grid(cells: dict[tuple[int, int], int]):
        return lambda column, line: cells.get((column, line), 0)

    layers = []
    for name, _, _ in sets:
        layers.append(builder.tiles("input_ground", grid(inputs[name])))
        layers.append(builder.tiles("inputnot_ground", grid(inputnots[name])))
        layers.append(builder.tiles(f"output_{name}", grid(outputs[name])))
    layers.append(builder.tiles("input_ground", grid(inputs["plain"])))
    layers.append(builder.tiles("output_ground", grid(outputs["ground"])))
    layers.append(builder.tiles("input_ground", grid(inputs["open"])))
    middle = (1, decor_y + 1)
    for index, (decoration, chance) in enumerate(((t.BUSH, 1.0), (t.ROCK, 0.6), (t.FLOWERS, 1.4))):
        layer_name = f"output{index + 1}_decor" if decoration != t.FLOWERS else f"output{index + 1}_ground"
        layers.append(builder.tiles(layer_name, grid({middle: gid(decoration)}), properties=[prop("Probability", "float", chance)]))
    path_cell = (6, decor_y + 1)
    layers.append(builder.tiles("input_ground", grid({path_cell: gid(t.DIRT)})))
    # The slime stands on the middle of its cell, and its origin, at its bottom-left corner, lies in the cell to the left, which joins the rule.
    layers.append(builder.objects("output_actors", [builder.object(name="slime", gid=gid(t.SLIME), x=path_cell[0] * 32 - 8, y=path_cell[1] * 32 + 28, width=48, height=48)]))
    options = [
        builder.object(name="sparse decoration", x=0, y=decor_y * 32, width=96, height=96, properties=[prop("Probability", "float", 0.3), prop("NoOverlappingOutput", "bool", True)]),
        builder.object(name="rare slimes", x=(path_cell[0] - 1) * 32, y=(path_cell[1] - 1) * 32, width=96, height=96, properties=[prop("Probability", "float", 0.05), prop("NoOverlappingOutput", "bool", True)]),
    ]
    layers.append(builder.objects("rule_options", options))
    builder.add(*layers)
    builder.save(path)
