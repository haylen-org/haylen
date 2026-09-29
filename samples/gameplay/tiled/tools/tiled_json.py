"""Builds documents in the JSON format of Tiled 1.12: maps with their layers and objects, tilesets, templates and worlds."""

from __future__ import annotations

import json
import math
import random
from pathlib import Path
from typing import Callable

TILED_VERSION = "1.12.0"
FORMAT_VERSION = "1.12"


def prop(name: str, kind: str, value, property_type: str | None = None) -> dict:
    result = {"name": name, "type": kind, "value": value}
    if property_type:
        result["propertytype"] = property_type
    return result


def item(kind: str, value) -> dict:
    """An item of a list property, which carries its own type."""
    return {"type": kind, "value": value}


def save(document: dict, path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(document, indent=1) + "\n")


def noise(width: int, height: int, scale: float, seed: int) -> Callable[[int, int], float]:
    """Smooth value noise from 0 to 1, so the maps look natural and stay the same on every run."""
    rng = random.Random(seed)
    columns, rows = math.ceil(width / scale) + 2, math.ceil(height / scale) + 2
    lattice = [[rng.random() for _ in range(columns)] for _ in range(rows)]

    def smooth(t: float) -> float:
        return t * t * (3 - 2 * t)

    def sample(x: int, y: int) -> float:
        fx, fy = x / scale, y / scale
        ix, iy = int(fx), int(fy)
        tx, ty = smooth(fx - ix), smooth(fy - iy)
        top = lattice[iy][ix] * (1 - tx) + lattice[iy][ix + 1] * tx
        bottom = lattice[iy + 1][ix] * (1 - tx) + lattice[iy + 1][ix + 1] * tx
        return top * (1 - ty) + bottom * ty

    return sample


class MapBuilder:
    """Collects the layers and objects of one map and numbers them the way Tiled does."""

    def __init__(self, width: int, height: int, tile_width: int = 32, tile_height: int = 32, orientation: str = "orthogonal", **extra) -> None:
        self.width, self.height = width, height
        self.document = {
            "compressionlevel": -1,
            "height": height,
            "infinite": False,
            "layers": [],
            "orientation": orientation,
            "renderorder": "right-down",
            "tiledversion": TILED_VERSION,
            "tileheight": tile_height,
            "tilesets": [],
            "tilewidth": tile_width,
            "type": "map",
            "version": FORMAT_VERSION,
            "width": width,
        }
        self.document.update(extra)
        self.next_layer = 1
        self.next_object = 1

    def tileset(self, first_gid: int, source: str) -> MapBuilder:
        self.document["tilesets"].append({"firstgid": first_gid, "source": source})
        return self

    def layer(self, kind: str, name: str, **extra) -> dict:
        layer = {"id": self.next_layer, "name": name, "type": kind, "opacity": 1, "visible": True, "x": 0, "y": 0}
        layer.update(extra)
        self.next_layer += 1
        return layer

    def tiles(self, name: str, cell: Callable[[int, int], int], **extra) -> dict:
        data = [cell(column, row) for row in range(self.height) for column in range(self.width)]
        return self.layer("tilelayer", name, width=self.width, height=self.height, data=data, **extra)

    def chunked(self, name: str, chunks: list[tuple[int, int]], size: int, cell: Callable[[int, int], int], **extra) -> dict:
        """A tile layer of an infinite map, stored as square chunks whose first cells are the given positions."""
        stored = [{"data": [cell(x + column, y + row) for row in range(size) for column in range(size)], "height": size, "width": size, "x": x, "y": y} for x, y in chunks]
        left, top = min(x for x, _ in chunks), min(y for _, y in chunks)
        right, bottom = max(x for x, _ in chunks) + size, max(y for _, y in chunks) + size
        return self.layer("tilelayer", name, chunks=stored, startx=left, starty=top, width=right - left, height=bottom - top, **extra)

    def objects(self, name: str, objects: list[dict], **extra) -> dict:
        return self.layer("objectgroup", name, draworder="topdown", objects=objects, **extra)

    def image(self, name: str, image: str, width: int, height: int, **extra) -> dict:
        return self.layer("imagelayer", name, image=image, imagewidth=width, imageheight=height, **extra)

    def group(self, name: str, layers: list[dict], **extra) -> dict:
        return self.layer("group", name, layers=layers, **extra)

    def object(self, **fields) -> dict:
        result = {"id": self.next_object, "name": "", "type": "", "x": 0, "y": 0, "width": 0, "height": 0, "rotation": 0, "visible": True}
        result.update(fields)
        self.next_object += 1
        return result

    def add(self, *layers: dict) -> MapBuilder:
        self.document["layers"].extend(layers)
        return self

    def save(self, path: Path) -> None:
        self.document["nextlayerid"] = self.next_layer
        self.document["nextobjectid"] = self.next_object
        save(self.document, path)


def tileset(name: str, image: str, image_width: int, image_height: int, tile_width: int, tile_height: int, tiles: list[dict] | None = None, **extra) -> dict:
    columns = image_width // tile_width
    document = {
        "columns": columns,
        "image": image,
        "imageheight": image_height,
        "imagewidth": image_width,
        "margin": 0,
        "name": name,
        "spacing": 0,
        "tilecount": columns * (image_height // tile_height),
        "tiledversion": TILED_VERSION,
        "tileheight": tile_height,
        "tilewidth": tile_width,
        "type": "tileset",
        "version": FORMAT_VERSION,
    }
    if tiles:
        document["tiles"] = tiles
    document.update(extra)
    return document


def collision(*objects: dict) -> dict:
    """The collision shapes of a tile, as Tiled stores them in the object group of the tile."""
    numbered = []
    for index, shape in enumerate(objects, start=1):
        entry = {"id": index, "name": "", "type": "", "rotation": 0, "visible": True, "x": 0, "y": 0, "width": 0, "height": 0}
        entry.update(shape)
        numbered.append(entry)
    return {"draworder": "index", "name": "", "objects": numbered, "opacity": 1, "type": "objectgroup", "visible": True, "x": 0, "y": 0}


def animation(first: int, count: int, milliseconds: int) -> list[dict]:
    return [{"duration": milliseconds, "tileid": first + frame} for frame in range(count)]
