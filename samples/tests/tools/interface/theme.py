"""The parchment theme of the interface tests: one atlas of pixel-art frames and the theme file whose surfaces cut it into nine-slices."""

from __future__ import annotations

from canvas import Canvas, Color, mix, rgb, shade

INK = rgb("#3b2614")
WOOD = rgb("#9a5b2e")
WOOD_DARK = rgb("#4a2a14")
PAPER = rgb("#ead9ae")
PAPER_DARK = rgb("#c9b07a")
TEAL = rgb("#2f8f83")
RED = rgb("#b53a2e")
GOLD = rgb("#e3a92b")
WHITE = rgb("#ffffff")

ATLAS_WIDTH = 256
ATLAS_HEIGHT = 144


class Atlas:
    """Places frames on shelves of the atlas and remembers the region of every surface."""

    def __init__(self) -> None:
        self.canvas = Canvas(ATLAS_WIDTH, ATLAS_HEIGHT)
        self.x, self.y, self.shelf = 0, 0, 0
        self.surfaces: dict[str, dict] = {}

    def place(self, width: int, height: int) -> tuple[int, int]:
        if self.x + width > ATLAS_WIDTH:
            self.x, self.y, self.shelf = 0, self.y + self.shelf + 2, 0
        origin = (self.x, self.y)
        self.x += width + 2
        self.shelf = max(self.shelf, height)
        return origin

    def add(self, names: list[str], width: int, height: int, painter, **surface) -> None:
        x, y = self.place(width, height)
        painter(self.canvas, x, y, width, height)
        for name in names:
            self.surfaces[name] = {"image": "interface/themes/parchment.png", "source": [x, y, width, height], **surface}


