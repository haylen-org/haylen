"""The tilesets and object templates of the Tiled tests, with tile classes, properties, animations and collision shapes."""

from __future__ import annotations

from tiled_json import animation, collision, item, prop, tileset

# Local tile ids of the terrain tileset, in the order of the tiles of `tiles/terrain.png`.
GRASS, FLOWERS, DIRT, SAND, STONE, WALL, HALF_WALL, FENCE = range(8)
WATER, LAVA = 8, 12
TORCH, BUSH, ROCK, CRATE, CHEST, SPIKES, FLAG = 16, 18, 19, 20, 21, 22, 23
COIN, SLIME, SIGN, KEY = 24, 28, 30, 31

# Local tile ids of the props tileset.
TREE, PINE, LAMP = 0, 1, 2

# The tiles are painted at four times the size of the grid of the maps, which they fill because the tilesets draw them at the size of the grid. Each tile sits in a gutter of its own edge pixels, so the linear filter never reads its neighbours.
SCALE = 4
MARGIN, SPACING = 2, 4
GRID = {"margin": MARGIN, "spacing": SPACING, "tilerendersize": "grid"}


def atlas_size(columns: int, rows: int, tile_width: int, tile_height: int) -> tuple[int, int]:
    return 2 * MARGIN + columns * tile_width + (columns - 1) * SPACING, 2 * MARGIN + rows * tile_height + (rows - 1) * SPACING


def shape(**fields) -> dict:
    """A collision shape given in the pixels of the grid, scaled to the pixels of the painted tile."""
    scaled = {key: value * SCALE if key in ("x", "y", "width", "height") else value for key, value in fields.items()}
    if "polygon" in fields:
        scaled["polygon"] = [{"x": point["x"] * SCALE, "y": point["y"] * SCALE} for point in fields["polygon"]]
    return collision(scaled)


FULL = {"width": 32, "height": 32}


def terrain() -> dict:
    tiles = [
        {"id": FLOWERS, "probability": 0.3},
        {"id": WALL, "type": "wall", "properties": [prop("solid", "bool", True)], "objectgroup": shape(**FULL)},
        {"id": HALF_WALL, "type": "wall", "objectgroup": shape(y=16, width=32, height=16)},
        {"id": FENCE, "type": "fence", "objectgroup": shape(y=9, width=32, height=14)},
        {"id": WATER, "type": "water", "animation": animation(WATER, 4, 180), "properties": [prop("liquid", "bool", True), prop("depth", "float", 1.5)], "objectgroup": shape(**FULL, properties=[prop("sensor", "bool", True)])},
        {"id": LAVA, "type": "lava", "animation": animation(LAVA, 4, 220), "properties": [prop("damage", "int", 5), prop("glow", "color", "#ffff7043")]},
        {"id": TORCH, "type": "wall", "animation": animation(TORCH, 2, 150), "objectgroup": shape(**FULL)},
        {"id": BUSH, "type": "bush", "objectgroup": shape(x=4, y=5, width=24, height=24, ellipse=True)},
        {"id": ROCK, "type": "rock", "objectgroup": shape(x=4, y=26, polygon=[{"x": 0, "y": 0}, {"x": 4, "y": -16}, {"x": 14, "y": -21}, {"x": 24, "y": -14}, {"x": 25, "y": 1}])},
        {"id": CRATE, "type": "crate", "properties": [prop("breakable", "bool", True)], "objectgroup": shape(x=2, y=2, width=28, height=28)},
        {"id": CHEST, "type": "chest", "properties": [prop("loot", "list", [item("string", "gold"), item("int", 25), item("list", [item("string", "potion"), item("color", "#ff66bb6a")])])]},
        {"id": COIN, "type": "coin", "animation": animation(COIN, 4, 120), "properties": [prop("value", "int", 10)]},
        {"id": SLIME, "type": "slime", "animation": animation(SLIME, 2, 400), "properties": [prop("health", "int", 3)]},
    ]
    width, height = atlas_size(8, 4, 32 * SCALE, 32 * SCALE)
    return tileset("terrain", "../tiles/terrain.png", width, height, 32 * SCALE, 32 * SCALE, tiles, **GRID, properties=[prop("style", "string", "painted in high definition")])


def props() -> dict:
    """Tall props that maps place as tile objects, which draw at the size of their objects."""
    tiles = [
        {"id": TREE, "type": "tree", "objectgroup": shape(x=11, y=50, width=10, height=12)},
        {"id": PINE, "type": "tree", "objectgroup": shape(x=12, y=52, width=8, height=10)},
        {"id": LAMP, "type": "lamp", "properties": [prop("light", "color", "#fffff59d"), prop("radius", "float", 96)], "objectgroup": shape(x=13, y=56, width=6, height=6)},
    ]
    width, height = atlas_size(3, 1, 32 * SCALE, 64 * SCALE)
    return tileset("props", "../tiles/props.png", width, height, 32 * SCALE, 64 * SCALE, tiles, margin=MARGIN, spacing=SPACING, objectalignment="bottom")


def diamonds() -> dict:
    width, height = atlas_size(4, 1, 64 * SCALE, 32 * SCALE)
    return tileset("diamonds", "../tiles/diamonds.png", width, height, 64 * SCALE, 32 * SCALE, **GRID)


def hexagons() -> dict:
    width, height = atlas_size(4, 1, 56 * SCALE, 64 * SCALE)
    return tileset("hexagons", "../tiles/hexagons.png", width, height, 56 * SCALE, 64 * SCALE, **GRID)
def lamp_template() -> dict:
    """A lamp post tile object with its light, which instances place and may override."""
    return {
        "type": "template",
        "tileset": {"firstgid": 1, "source": "../tilesets/props.tsj"},
        "object": {"gid": LAMP + 1, "width": 32, "height": 64, "name": "lamp", "type": "lamp", "rotation": 0, "visible": True, "properties": [prop("lit", "bool", True), prop("radius", "float", 96)]},
    }


def sign_template() -> dict:
    """A rectangle that marks a sign with the text it shows."""
    return {
        "type": "template",
        "object": {"width": 64, "height": 32, "name": "sign", "type": "sign", "rotation": 0, "visible": True, "properties": [prop("text", "string", "Welcome"), prop("color", "color", "#ffffd54f")]},
    }
