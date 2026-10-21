"""Packs the pieces of the fantasy theme into one atlas, writes the theme file whose surfaces cut it into nine-slices and cuts the sheet of its icons.

The pieces are the images painted for the theme with the "codex" command line, one 8-bit RGBA PNG per name of PIECES and the sheet `icons.png` with the icons of ICONS in rows of four cells, following the style guide in the README of the test project. Run it with `python3 samples/tests/tools/interface/pack-fantasy-theme.py <folder of the pieces>`. The same pieces always make the same files.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

# The tool runs from the test project, which keeps no compiled Python files.
sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[4] / "tools"))

from png_image import Image, read, write  # noqa: E402

THEME = Path(__file__).resolve().parents[2] / "content" / "interface" / "themes" / "fantasy"
ATLAS = "interface/themes/fantasy/atlas.png"
ATLAS_WIDTH = 1152
GAP = 4
ICONS = ["heart", "star", "coin", "gem", "gear", "shield", "potion", "key"]
ICON_COLUMNS = 4
OPAQUE = 240

BUTTON = {"slice": [44, 56, 40, 56], "scale": 0.6, "filter": "linear"}
PANEL = {"slice": 60, "filter": "linear"}

# Every piece with the surfaces that draw it, each with the nine-slice values of the theme file.
PIECES: dict[str, dict[str, dict]] = {
    "button": {"button": BUTTON, "buttonHover": {**BUTTON, "tint": "#FFE4EAFF"}, "chip": {**BUTTON, "scale": 0.4}},
    "button-pressed": {"buttonPressed": BUTTON},
    "button-primary": {"buttonPrimary": BUTTON, "buttonPrimaryHover": {**BUTTON, "tint": "#FFE4EEFF"}, "segmentSelected": {**BUTTON, "scale": 0.45}},
    "button-primary-pressed": {"buttonPrimaryPressed": BUTTON},
    "button-danger": {"buttonDestructive": BUTTON, "buttonDestructiveHover": {**BUTTON, "tint": "#FFFFE8E6"}},
    "button-danger-pressed": {"buttonDestructivePressed": BUTTON},
    "panel": {
        "panel": {**PANEL, "scale": 0.5, "padding": [20, 22, 24, 22]},
        "card": {**PANEL, "scale": 0.4, "padding": [16, 18, 20, 18]},
        "dialog": {**PANEL, "scale": 0.6, "padding": [26, 28, 30, 28]},
        "window": {**PANEL, "scale": 0.5, "padding": [0, 20, 24, 20]},
        "menu": {**PANEL, "scale": 0.35, "padding": 10},
        "toast": {**PANEL, "scale": 0.35, "padding": [12, 16, 14, 16]},
    },
    "close": {"close": {"filter": "linear"}, "closeHover": {"filter": "linear", "tint": "#FFFFE8E6"}},
    "close-pressed": {"closePressed": {"filter": "linear"}},
    "title-bar": {"titleBar": {"slice": [40, 48, 40, 48], "scale": 0.55, "padding": [8, 26, 12, 26], "filter": "linear"}},
    "banner": {"banner": {"slice": [0, 150, 0, 150], "scale": 0.55, "padding": [10, 90, 26, 90], "filter": "linear"}},
    "tooltip": {"tooltip": {"slice": 30, "scale": 0.5, "padding": [10, 14, 12, 14], "filter": "linear"}},
    "field": {"field": {"slice": 28, "scale": 0.6, "padding": [4, 10, 4, 10], "filter": "linear"}, "slot": {"slice": 28, "scale": 0.6, "filter": "linear"}},
    "field-focused": {"fieldFocused": {"slice": 28, "scale": 0.6, "padding": [4, 10, 4, 10], "filter": "linear"}, "chipSelected": {"slice": 28, "scale": 0.6, "filter": "linear"}},
    "check": {"check": {"filter": "linear"}},
    "check-checked": {"checkChecked": {"filter": "linear"}},
    "radio": {"radio": {"filter": "linear"}},
    "radio-selected": {"radioSelected": {"filter": "linear"}},
    "track": {"track": {"slice": 27, "scale": 0.9, "padding": 6, "filter": "linear"}, "segment": {"slice": 27, "scale": 0.9, "padding": 4, "filter": "linear"}},
    "fill": {"trackFill": {"slice": 15, "scale": 1.2, "colorize": True, "filter": "linear"}},
    "knob": {"knob": {"colorize": True, "filter": "linear"}},
    "tab": {"tab": {"slice": [36, 40, 20, 40], "scale": 0.5, "filter": "linear"}},
    "tab-selected": {"tabSelected": {"slice": [36, 40, 20, 40], "scale": 0.5, "filter": "linear"}},
    "scrollbar": {"scrollbar": {"slice": 32, "scale": 0.28, "colorize": True, "filter": "linear"}},
    "scrollbar-track": {"scrollbarTrack": {"slice": 32, "scale": 0.28, "filter": "linear"}},
    "badge": {"badge": {"slice": [0, 34, 0, 34], "scale": 0.5, "padding": [2, 4, 4, 4], "colorize": True, "filter": "linear"}},
    "row": {
        "cellSelected": {"slice": 30, "scale": 0.5, "filter": "linear"},
        "cellHover": {"slice": 30, "scale": 0.5, "tint": "#59FFFFFF", "filter": "linear"},
        "cellPressed": {"slice": 30, "scale": 0.5, "tint": "#FFB8C4E6", "filter": "linear"},
        "slotHighlighted": {"slice": 30, "scale": 0.5, "filter": "linear"},
    },
}

DOCUMENT = {
    "name": "fantasy",
    "imageFilter": "linear",
    "fontFiles": {"lilitaOne": "fonts/lilita-one-regular.ttf"},
    "fonts": {
        "body": {"font": "lilitaOne", "size": 24},
        "caption": {"font": "lilitaOne", "size": 19},
        "button": {"font": "lilitaOne", "size": 26},
        "heading": {"font": "lilitaOne", "size": 31},
        "title": {"font": "lilitaOne", "size": 49},
    },
    "colors": {
        "window": "#FF1E2138", "panel": "#FF2F3352", "raised": "#FF3A4066", "tooltip": "#F21E2138", "onTooltip": "#FFFFFFFF",
        "overlay": "#A0101224", "shadow": "#66080A18", "hover": "#1AFFFFFF", "pressed": "#590B0D1E", "selection": "#552F86F6",
        "focus": "#FFFFC93C", "caret": "#FFFFC93C", "border": "#FF4A5078", "borderStrong": "#FF15172A",
        "scrollbar": "#FFB9C0E0", "scrollbarHover": "#FFFFFFFF", "track": "#FF1E2138", "knob": "#FFFFFFFF",
        "text": "#FFFFFFFF", "textMuted": "#FFB9C0E0", "textDisabled": "#FF7A80A0",
        "accent": "#FF2F86F6", "accentHover": "#FF4A9AFF", "accentStrong": "#FF1F6AD0", "onAccent": "#FFFFFFFF",
        "accentBackground": "#FF24407A", "accentText": "#FF7DB8FF",
        "success": "#FF4CD45A", "onSuccess": "#FFFFFFFF", "successBackground": "#FF1F5A3A", "successText": "#FF7FE88A",
        "warning": "#FFFFB436", "onWarning": "#FF15172A", "warningBackground": "#FF6A4A1C", "warningText": "#FFFFCB70",
        "danger": "#FFF0473E", "dangerHover": "#FFFF6159", "dangerStrong": "#FFC8302A", "onDanger": "#FFFFFFFF",
        "dangerBackground": "#FF6E2430", "dangerText": "#FFFF8A84",
        "information": "#FF2EA8FF", "onInformation": "#FFFFFFFF", "informationBackground": "#FF1C4A70", "informationText": "#FF8FD0FF",
    },
    "metrics": {
        "controlHeight": 72, "controlRadius": 20, "controlPaddingX": 30, "controlPaddingY": 12, "itemSpacing": 16, "panelPadding": 10,
        "focusWidth": 4, "focusGap": 4, "progressHeight": 34, "choiceSize": 46, "sliderTrackHeight": 28, "sliderKnobSize": 50,
        "toggleWidth": 96, "toggleHeight": 48, "toggleKnobInset": 3, "iconSize": 44, "listRowHeight": 72, "windowTitleHeight": 64,
        "scrollbarSize": 18, "scrollbarGap": 10, "scrollbarInset": 4, "chipHeight": 52, "tabPaddingX": 32, "badgePaddingX": 14,
        "badgePaddingY": 4, "panelRadius": 24, "cellRadius": 18, "rowPadding": 18, "menuPadding": 10, "toastWidth": 600,
    },
}


def solidify(image: Image) -> Image:
    """Makes the pixels that the removal of the background left nearly opaque fully opaque, so no panel shows what lies behind it."""
    for start in range(3, len(image.pixels), 4):
        if image.pixels[start] >= OPAQUE:
            image.pixels[start] = 255
    return image


def pack(folder: Path) -> tuple[Image, dict[str, list[int]]]:
    """Drops the pieces from the tallest down onto the skyline of the atlas, each where it rests lowest, and returns the atlas with the region of every piece."""
    pieces = {name: solidify(read(folder / f"{name}.png")) for name in PIECES}
    order = sorted(pieces, key=lambda name: (-pieces[name].height, -pieces[name].width, name))
    skyline = [0] * ATLAS_WIDTH
    regions: dict[str, list[int]] = {}
    for name in order:
        width = pieces[name].width + GAP
        x = min(range(0, ATLAS_WIDTH - width + 1, GAP), key=lambda left: (max(skyline[left : left + width]), left))
        y = max(skyline[x : x + width])
        skyline[x : x + width] = [y + pieces[name].height + GAP] * width
        regions[name] = [x, y, pieces[name].width, pieces[name].height]

    atlas = Image.blank(ATLAS_WIDTH, max(skyline) - GAP)
    for name, (left, top, _, _) in regions.items():
        atlas.paste(pieces[name], left, top)
    return atlas, regions


def halve(image: Image) -> Image:
    """Shrinks an image to half its size, averaging each square of four pixels with their colors weighted by their alpha, so edges keep no dark fringe."""
    result = Image.blank(image.width // 2, image.height // 2)
    for y in range(result.height):
        for x in range(result.width):
            sums = [0, 0, 0, 0]
            for dy in (0, 1):
                for dx in (0, 1):
                    start = ((y * 2 + dy) * image.width + x * 2 + dx) * 4
                    alpha = image.pixels[start + 3]
                    for channel in range(3):
                        sums[channel] += image.pixels[start + channel] * alpha
                    sums[3] += alpha
            start = (y * result.width + x) * 4
            if sums[3] > 0:
                result.pixels[start : start + 4] = bytes([round(sums[0] / sums[3]), round(sums[1] / sums[3]), round(sums[2] / sums[3]), round(sums[3] / 4)])
    return result


def cut_icons(sheet: Image) -> dict[str, Image]:
    """Cuts the sheet into its cells and halves each one into an icon."""
    cell = sheet.width // ICON_COLUMNS
    return {name: halve(solidify(sheet.crop((index % ICON_COLUMNS) * cell, (index // ICON_COLUMNS) * cell, cell, cell))) for index, name in enumerate(ICONS)}


def document(theme: dict) -> str:
    """Writes the theme file with one line per entry of each section, the way theme files are written by hand."""
    lines = []
    for key, value in theme.items():
        if not isinstance(value, dict):
            lines.append(f"    {json.dumps(key)}: {json.dumps(value)}")
            continue
        entries = [f"        {json.dumps(name)}: {json.dumps(entry, separators=(', ', ': '))}" for name, entry in value.items()]
        lines.append(f"    {json.dumps(key)}: {{\n" + ",\n".join(entries) + "\n    }")
    return "{\n" + ",\n".join(lines) + "\n}\n"


def main() -> None:
    if len(sys.argv) != 2:
        sys.exit("Name the folder of the pieces, such as \"python3 pack-fantasy-theme.py pieces\".")
    atlas, regions = pack(Path(sys.argv[1]))
    surfaces = {}
    for name, uses in PIECES.items():
        for surface, values in uses.items():
            surfaces[surface] = {"image": ATLAS, "source": regions[name], **values}
    write(atlas, THEME / "atlas.png")
    for name, icon in cut_icons(read(Path(sys.argv[1]) / "icons.png")).items():
        write(icon, THEME / "icons" / f"{name}.png")
    (THEME / "theme.json").write_text(document({**DOCUMENT, "surfaces": dict(sorted(surfaces.items()))}))


if __name__ == "__main__":
    main()
