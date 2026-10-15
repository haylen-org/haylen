"""Packs the particle pieces of the effect library into its two atlases and moves the frames of every effect file with them.

The pieces are the images painted for the library with the "codex" command line, following the style guide in the README of the test project, one 8-bit RGBA PNG per piece: every name of SMOOTH and PIXEL, where a sheet of frames is the pieces `<name>-1` to `<name>-<count>`. Run it with `python3 samples/tests/tools/particles/pack-particle-textures.py <folder of the pieces>`. The tool writes `effects.png` and `pixel.png` with their atlas files into `content/particles/library/textures/`, adds the premultiplied copy of the soft dot, writes the glow line and the sheet of the runes, which tests draw whole, as files of their own, and rewrites the `frames` of every effect file that draws from an atlas, which it finds through the atlas files it replaces. The same pieces always make the same files.
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

# The tool runs from the test project, which keeps no compiled Python files.
sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[4] / "tools"))

from png_image import Image, read, write  # noqa: E402

CONTENT = Path(__file__).resolve().parents[2] / "content"
TEXTURES = CONTENT / "particles" / "library" / "textures"
WIDTH = {"effects": 1024, "pixel": 128}
GAP = {"effects": 4, "pixel": 2}
PREMULTIPLIED = ("soft-dot", "soft-dot-premultiplied")
# Textures that tests draw whole, as the ribbon of a trail and the sheet that "frameGrid" cuts, stay files of their own.
GLOW_LINE = "glow-line"
RUNES = ("rune", 4)
FRAMES = re.compile(r'"frames": (\[\[.*?\]\])')
TEXTURE = re.compile(r'"texture": "([^"]+)"')

# Every piece of an atlas with its count of frames.
SMOOTH = {
    "soft-dot": 1, "hard-dot": 1, "flash-burst": 1, "ring-thin": 1, "light-beam": 1, "spark-streak": 1, "swirl": 1, "slash-arc": 1,
    "rain-streak": 1, "bubble": 1, "cartoon-puff": 1, "droplet": 1, "feather": 1, "gem": 1, "heart": 1, "hex-cell": 1, "impact-star": 1,
    "plus-sign": 1, "shard": 1, "skull": 1, "snowflake": 1, "star-4point": 1, "star-5point": 1, "ash-flake": 1, "smoke-puff": 1,
    "flame-soft": 1, "coin": 4, "confetti": 4, "debris": 4, "leaf": 4, "petal": 4, "sparkle": 4, "smoke": 4, "flame": 4, "fireball": 4,
    "bolt": 4,
}
PIXEL = {
    "pixel-bolt": 4, "pixel-bubble": 1, "pixel-coin": 4, "pixel-dot": 4, "pixel-drop": 1, "pixel-explosion": 4, "pixel-flame": 4,
    "pixel-leaf": 4, "pixel-ring": 4, "pixel-smoke": 4, "pixel-spark": 1, "pixel-square": 1, "pixel-star": 4, "pixel-streak": 1,
}


def names(pieces: dict[str, int]) -> list[str]:
    return [name if count == 1 else f"{name}-{frame}" for name, count in pieces.items() for frame in range(1, count + 1)]


def premultiply(image: Image) -> Image:
    result = Image(image.width, image.height, bytearray(image.pixels))
    for start in range(0, len(result.pixels), 4):
        alpha = result.pixels[start + 3]
        for channel in range(3):
            result.pixels[start + channel] = round(result.pixels[start + channel] * alpha / 255)
    return result


def pack(pieces: dict[str, Image], width: int, gap: int) -> tuple[Image, dict[str, list[int]]]:
    """Drops the pieces from the tallest down onto the skyline of the atlas, each where it rests lowest, and returns the atlas with the region of every piece."""
    order = sorted(pieces, key=lambda name: (-pieces[name].height, -pieces[name].width, name))
    skyline = [0] * width
    regions: dict[str, list[int]] = {}
    for name in order:
        room = pieces[name].width + gap
        x = min(range(0, width - room + 1, gap), key=lambda left: (max(skyline[left : left + room]), left))
        y = max(skyline[x : x + room])
        skyline[x : x + room] = [y + pieces[name].height + gap] * room
        regions[name] = [x, y, pieces[name].width, pieces[name].height]

    atlas = Image.blank(width, max(skyline) - gap)
    for name, (left, top, _, _) in regions.items():
        atlas.paste(pieces[name], left, top)
    return atlas, regions


def previous(atlas: str) -> dict[tuple[int, ...], str]:
    """Returns the piece of every region of the atlas file that the tool replaces, or nothing for an atlas that does not exist yet."""
    path = TEXTURES / f"{atlas}.json"
    if not path.exists():
        return {}
    frames = json.loads(path.read_text())["frames"]
    return {(frame["frame"]["x"], frame["frame"]["y"], frame["frame"]["w"], frame["frame"]["h"]): name for name, frame in frames.items()}


def document(atlas: str, regions: dict[str, list[int]], order: list[str]) -> str:
    """Writes the atlas file in the hash layout with one line per frame, in the order of the pieces."""
    frames = [f'        "{name}": {{"frame": {{"x": {x}, "y": {y}, "w": {w}, "h": {h}}}}}' for name in order for x, y, w, h in [regions[name]]]
    return '{\n    "frames": {\n' + ",\n".join(frames) + f'\n    }},\n    "meta": {{"image": "{atlas}.png"}}\n}}\n'


def move_frames(atlas: str, before: dict[tuple[int, ...], str], regions: dict[str, list[int]]) -> None:
    """Rewrites the frames of every effect file that draws from the atlas with the regions that its pieces have now."""
    for path in sorted((CONTENT / "particles").rglob("*.particles")):
        text = path.read_text()
        texture = TEXTURE.search(text)
        if texture is None or (path.parent / texture.group(1)).resolve() != (TEXTURES / f"{atlas}.png").resolve():
            continue
        moved = []
        for rect in json.loads(FRAMES.search(text).group(1)):
            name = before.get(tuple(rect))
            if name not in regions:
                sys.exit(f"The effect file `{path}` draws the frame {rect} of \"{atlas}.png\", which is no piece of the atlas.")
            moved.append(regions[name])
        path.write_text(FRAMES.sub(lambda _: '"frames": ' + json.dumps(moved, separators=(", ", ": ")), text, count=1))


def main() -> None:
    if len(sys.argv) != 2:
        sys.exit("Name the folder of the pieces, such as \"python3 pack-particle-textures.py pieces\".")
    folder = Path(sys.argv[1])
    for atlas, pieces in (("effects", SMOOTH), ("pixel", PIXEL)):
        order = names(pieces)
        images = {name: read(folder / f"{name}.png") for name in order}
        if atlas == "effects":
            images[PREMULTIPLIED[1]] = premultiply(images[PREMULTIPLIED[0]])
            order.insert(1, PREMULTIPLIED[1])
        image, regions = pack(images, WIDTH[atlas], GAP[atlas])
        before = previous(atlas)
        write(image, TEXTURES / f"{atlas}.png")
        (TEXTURES / f"{atlas}.json").write_text(document(atlas, regions, order))
        move_frames(atlas, before, regions)

    write(read(folder / f"{GLOW_LINE}.png"), TEXTURES / f"{GLOW_LINE}.png")
    runes = [read(folder / f"{RUNES[0]}-{frame}.png") for frame in range(1, RUNES[1] + 1)]
    sheet = Image.blank(sum(rune.width for rune in runes), runes[0].height)
    for index, rune in enumerate(runes):
        sheet.paste(rune, index * runes[0].width, 0)
    write(sheet, TEXTURES / f"{RUNES[0]}-sheet.png")


if __name__ == "__main__":
    main()