def frame(canvas: Canvas, x: int, y: int, width: int, height: int, fill: Color, border: Color, radius: int = 5, bevel: int = 2, pressed: bool = False, grain: bool = False) -> None:
    """A rounded frame with a dark border and a light bevel on top, or at the bottom when pressed."""
    canvas.rounded(x, y, width, height, radius, border)
    canvas.rounded(x + 2, y + 2, width - 4, height - 4, max(1, radius - 2), fill)
    light, dark = shade(fill, 1.25), shade(fill, 0.72)
    top, bottom = (dark, light) if pressed else (light, dark)
    canvas.rect(x + 2 + radius // 2, y + 2, width - 4 - radius, bevel, top)
    canvas.rect(x + 2 + radius // 2, y + height - 2 - bevel, width - 4 - radius, bevel, bottom)
    if grain:
        for row in range(y + 6, y + height - 6, 5):
            canvas.rect(x + 4, row, width - 8, 1, shade(fill, 0.88))


def paper(fill: Color = PAPER, border: Color = WOOD_DARK):
    def paint(canvas: Canvas, x: int, y: int, width: int, height: int) -> None:
        frame(canvas, x, y, width, height, fill, border, radius=4)
        canvas.speckle(x + 4, y + 4, width - 8, height - 8, [shade(fill, 0.93), shade(fill, 1.05)], width * height // 10, x * 31 + y)
    return paint


def wood(fill: Color, pressed: bool = False):
    def paint(canvas: Canvas, x: int, y: int, width: int, height: int) -> None:
        frame(canvas, x, y, width, height, fill, WOOD_DARK, radius=6, bevel=3, pressed=pressed, grain=True)
    return paint


def framed(canvas: Canvas, x: int, y: int, width: int, height: int) -> None:
    """A thick wooden frame around parchment, for dialogs and windows."""
    frame(canvas, x, y, width, height, WOOD, WOOD_DARK, radius=6, bevel=2, grain=True)
    frame(canvas, x + 10, y + 10, width - 20, height - 20, PAPER, WOOD_DARK, radius=3, bevel=1)
    for cx, cy in ((x + 5, y + 5), (x + width - 6, y + 5), (x + 5, y + height - 6), (x + width - 6, y + height - 6)):
        canvas.ellipse(cx + 0.5, cy + 0.5, 2, 2, GOLD)


def inset(fill: Color, border: Color):
    def paint(canvas: Canvas, x: int, y: int, width: int, height: int) -> None:
        canvas.rounded(x, y, width, height, 4, border)
        canvas.rounded(x + 2, y + 2, width - 4, height - 4, 3, fill)
        canvas.rect(x + 3, y + 2, width - 6, 2, shade(fill, 0.8))
    return paint


def check(checked: bool):
    def paint(canvas: Canvas, x: int, y: int, width: int, height: int) -> None:
        inset(TEAL if checked else PAPER, WOOD_DARK)(canvas, x, y, width, height)
        if checked:
            canvas.line(x + 5, y + 10, x + 8.5, y + 14, 2.2, WHITE)
            canvas.line(x + 8.5, y + 14, x + 15, y + 6, 2.2, WHITE)
    return paint


def knob(canvas: Canvas, x: int, y: int, width: int, height: int) -> None:
    canvas.ellipse(x + width / 2, y + height / 2, width / 2, height / 2, WOOD_DARK)
    canvas.ellipse(x + width / 2, y + height / 2, width / 2 - 1.5, height / 2 - 1.5, WOOD)
    canvas.ellipse(x + width / 2 - 1.5, y + height / 2 - 1.5, 2.5, 2, shade(WOOD, 1.4))


def pill(fill: Color, border: Color):
    def paint(canvas: Canvas, x: int, y: int, width: int, height: int) -> None:
        canvas.rounded(x, y, width, height, height // 2, border)
        canvas.rounded(x + 2, y + 2, width - 4, height - 4, height // 2 - 2, fill)
        canvas.rect(x + height // 2, y + 2, width - height, 1, shade(fill, 1.2))
    return paint


def tab(selected: bool):
    def paint(canvas: Canvas, x: int, y: int, width: int, height: int) -> None:
        fill = PAPER if selected else PAPER_DARK
        canvas.rounded(x, y, width, height + 6, 5, WOOD_DARK)
        canvas.rounded(x + 2, y + 2, width - 4, height + 2, 4, fill)
        if selected:
            canvas.rect(x + 4, y + 2, width - 8, 2, GOLD)
    return paint


def slot(highlighted: bool):
    def paint(canvas: Canvas, x: int, y: int, width: int, height: int) -> None:
        inset(shade(WOOD_DARK, 1.3), GOLD if highlighted else WOOD_DARK)(canvas, x, y, width, height)
    return paint


def banner(canvas: Canvas, x: int, y: int, width: int, height: int) -> None:
    folds = [(x, y + 4), (x + 14, y + 4), (x + 14, y + height - 2), (x, y + height - 2), (x + 6, y + height / 2 + 1)]
    canvas.polygon(folds, shade(RED, 0.7))
    canvas.polygon([(x + width - px + x, py) for px, py in folds], shade(RED, 0.7))
    canvas.rect(x + 10, y, width - 20, height - 6, WOOD_DARK)
    canvas.rect(x + 11, y + 1, width - 22, height - 8, RED)
    canvas.rect(x + 11, y + 2, width - 22, 2, shade(RED, 1.3))


def build() -> tuple[Canvas, dict]:
    atlas = Atlas()
    atlas.add(["panel"], 32, 32, paper(), slice=10, scale=2, fill="tile", padding=4)
    atlas.add(["card", "menu"], 32, 32, paper(mix(PAPER, WHITE, 0.35), WOOD), slice=10, scale=2, fill="tile", padding=4)
    atlas.add(["dialog", "window"], 48, 48, framed, slice=16, scale=2, padding=8)
    atlas.add(["tooltip", "toast"], 24, 24, paper(shade(WOOD_DARK, 1.1), INK), slice=8, scale=2, padding=4)
    atlas.add(["button"], 32, 32, wood(WOOD), slice=10, scale=2)
    atlas.add(["buttonHover"], 32, 32, wood(shade(WOOD, 1.15)), slice=10, scale=2)
    atlas.add(["buttonPressed"], 32, 32, wood(shade(WOOD, 0.85), True), slice=10, scale=2)
    atlas.add(["buttonPrimary"], 32, 32, wood(TEAL), slice=10, scale=2)
    atlas.add(["buttonPrimaryHover"], 32, 32, wood(shade(TEAL, 1.15)), slice=10, scale=2)
    atlas.add(["buttonPrimaryPressed"], 32, 32, wood(shade(TEAL, 0.85), True), slice=10, scale=2)
    atlas.add(["buttonDestructive"], 32, 32, wood(RED), slice=10, scale=2)
    atlas.add(["buttonDestructiveHover"], 32, 32, wood(shade(RED, 1.15)), slice=10, scale=2)
    atlas.add(["buttonDestructivePressed"], 32, 32, wood(shade(RED, 0.85), True), slice=10, scale=2)
    atlas.add(["field"], 24, 24, inset(mix(PAPER, WHITE, 0.4), WOOD_DARK), slice=8, scale=2)
    atlas.add(["fieldFocused"], 24, 24, inset(mix(PAPER, WHITE, 0.6), GOLD), slice=8, scale=2)
    atlas.add(["check"], 20, 20, check(False))
    atlas.add(["checkChecked"], 20, 20, check(True))
    atlas.add(["track"], 24, 12, inset(shade(WOOD_DARK, 1.2), WOOD_DARK), slice=5, scale=2, padding=4)
    atlas.add(["segment"], 24, 24, inset(PAPER_DARK, WOOD_DARK), slice=8, scale=2)
    atlas.add(["trackFill"], 24, 12, pill(rgb("#f4f4f4"), rgb("#bdbdbd")), slice=5, scale=2, colorize=True)
    atlas.add(["knob"], 16, 16, knob)
    atlas.add(["tab"], 32, 24, tab(False), slice=[8, 8, 2, 8], scale=2)
    atlas.add(["tabSelected"], 32, 24, tab(True), slice=[8, 8, 2, 8], scale=2)
    atlas.add(["chip", "badge"], 32, 20, pill(PAPER_DARK, WOOD_DARK), slice=[0, 10, 0, 10], scale=2)
    atlas.add(["chipSelected"], 32, 20, pill(mix(TEAL, WHITE, 0.6), WOOD_DARK), slice=[0, 10, 0, 10], scale=2)
    atlas.add(["segmentSelected"], 32, 20, pill(TEAL, WOOD_DARK), slice=[0, 10, 0, 10], scale=2)
    atlas.add(["slot"], 24, 24, slot(False), slice=7, scale=2)
    atlas.add(["slotHighlighted"], 24, 24, slot(True), slice=7, scale=2)
    atlas.add(["banner"], 64, 28, banner, slice=[0, 16, 0, 16], scale=2, padding=[4, 24, 12, 24])

    document = {
        "name": "parchment",
        "colors": {
            "window": "#ffead9ae", "panel": "#ffead9ae", "raised": "#fff3e6c4", "tooltip": "#f23b2614", "overlay": "#99201008",
            "hover": "#1a3b2614", "pressed": "#333b2614", "selection": "#592f8f83", "focus": "#ffe3a92b",
            "border": "#ff9a5b2e", "borderStrong": "#ff4a2a14", "scrollbar": "#ff9a5b2e", "scrollbarHover": "#ff4a2a14",
            "text": "#ff3b2614", "textMuted": "#ff7a5a3a", "textDisabled": "#ffa89070", "onTooltip": "#fff3e6c4",
            "accent": "#ff2f8f83", "accentHover": "#ff3aa596", "accentStrong": "#ff26756b", "onAccent": "#ffffffff",
            "accentBackground": "#332f8f83", "accentText": "#ff1f6f66",
        },
        "metrics": {
            "controlHeight": 72, "controlRadius": 8, "panelPadding": 20, "choiceSize": 40, "sliderTrackHeight": 24, "sliderKnobSize": 32,
            "toggleWidth": 80, "toggleHeight": 40, "progressHeight": 28, "focusWidth": 4, "slotSize": 96,
        },
        "fontFiles": {"lilitaOne": "fonts/lilita_one_regular.ttf", "firaSans": "fonts/fira_sans_regular.otf"},
        "fonts": {
            "title": {"font": "lilitaOne", "size": 60}, "heading": {"font": "lilitaOne", "size": 40},
            "button": {"font": "lilitaOne", "size": 28}, "body": {"font": "firaSans", "size": 30},
            "caption": {"font": "firaSans", "size": 24},
        },
        "surfaces": atlas.surfaces,
    }
    return atlas.canvas, document
