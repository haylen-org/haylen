"""Checks that the frames of every animated sheet of the samples hold one pivot, so no animation dances.

The file `tools/sprite-sheets.json` declares every animated sheet of the repository: an atlas whose tags or numbered frames are its clips, a grid whose clips list their cells, or a Tiled tileset whose animated tiles are its clips. A sheet names the pivot its clips keep, and `pivots` gives a clip another one: `base` for characters and objects that stand on the ground, whose feet stay on one line and whose head and torso stay over one point, `foot` for things fixed at their base that move above it, such as a torch whose flame flickers over its bracket, whose lowest part stays on one point, or `center` for items that spin in place, whose middle stays on one point. A clip that moves the body on purpose, such as an attack that lunges or a death that falls, is listed under `free` with the reason. Trimmed atlas frames count from their place in the untrimmed frame, and a clip of a tileset painted over another tile, such as a torch on a wall, names that tile under `behind`, so only what differs from it counts. Run it with `uv run tools/check-sprite-sheets.py`, which prints every clip that drifts and fails when one does, and with `--align <sheet>` to move the content of every frame of a grid or a tileset whose clips drift inside its cell, onto the middle pivot of its clip, and save the image.
"""

import json
import re
import sys
from pathlib import Path

from png_image import Image
from sprite_frame import SpriteFrame


