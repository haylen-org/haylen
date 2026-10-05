"""The pixel art of the interface tests: item icons, landscapes for pictures and carousel pages, avatars and the wooden sign of the image button."""

from __future__ import annotations

import math
import random

from canvas import Canvas, rgb, shade

GOLD = rgb("#f2c14e")
STEEL = rgb("#cfd8dc")
BROWN = rgb("#7a4a24")
OUTLINE = rgb("#2b1d12")


def star_points(cx: float, cy: float, outer: float, inner: float, count: int = 5) -> list[tuple[float, float]]:
    return [(cx + math.cos(-math.pi / 2 + index * math.pi / count) * (outer if index % 2 == 0 else inner), cy + math.sin(-math.pi / 2 + index * math.pi / count) * (outer if index % 2 == 0 else inner)) for index in range(count * 2)]


def sword(c: Canvas) -> None:
    c.line(9, 23, 25, 7, 5, OUTLINE)
    c.line(9, 23, 25, 7, 3, STEEL)
    c.line(11, 20, 24, 7, 1, rgb("#ffffff"))
    c.line(8, 17, 15, 24, 4, GOLD)
    c.line(5, 27, 9, 23, 4, BROWN)
    c.ellipse(5, 27, 2.5, 2.5, GOLD)


def shield(c: Canvas) -> None:
    outline = [(5, 4), (27, 4), (27, 15), (16, 29), (5, 15)]
    c.polygon(outline, OUTLINE)
    c.polygon([(7, 6), (25, 6), (25, 15), (16, 26), (7, 15)], rgb("#3f6fb5"))
    c.rect(15, 7, 3, 18, GOLD)
    c.rect(8, 12, 17, 3, GOLD)


def potion(c: Canvas) -> None:
    c.ellipse(16, 20, 10, 9.5, OUTLINE)
    c.ellipse(16, 20, 8.5, 8, rgb("#dfe9f2"))
    c.ellipse(16, 22, 8, 6, rgb("#d6336c"))
    c.rect(12, 5, 8, 8, OUTLINE)
    c.rect(13, 6, 6, 7, rgb("#dfe9f2"))
    c.rect(12, 3, 8, 4, BROWN)
    c.ellipse(12.5, 18, 2, 2.5, rgb("#ffffff", 180))


def gem(c: Canvas) -> None:
    c.polygon([(16, 3), (28, 12), (16, 29), (4, 12)], OUTLINE)
    c.polygon([(16, 5), (26, 12), (16, 26), (6, 12)], rgb("#26c6da"))
    c.polygon([(16, 5), (26, 12), (16, 12)], rgb("#80deea"))
    c.polygon([(6, 12), (16, 12), (16, 26)], rgb("#0097a7"))


def coin(c: Canvas) -> None:
    c.ellipse(16, 16, 12, 12, OUTLINE)
    c.ellipse(16, 16, 10.5, 10.5, GOLD)
    c.ellipse(16, 16, 7, 7, shade(GOLD, 0.8))
    c.rect(15, 11, 2, 10, rgb("#fff3c4"))
    c.ellipse(12, 11, 2, 2, rgb("#ffffff", 200))


def heart(c: Canvas) -> None:
    for color, grow in ((OUTLINE, 1.5), (rgb("#e53935"), 0)):
        c.ellipse(11, 12, 6 + grow, 6 + grow, color)
        c.ellipse(21, 12, 6 + grow, 6 + grow, color)
        c.polygon([(4.5 - grow, 14), (27.5 + grow, 14), (16, 27 + grow)], color)
    c.ellipse(10, 10, 2, 2, rgb("#ffffff", 200))


def key(c: Canvas) -> None:
    c.ellipse(10, 11, 7, 7, OUTLINE)
    c.ellipse(10, 11, 5.5, 5.5, GOLD)
    c.ellipse(10, 11, 2.5, 2.5, OUTLINE)
    c.line(14, 15, 26, 27, 5, OUTLINE)
    c.line(14, 15, 26, 27, 3, GOLD)
    c.line(21, 22, 18, 25, 3, GOLD)
    c.line(25, 26, 22, 29, 3, GOLD)


def apple(c: Canvas) -> None:
    c.ellipse(16, 19, 11, 10, OUTLINE)
    c.ellipse(16, 19, 9.5, 8.5, rgb("#e53935"))
    c.ellipse(12, 16, 2.5, 3, rgb("#ffffff", 170))
    c.line(16, 11, 17, 5, 2, BROWN)
    c.ellipse(21, 7, 4, 2, rgb("#43a047"))


def star(c: Canvas) -> None:
    c.polygon(star_points(16, 17, 14, 6.5), OUTLINE)
    c.polygon(star_points(16, 17, 11.5, 5), rgb("#ffd54f"))


def gear(c: Canvas) -> None:
    for index in range(8):
        angle = index * math.pi / 4
        c.line(16, 16, 16 + math.cos(angle) * 13, 16 + math.sin(angle) * 13, 5, OUTLINE)
        c.line(16, 16, 16 + math.cos(angle) * 12, 16 + math.sin(angle) * 12, 3, rgb("#90a4ae"))
    c.ellipse(16, 16, 9, 9, OUTLINE)
    c.ellipse(16, 16, 7.5, 7.5, rgb("#90a4ae"))
    c.ellipse(16, 16, 3, 3, OUTLINE)


