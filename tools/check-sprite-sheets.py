"""Checks that the frames of every animated sheet of the samples hold one pivot, so no animation dances.

The file `tools/sprite-sheets.json` declares every animated sheet of the repository: an atlas whose tags or numbered frames are its clips, a grid whose clips list their cells, or a Tiled tileset whose animated tiles are its clips. A sheet names the pivot its clips keep, and `pivots` gives a clip another one: `base` for characters and objects that stand on the ground, whose feet stay on one line and whose head and torso stay over one point, `foot` for things fixed at their base that move above it, such as a torch whose flame flickers over its bracket, whose lowest part stays on one point, or `center` for items that spin in place, whose middle stays on one point. A clip that moves the body on purpose, such as an attack that lunges or a death that falls, is listed under `free` with the reason. Trimmed atlas frames count from their place in the untrimmed frame, and a clip of a tileset painted over another tile, such as a torch on a wall, names that tile under `behind`, so only what differs from it counts. Run it with `python3 tools/check-sprite-sheets.py`, which prints every clip that drifts and fails when one does, and with `--align <sheet>` to move the content of every frame of a grid or a tileset whose clips drift inside its cell, onto the middle pivot of its clip, and save the image.
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))

from png_image import Image, read, write  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "tools" / "sprite-sheets.json"
NUMBERED = re.compile(r"^(.*?)[-_ ](\d+)(\.png)?$")


class Frame:
    """A frame of a clip: its region of the image and where that region sits in the untrimmed frame."""

    def __init__(self, x: int, y: int, width: int, height: int, left: int = 0, top: int = 0, full: tuple[int, int] | None = None, behind: Frame | None = None) -> None:
        self.x, self.y, self.width, self.height = x, y, width, height
        self.left, self.top = left, top
        self.full = full or (width, height)
        self.behind = behind

    def shows(self, image: Image, x: int, y: int) -> bool:
        """Tells whether a pixel of the frame is part of what it draws, which over a background is what differs from it."""
        offset = ((self.y + y) * image.width + self.x + x) * 4
        if self.behind is None:
            return image.pixels[offset + 3] > 96
        under = ((self.behind.y + y) * image.width + self.behind.x + x) * 4
        return sum(abs(image.pixels[offset + channel] - image.pixels[under + channel]) for channel in range(3)) >= 40

    def measure(self, image: Image) -> tuple[float, float, float, float, float] | None:
        """Returns the line of the feet, the middle of the head and torso, the middle of the visible box and the middle of its lowest part, in the untrimmed frame."""
        lit = [(x, y) for y in range(self.height) for x in range(self.width) if self.shows(image, x, y)]
        if not lit:
            return None
        xs = [x for x, _ in lit]
        ys = [y for _, y in lit]
        top, bottom = min(ys), max(ys)
        upper = [x for x, y in lit if y <= top + (bottom - top) * 0.55]
        lower = [x for x, y in lit if y >= bottom - (bottom - top) * 0.15]
        return bottom + self.top, sum(upper) / len(upper) + self.left, (min(xs) + max(xs)) / 2 + self.left, (top + bottom) / 2 + self.top, sum(lower) / len(lower) + self.left


def atlas_clips(path: Path) -> dict[str, list[Frame]]:
    document = json.loads(path.read_text())
    entries = document["frames"]
    named = list(entries.items()) if isinstance(entries, dict) else [(entry["filename"], entry) for entry in entries]
    frames = []
    for name, entry in named:
        region, trim = entry["frame"], entry.get("spriteSourceSize", {})
        source = entry.get("sourceSize")
        frames.append((name, Frame(region["x"], region["y"], region["w"], region["h"], trim.get("x", 0), trim.get("y", 0), (source["w"], source["h"]) if source else None)))
    tags = document.get("meta", {}).get("frameTags", [])
    if tags:
        return {tag["name"]: [frame for _, frame in frames[tag["from"] : tag["to"] + 1]] for tag in tags}
    clips: dict[str, list[tuple[int, Frame]]] = {}
    for name, frame in frames:
        match = NUMBERED.match(name)
        if match:
            clips.setdefault(match.group(1), []).append((int(match.group(2)), frame))
    return {name: [frame for _, frame in sorted(items, key=lambda item: item[0])] for name, items in clips.items() if len(items) > 1}


def grid_clips(sheet: dict) -> dict[str, list[Frame]]:
    width, height = sheet["frame"]
    columns = read(ROOT / sheet["grid"]).width // width
    return {name: [Frame((cell - 1) % columns * width, (cell - 1) // columns * height, width, height) for cell in cells] for name, cells in sheet["clips"].items()}


def tileset_clips(path: Path, behind: dict[str, int]) -> dict[str, list[Frame]]:
    tileset = json.loads(path.read_text())
    width, height = tileset["tilewidth"], tileset["tileheight"]
    margin, spacing, columns = tileset.get("margin", 0), tileset.get("spacing", 0), tileset["columns"]

    def tile(index: int, under: int | None = None) -> Frame:
        return Frame(margin + index % columns * (width + spacing), margin + index // columns * (height + spacing), width, height, behind=None if under is None else tile(under))

    clips = {}
    for entry in tileset.get("tiles", []):
        if "animation" in entry:
            name = str(entry.get("type") or entry.get("class") or entry["id"])
            clips[name] = [tile(step["tileid"], behind.get(name)) for step in entry["animation"]]
    return clips


def image_of(sheet: dict) -> Path:
    if "grid" in sheet:
        return ROOT / sheet["grid"]
    if "tileset" in sheet:
        tileset = json.loads((ROOT / sheet["tileset"]).read_text())
        return (ROOT / sheet["tileset"]).parent / tileset["image"]
    document = json.loads((ROOT / sheet["atlas"]).read_text())
    return (ROOT / sheet["atlas"]).parent / document["meta"]["image"]


def clips_of(sheet: dict) -> dict[str, list[Frame]]:
    if "grid" in sheet:
        return grid_clips(sheet)
    if "tileset" in sheet:
        return tileset_clips(ROOT / sheet["tileset"], sheet.get("behind", {}))
    return atlas_clips(ROOT / sheet["atlas"])


def drift(frames: list[Frame], image: Image, pivot: str) -> str | None:
    """Describes how far the frames of a clip move off their pivot, or returns nothing when they hold it."""
    measures = [measure for measure in (frame.measure(image) for frame in frames) if measure]
    if len(measures) < 2:
        return None
    width, height = frames[0].full

    def spread(index: int) -> float:
        return max(measure[index] for measure in measures) - min(measure[index] for measure in measures)

    if pivot == "foot":
        across, feet = spread(4), spread(0)
        if across > max(3.0, width * 0.04) or feet > max(3.0, height * 0.04):
            return f"the lowest part moves {across:.1f} pixels sideways and {feet:.1f} pixels up and down"
        return None
    if pivot == "base":
        torso, feet = spread(1), spread(0)
        if torso > max(3.0, width * 0.04) or feet > max(3.0, height * 0.10):
            return f"the head and torso move {torso:.1f} pixels sideways and the feet {feet:.1f} pixels up and down"
        return None
    across, down = spread(2), spread(3)
    if max(across, down) > max(2.0, max(width, height) * 0.04):
        return f"the middle moves {across:.1f} pixels sideways and {down:.1f} pixels up and down"
    return None


def align(sheet: dict) -> int:
    """Moves the content of every frame of the drifting clips of a grid or a tileset onto the middle pivot of its clip, saves the image and returns how many frames moved."""
    path = image_of(sheet)
    image = read(path)
    free = sheet.get("free", {})
    moved = 0
    for clip, frames in clips_of(sheet).items():
        pivot = sheet.get("pivots", {}).get(clip, sheet["pivot"])
        if clip in free or not drift(frames, image, pivot):
            continue
        measures = [frame.measure(image) for frame in frames]
        across, down = {"base": (1, 0), "foot": (4, 0), "center": (2, 3)}[pivot]
        targetX = sorted(measure[across] for measure in measures)[len(measures) // 2]
        targetY = sorted(measure[down] for measure in measures)[len(measures) // 2]
        for frame, measure in zip(frames, measures):
            shiftX, shiftY = round(targetX - measure[across]), round(targetY - measure[down])
            if shiftX == 0 and shiftY == 0:
                continue
            cell = image.crop(frame.x, frame.y, frame.width, frame.height)
            lit = [(x, y) for y in range(frame.height) for x in range(frame.width) if cell.alpha(x, y)]
            if any(not (0 <= x + shiftX < frame.width and 0 <= y + shiftY < frame.height) for x, y in lit):
                sys.exit(f'A frame of the clip "{clip}" has no room in its cell to move onto the pivot, so it needs to be painted again.')
            image.paste(Image.blank(frame.width, frame.height), frame.x, frame.y)
            for y in range(frame.height):
                for x in range(frame.width):
                    targetColumn, targetRow = x + shiftX, y + shiftY
                    if 0 <= targetColumn < frame.width and 0 <= targetRow < frame.height:
                        source = (y * frame.width + x) * 4
                        destination = ((frame.y + targetRow) * image.width + frame.x + targetColumn) * 4
                        image.pixels[destination : destination + 4] = cell.pixels[source : source + 4]
            moved += 1
    write(image, path)
    return moved


def check(sheets: list[dict]) -> list[str]:
    problems = []
    for sheet in sheets:
        name = sheet.get("atlas") or sheet.get("grid") or sheet.get("tileset")
        image = read(image_of(sheet))
        free = sheet.get("free", {})
        clips = clips_of(sheet)
        for clip in free:
            if clip not in clips:
                problems.append(f'The sheet `{name}` frees the clip "{clip}", which it does not have.')
        for clip, frames in clips.items():
            if clip in free:
                continue
            pivot = sheet.get("pivots", {}).get(clip, sheet["pivot"])
            moved = drift(frames, image, pivot)
            if moved:
                problems.append(f'The clip "{clip}" of `{name}` does not hold its {pivot} pivot: {moved}.')
    return problems


def undeclared(sheets: list[dict]) -> list[str]:
    """Returns the atlases with tags and the tilesets with animated tiles of the samples that the manifest does not declare."""
    declared = {sheet.get("atlas") or sheet.get("tileset") for sheet in sheets}
    found = []
    for path in sorted((ROOT / "samples").rglob("*.json")) + sorted((ROOT / "samples").rglob("*.tsj")):
        try:
            document = json.loads(path.read_text())
        except (json.JSONDecodeError, UnicodeDecodeError):
            continue
        animated = isinstance(document, dict) and (document.get("meta", {}).get("frameTags") if isinstance(document.get("meta"), dict) else None)
        animated = animated or (path.suffix == ".tsj" and any("animation" in entry for entry in document.get("tiles", [])))
        relative = path.relative_to(ROOT).as_posix()
        if animated and relative not in declared:
            found.append(relative)
    return found


def main() -> None:
    sheets = json.loads(MANIFEST.read_text())["sheets"]
    if len(sys.argv) == 3 and sys.argv[1] == "--align":
        sheet = next((sheet for sheet in sheets if sys.argv[2] in (sheet.get("grid"), sheet.get("tileset"))), None)
        if sheet is None:
            sys.exit(f"The manifest declares no grid or tileset `{sys.argv[2]}`.")
        print(f"Moved {align(sheet)} frames of `{sys.argv[2]}` onto the pivots of their clips.")
        return
    problems = [f"The animated sheet `{path}` is not declared in `tools/sprite-sheets.json`." for path in undeclared(sheets)] + check(sheets)
    for problem in problems:
        print(problem)
    if problems:
        sys.exit(1)
    print(f"Every clip of the {len(sheets)} animated sheets holds its pivot.")


if __name__ == "__main__":
    main()