class SpriteSheets:
    """The check and the alignment of the pivots of the animated sheets that `tools/sprite-sheets.json` declares."""

    ROOT = Path(__file__).resolve().parents[1]
    MANIFEST = ROOT / "tools" / "sprite-sheets.json"
    NUMBERED = re.compile(r"^(.*?)[-_ ](\d+)(\.png)?$")
    # The measures of `SpriteFrame.measure` that each pivot keeps across and down.
    PIVOT_MEASURES = {"base": (1, 0), "foot": (4, 0), "center": (2, 3)}

    @staticmethod
    def atlas_clips(path: Path) -> dict[str, list[SpriteFrame]]:
        document = json.loads(path.read_text(encoding="utf-8"))
        entries = document["frames"]
        named = list(entries.items()) if isinstance(entries, dict) else [(entry["filename"], entry) for entry in entries]
        frames = []
        for name, entry in named:
            region, trim = entry["frame"], entry.get("spriteSourceSize", {})
            source = entry.get("sourceSize")
            full = (source["w"], source["h"]) if source else None
            frames.append((name, SpriteFrame(region["x"], region["y"], region["w"], region["h"], trim.get("x", 0), trim.get("y", 0), full)))
        tags = document.get("meta", {}).get("frameTags", [])
        if tags:
            return {tag["name"]: [frame for _, frame in frames[tag["from"] : tag["to"] + 1]] for tag in tags}
        clips: dict[str, list[tuple[int, SpriteFrame]]] = {}
        for name, frame in frames:
            match = SpriteSheets.NUMBERED.match(name)
            if match:
                clips.setdefault(match.group(1), []).append((int(match.group(2)), frame))
        return {name: [frame for _, frame in sorted(items, key=lambda item: item[0])] for name, items in clips.items() if len(items) > 1}

    @staticmethod
    def grid_clips(sheet: dict) -> dict[str, list[SpriteFrame]]:
        width, height = sheet["frame"]
        columns = Image.read(SpriteSheets.ROOT / sheet["grid"]).width // width
        return {name: [SpriteFrame((cell - 1) % columns * width, (cell - 1) // columns * height, width, height) for cell in cells] for name, cells in sheet["clips"].items()}

    @staticmethod
    def tile(tileset: dict, index: int, under: int | None = None) -> SpriteFrame:
        """The frame of a tile of a tileset, painted over the tile `under` when it names one."""
        width, height = tileset["tilewidth"], tileset["tileheight"]
        margin, spacing, columns = tileset.get("margin", 0), tileset.get("spacing", 0), tileset["columns"]
        behind = None if under is None else SpriteSheets.tile(tileset, under)
        return SpriteFrame(margin + index % columns * (width + spacing), margin + index // columns * (height + spacing), width, height, behind=behind)

    @staticmethod
    def tileset_clips(path: Path, behind: dict[str, int]) -> dict[str, list[SpriteFrame]]:
        tileset = json.loads(path.read_text(encoding="utf-8"))
        clips = {}
        for entry in tileset.get("tiles", []):
            if "animation" in entry:
                name = str(entry.get("type") or entry.get("class") or entry["id"])
                clips[name] = [SpriteSheets.tile(tileset, step["tileid"], behind.get(name)) for step in entry["animation"]]
        return clips

    @staticmethod
    def image_of(sheet: dict) -> Path:
        if "grid" in sheet:
            return SpriteSheets.ROOT / sheet["grid"]
        if "tileset" in sheet:
            tileset = json.loads((SpriteSheets.ROOT / sheet["tileset"]).read_text(encoding="utf-8"))
            return (SpriteSheets.ROOT / sheet["tileset"]).parent / tileset["image"]
        document = json.loads((SpriteSheets.ROOT / sheet["atlas"]).read_text(encoding="utf-8"))
        return (SpriteSheets.ROOT / sheet["atlas"]).parent / document["meta"]["image"]

    @staticmethod
    def clips_of(sheet: dict) -> dict[str, list[SpriteFrame]]:
        if "grid" in sheet:
            return SpriteSheets.grid_clips(sheet)
        if "tileset" in sheet:
            return SpriteSheets.tileset_clips(SpriteSheets.ROOT / sheet["tileset"], sheet.get("behind", {}))
        return SpriteSheets.atlas_clips(SpriteSheets.ROOT / sheet["atlas"])

    @staticmethod
    def spread(measures: list[tuple[float, ...]], index: int) -> float:
        return max(measure[index] for measure in measures) - min(measure[index] for measure in measures)

    @staticmethod
    def drift(frames: list[SpriteFrame], image: Image, pivot: str) -> str | None:
        """Describes how far the frames of a clip move off their pivot, or returns nothing when they hold it."""
        measures = [measure for measure in (frame.measure(image) for frame in frames) if measure]
        if len(measures) < 2:
            return None
        width, height = frames[0].full
        if pivot == "foot":
            across, feet = SpriteSheets.spread(measures, 4), SpriteSheets.spread(measures, 0)
            if across > max(3.0, width * 0.04) or feet > max(3.0, height * 0.04):
                return f"the lowest part moves {across:.1f} pixels sideways and {feet:.1f} pixels up and down"
            return None
        if pivot == "base":
            torso, feet = SpriteSheets.spread(measures, 1), SpriteSheets.spread(measures, 0)
            if torso > max(3.0, width * 0.04) or feet > max(3.0, height * 0.10):
                return f"the head and torso move {torso:.1f} pixels sideways and the feet {feet:.1f} pixels up and down"
            return None
        across, down = SpriteSheets.spread(measures, 2), SpriteSheets.spread(measures, 3)
        if max(across, down) > max(2.0, max(width, height) * 0.04):
            return f"the middle moves {across:.1f} pixels sideways and {down:.1f} pixels up and down"
        return None

    @staticmethod
    def move(image: Image, frame: SpriteFrame, shift_x: int, shift_y: int, clip: str) -> None:
        """Moves the content of a frame inside its cell, and stops when the content has no room there."""
        cell = image.crop(frame.x, frame.y, frame.width, frame.height)
        lit = [(x, y) for y in range(frame.height) for x in range(frame.width) if cell.alpha(x, y)]
        if any(not (0 <= x + shift_x < frame.width and 0 <= y + shift_y < frame.height) for x, y in lit):
            sys.exit(f'A frame of the clip "{clip}" has no room in its cell to move onto the pivot, so it needs to be painted again.')
        image.paste(Image.blank(frame.width, frame.height), frame.x, frame.y)
        for y in range(frame.height):
            for x in range(frame.width):
                column, row = x + shift_x, y + shift_y
                if 0 <= column < frame.width and 0 <= row < frame.height:
                    source = (y * frame.width + x) * 4
                    destination = ((frame.y + row) * image.width + frame.x + column) * 4
                    image.pixels[destination : destination + 4] = cell.pixels[source : source + 4]

    @staticmethod
    def align(sheet: dict) -> int:
        """Moves the content of every frame of the drifting clips of a grid or a tileset onto the middle pivot of its clip, saves the image and returns how many frames moved."""
        path = SpriteSheets.image_of(sheet)
        image = Image.read(path)
        free = sheet.get("free", {})
        moved = 0
        for clip, frames in SpriteSheets.clips_of(sheet).items():
            pivot = sheet.get("pivots", {}).get(clip, sheet["pivot"])
            if clip in free or not SpriteSheets.drift(frames, image, pivot):
                continue
            measures = [frame.measure(image) for frame in frames]
            across, down = SpriteSheets.PIVOT_MEASURES[pivot]
            target_x = sorted(measure[across] for measure in measures)[len(measures) // 2]
            target_y = sorted(measure[down] for measure in measures)[len(measures) // 2]
            for frame, measure in zip(frames, measures, strict=True):
                shift_x, shift_y = round(target_x - measure[across]), round(target_y - measure[down])
                if shift_x == 0 and shift_y == 0:
                    continue
                SpriteSheets.move(image, frame, shift_x, shift_y, clip)
                moved += 1
        image.write(path)
        return moved

    @staticmethod
    def check(sheets: list[dict]) -> list[str]:
        problems = []
        for sheet in sheets:
            name = sheet.get("atlas") or sheet.get("grid") or sheet.get("tileset")
            image = Image.read(SpriteSheets.image_of(sheet))
            free = sheet.get("free", {})
            clips = SpriteSheets.clips_of(sheet)
            problems += [f'The sheet `{name}` frees the clip "{clip}", which it does not have.' for clip in free if clip not in clips]
            for clip, frames in clips.items():
                if clip in free:
                    continue
                pivot = sheet.get("pivots", {}).get(clip, sheet["pivot"])
                moved = SpriteSheets.drift(frames, image, pivot)
                if moved:
                    problems.append(f'The clip "{clip}" of `{name}` does not hold its {pivot} pivot: {moved}.')
        return problems

    @staticmethod
    def undeclared(sheets: list[dict]) -> list[str]:
        """Returns the atlases with tags and the tilesets with animated tiles of the samples that the manifest does not declare."""
        declared = {sheet.get("atlas") or sheet.get("tileset") for sheet in sheets}
        found = []
        samples = SpriteSheets.ROOT / "samples"
        for path in [*sorted(samples.rglob("*.json")), *sorted(samples.rglob("*.tsj"))]:
            try:
                document = json.loads(path.read_text(encoding="utf-8"))
            except json.JSONDecodeError, UnicodeDecodeError:
                continue
            meta = document.get("meta") if isinstance(document, dict) else None
            animated = bool(meta.get("frameTags")) if isinstance(meta, dict) else False
            animated = animated or (path.suffix == ".tsj" and any("animation" in entry for entry in document.get("tiles", [])))
            relative = path.relative_to(SpriteSheets.ROOT).as_posix()
            if animated and relative not in declared:
                found.append(relative)
        return found

    @staticmethod
    def main() -> None:
        sheets = json.loads(SpriteSheets.MANIFEST.read_text(encoding="utf-8"))["sheets"]
        if len(sys.argv) == 3 and sys.argv[1] == "--align":
            sheet = next((sheet for sheet in sheets if sys.argv[2] in (sheet.get("grid"), sheet.get("tileset"))), None)
            if sheet is None:
                sys.exit(f"The manifest declares no grid or tileset `{sys.argv[2]}`.")
            print(f"Moved {SpriteSheets.align(sheet)} frames of `{sys.argv[2]}` onto the pivots of their clips.")
            return
        problems = [f"The animated sheet `{path}` is not declared in `tools/sprite-sheets.json`." for path in SpriteSheets.undeclared(sheets)]
        problems += SpriteSheets.check(sheets)
        for problem in problems:
            print(problem)
        if problems:
            sys.exit(1)
        print(f"Every clip of the {len(sheets)} animated sheets holds its pivot.")


if __name__ == "__main__":
    SpriteSheets.main()
