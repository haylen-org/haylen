"""Generates the content of the Tiled tests: the tilesets, the templates, the maps and the world, as files that open and edit in Tiled 1.12.

The images of the tilesets and the backdrops are painted for the project with the "codex" command line, as the art guide in the README of the test project describes, and the tilesets name them with their sizes. The transitions of water, sand, paths, lava and grass are drawn from those paintings by `transitions.py`, the roofs are packed by `pack-roofs.py`, the scenery tileset names the props of the Camera tests, and the sky and the sea behind the maps are drawn by `images.py`.

Run it from anywhere with `python3 samples/tests/tools/tiled/generate-content.py`. The same run always writes the same files into `samples/tests/content/tiled`.
"""

from __future__ import annotations

import sys
from pathlib import Path

# The tool runs from the test project, which keeps no compiled Python files.
sys.dont_write_bytecode = True

import images  # noqa: E402
import maps_play  # noqa: E402
import maps_views  # noqa: E402
import tilesets  # noqa: E402
import transitions  # noqa: E402
from tiled_json import save  # noqa: E402

CONTENT = Path(__file__).resolve().parents[2] / "content" / "tiled"


def write_tilesets() -> None:
    save(tilesets.terrain(), CONTENT / "tilesets" / "terrain.tsj")
    save(tilesets.props(), CONTENT / "tilesets" / "props.tsj")
    save(tilesets.diamonds(), CONTENT / "tilesets" / "diamonds.tsj")
    save(tilesets.hexagons(), CONTENT / "tilesets" / "hexagons.tsj")
    save(tilesets.scenery(), CONTENT / "tilesets" / "scenery.tsj")
    save(tilesets.roofs(), CONTENT / "tilesets" / "roofs.tsj")
    transitions.write_image()
    images.write_sky()
    images.write_sea()
    save(transitions.transitions(), CONTENT / "tilesets" / "transitions.tsj")
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
    maps_play.garden(CONTENT)


def main() -> None:
    write_tilesets()
    write_maps()
    print(f'Wrote the content of the Tiled tests to "{CONTENT}".')


if __name__ == "__main__":
    main()
