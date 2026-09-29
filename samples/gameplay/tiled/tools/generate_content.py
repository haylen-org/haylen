"""Generates the content of the Tiled sample: the pixel art, the tilesets, the templates, the maps and the world, as files that open and edit in Tiled 1.12.

Run it from anywhere with `python3 samples/gameplay/tiled/tools/generate_content.py`. The same run always writes the same files.
"""

from __future__ import annotations

import sys
from pathlib import Path

# The tool runs from the sample folder, which keeps no compiled Python files.
sys.dont_write_bytecode = True

import art  # noqa: E402
import maps_play  # noqa: E402
import maps_views  # noqa: E402
import tilesets  # noqa: E402
from tiled_json import save  # noqa: E402

CONTENT = Path(__file__).resolve().parents[1] / "content"


def write_art() -> None:
    art.terrain().save(CONTENT / "tiles" / "terrain.png")
    art.props().save(CONTENT / "tiles" / "props.png")
    art.diamond_tiles().save(CONTENT / "tiles" / "diamonds.png")
    art.hex_tiles().save(CONTENT / "tiles" / "hexagons.png")
    art.sky().save(CONTENT / "images" / "sky.png")
    art.clouds().save(CONTENT / "images" / "clouds.png")
    art.hills(640, 200, "#4a5d7a", 7, 3).save(CONTENT / "images" / "hills_far.png")
    art.hills(640, 160, "#3d6b4a", 8, 2).save(CONTENT / "images" / "hills_near.png")
    art.hero().save(CONTENT / "sprites" / "hero.png")


def write_tilesets() -> None:
    save(tilesets.terrain(), CONTENT / "tilesets" / "terrain.tsj")
    save(tilesets.props(), CONTENT / "tilesets" / "props.tsj")
    save(tilesets.diamonds(), CONTENT / "tilesets" / "diamonds.tsj")
    save(tilesets.hexagons(), CONTENT / "tilesets" / "hexagons.tsj")
    save(tilesets.lamp_template(), CONTENT / "templates" / "lamp.tj")
    save(tilesets.sign_template(), CONTENT / "templates" / "sign.tj")


def write_maps() -> None:
    maps = CONTENT / "maps"
    maps_views.orthogonal(maps)
    maps_views.diamond_map(maps, "isometric", 16, 16, orientation="isometric")
    maps_views.diamond_map(maps, "staggered", 12, 28, orientation="staggered", staggeraxis="y", staggerindex="odd")
    maps_views.hexagonal(maps)
    maps_views.oblique(maps)
    maps_views.infinite(maps)
    maps_views.animations(maps)
    maps_views.parallax(maps)
    maps_views.groups(maps)
    maps_views.world(maps / "world")
    maps_play.objects(maps)
    maps_play.properties(maps)
    maps_play.collision(maps)
    maps_play.spawning(maps)
    maps_play.ysort(maps)
    maps_play.raycasts(maps)


def main() -> None:
    write_art()
    write_tilesets()
    write_maps()
    print(f"Wrote the content of the Tiled sample to {CONTENT}")


if __name__ == "__main__":
    main()
