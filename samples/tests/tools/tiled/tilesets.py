"""The tilesets and object templates of the Tiled tests, with tile classes, properties, animations and collision shapes."""

from __future__ import annotations

from tiled_json import FORMAT_VERSION, TILED_VERSION, animation, collision, item, prop, tileset

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


# The props of the island of the Camera tests, as their areas of `content/camera/props.png` in the order of the tiles of the scenery tileset.
SCENERY_IMAGE = "../../camera/props.png"
SCENERY_SIZE = (1024, 1052)
SCENERY = (
    ("shrine", (6, 6, 233, 292)),
    ("cottage", (251, 6, 280, 279)),
    ("well", (543, 6, 216, 272)),
    ("signpost", (771, 6, 205, 256)),
    ("stump", (6, 310, 313, 224)),
    ("log", (331, 310, 320, 208)),
    ("mushrooms", (663, 310, 252, 218)),
    ("flowers", (6, 546, 267, 246)),
    ("reeds", (285, 546, 226, 257)),
    ("lilies", (523, 546, 274, 167)),
    ("barrel", (809, 546, 182, 225)),
    ("campfire", (6, 815, 233, 231)),
)
SHRINE, COTTAGE, WELL, SIGNPOST, STUMP, LOG, MUSHROOMS, BLOSSOMS, REEDS, LILIES, BARREL, CAMPFIRE = range(12)


# The roofs of the group layers test seen from straight above, as their areas of `tiles/roofs.png`, which `pack-roofs.py` packs.
ROOFS_IMAGE = "../tiles/roofs.png"
ROOFS_SIZE = (757, 196)
ROOFS = (
    ("clay", (6, 6, 238, 183)),
    ("slate", (256, 6, 234, 183)),
    ("thatch", (502, 6, 243, 184)),
)
CLAY, SLATE, THATCH = range(3)


def collection(name: str, image: str, size: tuple[int, int], areas: tuple, alignment: str) -> dict:
    """An image collection whose tiles are areas of one image, which maps place as tile objects of any size."""
    tiles = [{"id": index, "type": kind, "image": image, "imagewidth": size[0], "imageheight": size[1], "x": x, "y": y, "width": width, "height": height} for index, (kind, (x, y, width, height)) in enumerate(areas)]
    return {
        "columns": 0,
        "grid": {"height": 1, "orientation": "orthogonal", "width": 1},
        "margin": 0,
        "name": name,
        "objectalignment": alignment,
        "spacing": 0,
        "tilecount": len(tiles),
        "tiledversion": TILED_VERSION,
        "tileheight": max(area[3] for _, area in areas),
        "tiles": tiles,
        "tilewidth": max(area[2] for _, area in areas),
        "type": "tileset",
        "version": FORMAT_VERSION,
    }


def scenery() -> dict:
    """The painted props of the Camera tests, standing on the middle of their bottom edge."""
    return collection("scenery", SCENERY_IMAGE, SCENERY_SIZE, SCENERY, "bottom")


def roofs() -> dict:
    """The roofs of the town of the group layers test, placed by their top-left corner like the walls under them."""
    return collection("roofs", ROOFS_IMAGE, ROOFS_SIZE, ROOFS, "topleft")


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
    """A wooden sign tile object with the text it shows, which instances place and may override."""
    return {
        "type": "template",
        "tileset": {"firstgid": 1, "source": "../tilesets/terrain.tsj"},
        "object": {"gid": SIGN + 1, "width": 56, "height": 56, "name": "sign", "type": "sign", "rotation": 0, "visible": True, "properties": [prop("text", "string", "Welcome"), prop("color", "color", "#ffffd54f")]},
    }
