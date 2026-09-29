"""The tilesets and object templates of the Tiled sample, with tile classes, properties, animations and collision shapes."""

from __future__ import annotations

from tiled_json import animation, collision, item, prop, tileset

# Local tile ids of the terrain tileset, in the order art.terrain draws them.
GRASS, FLOWERS, DIRT, SAND, STONE, WALL, HALF_WALL, FENCE = range(8)
WATER, LAVA = 8, 12
TORCH, BUSH, ROCK, CRATE, CHEST, SPIKES, FLAG = 16, 18, 19, 20, 21, 22, 23
COIN, SLIME, SIGN, KEY = 24, 28, 30, 31

# Local tile ids of the props tileset.
TREE, PINE, LAMP = 0, 1, 2

FULL = {"width": 32, "height": 32}


def terrain() -> dict:
    tiles = [
        {"id": FLOWERS, "probability": 0.3},
        {"id": WALL, "type": "wall", "properties": [prop("solid", "bool", True)], "objectgroup": collision(FULL)},
        {"id": HALF_WALL, "type": "wall", "objectgroup": collision({"y": 16, "width": 32, "height": 16})},
        {"id": FENCE, "type": "fence", "objectgroup": collision({"y": 9, "width": 32, "height": 14})},
        {"id": WATER, "type": "water", "animation": animation(WATER, 4, 180), "properties": [prop("liquid", "bool", True), prop("depth", "float", 1.5)], "objectgroup": collision(dict(FULL, properties=[prop("sensor", "bool", True)]))},
        {"id": LAVA, "type": "lava", "animation": animation(LAVA, 4, 220), "properties": [prop("damage", "int", 5), prop("glow", "color", "#ffff7043")]},
        {"id": TORCH, "type": "wall", "animation": animation(TORCH, 2, 150), "objectgroup": collision(FULL)},
        {"id": BUSH, "type": "bush", "objectgroup": collision({"x": 4, "y": 5, "width": 24, "height": 24, "ellipse": True})},
        {"id": ROCK, "type": "rock", "objectgroup": collision({"x": 4, "y": 26, "polygon": [{"x": 0, "y": 0}, {"x": 4, "y": -16}, {"x": 14, "y": -21}, {"x": 24, "y": -14}, {"x": 25, "y": 1}]})},
        {"id": CRATE, "type": "crate", "properties": [prop("breakable", "bool", True)], "objectgroup": collision({"x": 2, "y": 2, "width": 28, "height": 28})},
        {"id": CHEST, "type": "chest", "properties": [prop("loot", "list", [item("string", "gold"), item("int", 25), item("list", [item("string", "potion"), item("color", "#ff66bb6a")])])]},
        {"id": COIN, "type": "coin", "animation": animation(COIN, 4, 120), "properties": [prop("value", "int", 10)]},
        {"id": SLIME, "type": "slime", "animation": animation(SLIME, 2, 400), "properties": [prop("health", "int", 3)]},
    ]
    return tileset("terrain", "../tiles/terrain.png", 256, 128, 32, 32, tiles, properties=[prop("style", "string", "generated pixel art")])


def props() -> dict:
    tiles = [
        {"id": TREE, "type": "tree", "objectgroup": collision({"x": 11, "y": 50, "width": 10, "height": 12})},
        {"id": PINE, "type": "tree", "objectgroup": collision({"x": 12, "y": 52, "width": 8, "height": 10})},
        {"id": LAMP, "type": "lamp", "properties": [prop("light", "color", "#fffff59d"), prop("radius", "float", 96)], "objectgroup": collision({"x": 13, "y": 56, "width": 6, "height": 6})},
    ]
    return tileset("props", "../tiles/props.png", 96, 64, 32, 64, tiles, objectalignment="bottom")


def diamonds() -> dict:
    return tileset("diamonds", "../tiles/diamonds.png", 256, 32, 64, 32)


def hexagons() -> dict:
    return tileset("hexagons", "../tiles/hexagons.png", 224, 64, 56, 64)


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
