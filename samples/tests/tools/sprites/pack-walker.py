"""Builds the walker sheet of the Sprites and Animation tests from the three sheets of it painted for the project.

The paintings are the idle, run and attack frames of the walker in grids of four columns and two rows of cells of 384 by 512 pixels on flat magenta, painted with the "codex" command line in the style of the README of the test project. Run it with `python3 samples/tests/tools/sprites/pack-walker.py <folder of idle.png, run.png and attack.png>`. The tool keys the magenta out and unmixes it from the edges, scales each clip so the walker keeps one size, measured from the yellow headband down to the feet in a frame of the clip that stands upright, the run at the size of the idle, and writes `content/sprites/sheets/walker.png`: cells of 256 pixels in eight columns, the idle frames in the first row, the run in the second and the attack in the third. Every frame puts the middle of its head and torso on the middle of its cell, and each clip puts the lowest feet of its frames on the ground line of the cells, with the frames where both feet leave the ground at most `LIFT` above it, so the run keeps a natural bounce and no clip drifts.
"""

from __future__ import annotations

import subprocess
import sys
import tempfile
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[4] / "tools"))

from png_image import Image, read, write  # noqa: E402

SHEET = Path(__file__).resolve().parents[2] / "content" / "sprites" / "sheets" / "walker.png"
CELL = 256
COLUMNS = 8
GROUND = 248
HEIGHT = 228
# The highest the feet of a frame go above the ground line, in pixels of a cell.
LIFT = 14
PAINTED = (384, 512)
# Each clip with its count of frames and the frame that stands upright like the first idle frame, which sets the size of the clip, or nothing for a clip painted at the size of the idle.
CLIPS = (("idle", 4, 1), ("run", 8, None), ("attack", 6, 6))


def key(painted: Image) -> Image:
    """Turns the magenta of a painting into transparency, unmixing it from the colors of the edges."""
    result = Image.blank(painted.width, painted.height)
    for index in range(painted.width * painted.height):
        red, green, blue = painted.pixels[index * 4 : index * 4 + 3]
        alpha = min(1.0, max(0.0, 1.0 - (min(red, blue) - green - 30) / 225))
        if alpha <= 0.02:
            continue
        unmixed = [min(255, max(0, round((value - (1 - alpha) * magenta) / alpha))) for value, magenta in ((red, 255), (green, 0), (blue, 255))]
        result.pixels[index * 4 : index * 4 + 4] = bytes((*unmixed, round(alpha * 255)))
    return result


def bounds(image: Image) -> tuple[int, int, int, int, float]:
    """Returns the visible box of a frame and the middle of its head and torso."""
    lit = [(x, y) for y in range(image.height) for x in range(image.width) if image.alpha(x, y) > 128]
    xs = [x for x, _ in lit]
    ys = [y for _, y in lit]
    top, bottom = min(ys), max(ys)
    upper = [x for x, y in lit if y <= top + (bottom - top) * 0.55]
    return min(xs), top, max(xs), bottom, sum(upper) / len(upper)


def stature(image: Image) -> int:
    """Returns the height from the yellow headband down to the feet, which is the same in every frame that stands upright."""
    feet = max(y for y in range(image.height) for x in range(image.width) if image.alpha(x, y) > 128)
    band = sorted(y for y in range(image.height) for x in range(image.width) if image.alpha(x, y) > 128 and image.pixels[(y * image.width + x) * 4] > 200 and image.pixels[(y * image.width + x) * 4 + 1] > 160 and image.pixels[(y * image.width + x) * 4 + 2] < 100)
    return feet - band[len(band) // 4]


def scaled(image: Image, factor: float, folder: Path) -> Image:
    source, target = folder / "frame.png", folder / "scaled.png"
    write(image, source)
    subprocess.run(["sips", "-z", str(round(image.height * factor)), str(round(image.width * factor)), str(source), "--out", str(target)], check=True, capture_output=True)
    return read(target)


def main() -> None:
    if len(sys.argv) != 2:
        sys.exit("Name the folder of the paintings, such as \"python3 pack-walker.py walker\".")
    folder = Path(sys.argv[1])
    frames = {}
    for clip, count, _ in CLIPS:
        painting = read(folder / f"{clip}.png")
        frames[clip] = [key(painting.crop(index % 4 * PAINTED[0], index // 4 * PAINTED[1], *PAINTED)) for index in range(count)]

    _, top, _, bottom, _ = bounds(frames["idle"][0])
    factor = HEIGHT / (bottom - top + 1)
    standing = stature(frames["idle"][0])
    sheet = Image.blank(CELL * COLUMNS, CELL * len(CLIPS))
    with tempfile.TemporaryDirectory() as temporary:
        for row, (clip, _, upright) in enumerate(CLIPS):
            size = factor if upright is None else factor * standing / stature(frames[clip][upright - 1])
            images = [scaled(frame, size, Path(temporary)) for frame in frames[clip]]
            ground = max(bounds(image)[3] for image in images)
            for column, image in enumerate(images):
                _, _, _, bottom, torso = bounds(image)
                left, top = round(CELL / 2 - torso), GROUND - ground + max(0, ground - bottom - LIFT)
                for y in range(image.height):
                    for x in range(image.width):
                        cellX, cellY = left + x, top + y
                        offset = (y * image.width + x) * 4
                        # The scaling leaves a faint rim of the magenta around the frame, which no pixel of the walker is.
                        if image.pixels[offset + 3] < 8:
                            continue
                        if not (0 <= cellX < CELL and 0 <= cellY < CELL):
                            sys.exit(f"Frame {column + 1} of the {clip} clip does not fit its cell, so it needs to be painted smaller.")
                        target = ((row * CELL + cellY) * sheet.width + column * CELL + cellX) * 4
                        sheet.pixels[target : target + 4] = image.pixels[offset : offset + 4]
    write(sheet, SHEET)


if __name__ == "__main__":
    main()