def hammer(c: Canvas) -> None:
    c.line(8, 27, 19, 12, 5, OUTLINE)
    c.line(8, 27, 19, 12, 3, BROWN)
    c.polygon([(12, 6), (22, 2), (29, 14), (19, 18)], OUTLINE)
    c.polygon([(13.5, 6.5), (21.5, 3.5), (27, 13), (19.5, 16)], rgb("#90a4ae"))


def fish(c: Canvas) -> None:
    c.polygon([(4, 9), (10, 16), (4, 23)], OUTLINE)
    c.polygon([(5, 11), (9, 16), (5, 21)], rgb("#26a69a"))
    c.ellipse(18, 16, 11, 7, OUTLINE)
    c.ellipse(18, 16, 9.5, 5.5, rgb("#26a69a"))
    c.ellipse(18, 18, 7, 2.5, rgb("#80cbc4"))
    c.ellipse(23, 14, 1.5, 1.5, OUTLINE)


def flag(c: Canvas) -> None:
    c.rect(5, 3, 3, 27, OUTLINE)
    c.polygon([(7, 3), (29, 8), (7, 17)], OUTLINE)
    c.polygon([(8, 5), (25, 8.5), (8, 14.5)], rgb("#ffffff"))


ICONS = {"sword": sword, "shield": shield, "potion": potion, "gem": gem, "coin": coin, "heart": heart, "key": key, "apple": apple, "star": star, "gear": gear, "hammer": hammer, "fish": fish, "flag": flag}


def icon(name: str) -> Canvas:
    canvas = Canvas(32, 32)
    ICONS[name](canvas)
    return canvas


PALETTES = {
    "day": {"top": "#4fa3e0", "bottom": "#bfe6ff", "sun": "#fff3b0", "far": "#6d8fb3", "near": "#3f7a57", "sea": "#2e86c1", "sand": "#f0d58c"},
    "dusk": {"top": "#3b2a63", "bottom": "#f28e5b", "sun": "#ffcf6b", "far": "#6b4e7a", "near": "#3d3553", "sea": "#5a4c8c", "sand": "#c99a6b"},
    "night": {"top": "#070b1f", "bottom": "#23305e", "sun": "#e8ecf7", "far": "#2c3a66", "near": "#18223f", "sea": "#16244a", "sand": "#5d5a6e"},
}


def landscape(name: str, width: int = 320, height: int = 180) -> Canvas:
    colors = {key: rgb(value) for key, value in PALETTES[name].items()}
    canvas = Canvas(width, height)
    canvas.gradient(0, 0, width, height, colors["top"], colors["bottom"])
    if name == "night":
        canvas.speckle(0, 0, width, height // 2, [rgb("#ffffff"), rgb("#ffffff", 140)], 90, 7)
    canvas.ellipse(width * 0.75, height * 0.3, 16, 16, colors["sun"])
    rng = random.Random(len(name))
    for layer, base in ((colors["far"], 0.55), (colors["near"], 0.68)):
        points = [(0, height)]
        for x in range(0, width + 20, 20):
            points.append((x, height * base - rng.random() * height * 0.18))
        points.append((width, height))
        canvas.polygon(points, layer)
    canvas.rect(0, round(height * 0.72), width, height - round(height * 0.72), colors["sea"])
    for row in range(round(height * 0.75), height, 6):
        for x in range(rng.randrange(8), width, 23):
            canvas.rect(x, row, 8, 1, shade(colors["sea"], 1.3))
    canvas.ellipse(width * 0.3, height * 0.8, 44, 9, colors["sand"])
    canvas.line(width * 0.3, height * 0.8, width * 0.3 + 6, height * 0.56, 3, rgb("#6d4c2f"))
    for angle in (-2.6, -2.0, -1.1, -0.5):
        canvas.line(width * 0.3 + 6, height * 0.56, width * 0.3 + 6 + math.cos(angle) * 18, height * 0.56 + math.sin(angle) * 8 + 6, 3, rgb("#2e7d32"))
    return canvas


def avatar(skin: str, hair: str, background: str) -> Canvas:
    canvas = Canvas(64, 64)
    canvas.rect(0, 0, 64, 64, rgb(background))
    canvas.ellipse(32, 66, 24, 18, rgb("#37474f"))
    canvas.ellipse(32, 30, 15, 17, rgb(skin))
    canvas.ellipse(32, 17, 17, 9, rgb(hair))
    canvas.rect(16, 16, 5, 16, rgb(hair))
    canvas.rect(43, 16, 5, 16, rgb(hair))
    canvas.rect(25, 29, 4, 4, OUTLINE)
    canvas.rect(35, 29, 4, 4, OUTLINE)
    canvas.rect(28, 39, 8, 2, shade(rgb(skin), 0.7))
    return canvas


def sign(tone: float, drop: int) -> Canvas:
    canvas = Canvas(160, 64)
    wood = shade(rgb("#a0632f"), tone)
    canvas.rounded(4, 6 + drop, 152, 52, 8, OUTLINE)
    canvas.rounded(7, 9 + drop, 146, 46, 6, wood)
    for row in range(16 + drop, 52 + drop, 7):
        canvas.rect(12, row, 136, 1, shade(wood, 0.85))
    canvas.rect(12, 10 + drop, 136, 2, shade(wood, 1.25))
    for x in (16, 144):
        canvas.ellipse(x, 32 + drop, 3, 3, rgb("#cfd8dc"))
    return canvas
