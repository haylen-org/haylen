"""Checks the edges of every image of the samples and the templates, so no picture carries what is left of the background it was painted on or jagged edges.

The art of the repository is painted on flat magenta, which the tools that cut it turn into transparency. An image fails when a pixel that is partly transparent is still close to that magenta, which shows as a pink rim or speck, and when its edges are jagged: when many opaque pixels at its outline touch transparency both sideways and up or down, the steps of an edge drawn without the soft pixels that smooth it. Pixel art and images that tests read pixel by pixel keep hard edges on purpose and are listed in `tools/image-edges.json` with the reason. Run it with `python3 tools/check-image-edges.py`, which prints every image that fails and fails when one does, and with `--clean <image>` to make every leftover pixel of the magenta of an image transparent.
"""

from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))

from png_image import Image, read, write  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
EXCEPTIONS = ROOT / "tools" / "image-edges.json"
KEY = (255, 0, 255)
# How close to the magenta a partly transparent pixel is when it is what is left of the background, and the share of steps among the outline pixels that makes an edge jagged.
KEY_DISTANCE = 50
JAGGED_SHARE = 0.2


def leftovers(image: Image) -> list[int]:
    """Returns the pixels that are partly transparent and still close to the magenta of the background."""
    found = []
    for index in range(image.width * image.height):
        alpha = image.pixels[index * 4 + 3]
        if 0 < alpha < 200 and sum((image.pixels[index * 4 + channel] - KEY[channel]) ** 2 for channel in range(3)) < KEY_DISTANCE**2:
            found.append(index)
    return found


def jagged(image: Image) -> float:
    """Returns the share of the opaque outline pixels that are steps, which touch transparency both sideways and up or down."""
    width, alphas = image.width, image.pixels[3::4]
    outline = steps = 0
    for y in range(1, image.height - 1):
        for x in range(1, width - 1):
            index = y * width + x
            if alphas[index] != 255:
                continue
            sideways = alphas[index - 1] == 0 or alphas[index + 1] == 0
            upright = alphas[index - width] == 0 or alphas[index + width] == 0
            if sideways or upright:
                outline += 1
                steps += sideways and upright
    return steps / outline if outline > 40 else 0.0


def images() -> list[str]:
    listed = subprocess.run(["git", "ls-files", "--", "samples/*.png", "templates/*.png"], cwd=ROOT, capture_output=True, text=True, check=True).stdout.split()
    return [name for name in listed if (ROOT / name).exists()]


def check() -> list[str]:
    hard = json.loads(EXCEPTIONS.read_text())["hardEdges"]
    problems = [f"The image `{name}` is listed in `tools/image-edges.json` but does not exist." for name in hard if not (ROOT / name).exists()]
    for name in images():
        image = read(ROOT / name)
        if min(image.pixels[3::4]) == 255:
            continue
        left = leftovers(image)
        if left:
            problems.append(f"The image `{name}` keeps {len(left)} pixels of the magenta it was painted on. Clean it with \"python3 tools/check-image-edges.py --clean {name}\".")
        share = 0.0 if name in hard else jagged(image)
        if share > JAGGED_SHARE:
            problems.append(f"The image `{name}` has jagged edges: {share:.0%} of its outline pixels are steps. Paint it again with smooth edges, or list it in `tools/image-edges.json` when it is pixel art.")
    return problems


def clean(name: str) -> int:
    path = ROOT / name
    image = read(path)
    left = leftovers(image)
    for index in left:
        image.pixels[index * 4 : index * 4 + 4] = bytes(4)
    write(image, path)
    return len(left)


def main() -> None:
    if len(sys.argv) == 3 and sys.argv[1] == "--clean":
        print(f"Made {clean(sys.argv[2])} pixels of the magenta of `{sys.argv[2]}` transparent.")
        return
    problems = check()
    for problem in problems:
        print(problem)
    if problems:
        sys.exit(1)
    print("Every image of the samples and the templates has clean edges.")


if __name__ == "__main__":
    main()
