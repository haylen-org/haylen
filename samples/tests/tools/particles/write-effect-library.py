"""Writes the effect files and the catalog of the particle library of the test project from the layers of each effect.

Every effect is a list of layers, such as a soft halo, a flare, a ring, sparkles, rising icons, puffs or debris, built by the functions of this tool from the pieces of the atlas `content/particles/library/textures/effects.json`, which the tool `pack-particle-textures.py` packs. An effect of one layer becomes a file of one emitter and the others composite files of inline emitters, one folder for each category, and `catalog.json` lists them with their name, style, kind and the view they suit. Run it with `python3 samples/tests/tools/particles/write-effect-library.py` after packing the atlas. The pixel art effects draw from `pixel.png` and keep their own files, which the tool lists in the catalog as they are.
"""

from __future__ import annotations

import json
import sys
from dataclasses import dataclass, field
from pathlib import Path

LIBRARY = Path(__file__).resolve().parents[2] / "content" / "particles" / "library"
TEXTURES = LIBRARY / "textures"
ATLAS = json.loads((TEXTURES / "effects.json").read_text())["frames"]
TAU = 6.2832
QUARTER = 1.5708
# Rings, circles and swirls on the ground lie on a plane tilted by this angle.
GROUND = 1.15


def frames(piece: str) -> list[list[int]]:
    """Returns the frame of a piece, or the frames `<piece>-1` to `<piece>-<count>` of a sheet."""
    names = [piece] if piece in ATLAS else [f"{piece}-{index}" for index in range(1, 9) if f"{piece}-{index}" in ATLAS]
    if not names:
        sys.exit(f'The atlas has no piece "{piece}".')
    return [[ATLAS[name]["frame"][key] for key in ("x", "y", "w", "h")] for name in names]


def color(rgb: str, alpha: float = 1.0) -> str:
    return f"#{round(alpha * 255):02X}{rgb.lstrip('#').upper()}"


def fade(rgb: str, peak: float = 1.0, rise: float = 0.15, hold: float | None = None) -> dict:
    """Colors that rise to the peak alpha early in the life, optionally hold it, and fade out by the end."""
    if hold is None:
        return {"colors": [color(rgb, 0), color(rgb, peak), color(rgb, 0)], "colorTimes": [0, rise, 1]}
    return {"colors": [color(rgb, 0), color(rgb, peak), color(rgb, peak), color(rgb, 0)], "colorTimes": [0, rise, hold, 1]}


def ramp(*stops: tuple[float, str, float]) -> dict:
    """Colors at their times, each stop a time, a color and an alpha."""
    return {"colors": [color(rgb, alpha) for _, rgb, alpha in stops], "colorTimes": [time for time, _, _ in stops]}


def emitter(piece: str, **options) -> dict:
    layer = {"texture": "../textures/effects.png", "filter": "linear", "frames": frames(piece), "blend": "additive"}
    layer.update(options)
    return layer


def once(count: int | list[int], duration: float = 0.05, delay: float = 0.0, **extra) -> dict:
    timing = {"rate": 0, "bursts": [{"time": 0, "count": count, **extra}], "duration": duration}
    if delay:
        timing["delay"] = delay
    return timing


def steady(rate: float, prewarm: float = 0.0) -> dict:
    timing = {"rate": rate}
    if prewarm:
        timing["prewarm"] = prewarm
    return timing


def still(size: float, end: float | None = None, curve: object = "quadOut") -> dict:
    """A particle that stays where it is born and grows from its size to its end size."""
    return {"speed": 0, "startSize": size, "endSize": size if end is None else end, "sizeCurve": curve}


def pop(size: float, settle: float = 0.25, speed: object = 0) -> dict:
    """A particle that pops to its full size early and shrinks to a share of it."""
    return {"speed": speed, "startSize": 0, "endSize": size, "sizeCurve": {"points": [0, 1.15, 1, settle]}}


@dataclass
class Effect:
    category: str
    id: str
    name: str
    style: str
    kind: str
    view: str
    layers: list[tuple[str, dict]] = field(default_factory=list)


EFFECTS: list[Effect] = []


def effect(category: str, id: str, name: str, style: str, kind: str, *layers: tuple[str, dict], view: str | None = None) -> None:
    """Adds an effect, whose view is `top` when something of it lies on the ground, `side` for the floor of a side view and `any` otherwise."""
    if view is None:
        view = "top" if any("tilt" in layer for _, layer in layers) else "any"
    EFFECTS.append(Effect(category, id, name, style, kind, view, list(layers)))


def halo(rgb: str, size: float, life: float, alpha: float = 0.4, timing: dict | None = None, **extra) -> dict:
    """A large soft glow behind an effect, which gives the light of the effect to everything around it."""
    glow = {"colors": [color(rgb, 0), color(rgb, alpha), color(rgb, alpha * 0.35), color(rgb, 0)], "colorTimes": [0, 0.12, 0.45, 1]}
    return emitter("soft-dot", **(timing or once(1)), maxParticles=4, lifetime=life, **still(size * 0.7, size), **glow, **extra)


def flare(piece: str, rgb: str, size: float, life: float, timing: dict | None = None, rotation: object = 0, spin: object = 0, settle: float = 0.25, alpha: float = 1.0, **extra) -> dict:
    return emitter(piece, **(timing or once(1)), maxParticles=8, lifetime=life, **pop(size, settle), rotation=rotation, spin=spin, **fade(rgb, alpha, 0.1), **extra)


def ring(rgb: str, start: float, end: float, life: float, piece: str = "ring-thin", timing: dict | None = None, alpha: float = 1.0, tilt: float = 0.0, **extra) -> dict:
    layer = emitter(piece, **(timing or once(1)), maxParticles=8, lifetime=life, **still(start, end, "quadOut"), **fade(rgb, alpha, 0.08), **extra)
    if tilt:
        layer["tilt"] = tilt
    return layer


def burst(piece: str, rgb: str, count: int, speed: list[float], life: list[float], size: list[float], timing: dict | None = None, alpha: float = 1.0, **extra) -> dict:
    """Pieces thrown out in every direction that slow down, shrink and fade."""
    options = {"maxParticles": count * 2, "lifetime": life, "speed": speed, "spread": TAU, "damping": 3.5, "startSize": size, "endSize": 0, "sizeCurve": "quadIn", "rotation": [0, TAU], **fade(rgb, alpha, 0.05)}
    options.update(extra)
    return emitter(piece, **(timing or once(count)), **options)


def rising(piece: str, rgb: str, rate: float, width: float, speed: list[float], life: list[float], size: list[float], alpha: float = 1.0, **extra) -> dict:
    """Pieces that rise from a strip and fade, such as sparkles and status icons."""
    options = {"maxParticles": 64, "lifetime": life, "speed": speed, "direction": -QUARTER, "spread": 0.3, "shape": "ellipse", "shapeSize": [width, width * 0.3], "startSize": size, "endSizeScale": 0.6, "sizeCurve": {"points": [0, 1.2, 1]}, **fade(rgb, alpha, 0.15, 0.6)}
    options.update(extra)
    return emitter(piece, **steady(rate, max(life) if isinstance(life, list) else life), **options)


def ground(piece: str, rgb: str, size: float, life: float, rate: float, spin: float, alpha: float = 1.0, **extra) -> dict:
    """A circle that lies on the ground and keeps turning, crossfading with the next one so it never blinks."""
    options = {"maxParticles": 6, "lifetime": life, **still(size), "spin": spin, "tilt": GROUND, "localSpace": True, **fade(rgb, alpha, 0.25, 0.75)}
    options.update(extra)
    return emitter(piece, **steady(rate, life), **options)


# Hits: a hot flare that pops at the point of contact, a light of the color of the hit around it and pieces that fly off.
effect("hits", "hit-light", "Light hit", "casual", "once",
       ("glow", halo("FFFFFF", 300, 0.32, 0.35)),
       ("flare", flare("flare", "FFFFFF", 190, 0.28, rotation=[-0.3, 0.3])),
       ("sparkles", burst("star-4point", "FFFFFF", 7, [160, 320], [0.35, 0.55], [16, 30])))
effect("hits", "hit-rays", "Ray hit", "casual", "once",
       ("glow", halo("FF9A2E", 360, 0.32, 0.45)),
       ("rays", flare("flash-burst", "FFC266", 280, 0.32, rotation=[0, TAU], spin=[-1, 1])),
       ("flare", flare("flare", "FFFFFF", 150, 0.22, rotation=[0, 0.8])),
       ("sparks", burst("spark-streak", "FFD08A", 10, [420, 700], [0.18, 0.32], [26, 40], alignToVelocity=True, stretch=0.04, rotation=0)))
effect("hits", "hit-ring", "Ring hit", "casual", "once",
       ("glow", halo("9B4DFF", 380, 0.32, 0.45)),
       ("ring", ring("F2D6FF", 50, 210, 0.38)),
       ("core", flare("soft-dot", "FFFFFF", 90, 0.2)),
       ("sparkles", burst("star-4point", "7FE8FF", 9, [180, 300], [0.4, 0.65], [14, 26])))
effect("hits", "hit-slash", "Slash hit", "casual", "once",
       ("glow", halo("FF2E4D", 340, 0.32, 0.4)),
       ("slash", flare("flare-thin", "FF5A70", 420, 0.26, rotation=-0.785, settle=0.1)),
       ("crescent", ring("FF8A9A", 70, 140, 0.3, piece="ring-blades", rotation=[0, TAU], spin=4)),
       ("core", flare("star-4point", "FFFFFF", 110, 0.22, rotation=-0.785)),
       ("sparkles", burst("star-4point", "FFB0BC", 6, [140, 260], [0.35, 0.5], [14, 24])))
effect("hits", "hit-energy", "Energy hit", "casual", "once",
       ("glow", halo("1E6BFF", 420, 0.32, 0.5)),
       ("spikes", flare("flare-thin", "4C8DFF", 460, 0.3, timing=once(3), rotation=[0, TAU], settle=0.05)),
       ("core", flare("flare", "DDEBFF", 170, 0.24, rotation=[-0.4, 0.4])),
       ("sparkles", burst("star-4point", "9CC4FF", 8, [200, 340], [0.3, 0.5], [12, 22])))
effect("hits", "hit-critical", "Critical hit", "casual", "once",
       ("glow", halo("FF3B1F", 460, 0.6, 0.5)),
       ("burst", flare("impact-star", "FFD24A", 300, 0.34, rotation=[0, TAU])),
       ("rays", flare("flash-burst", "FF7A3D", 420, 0.4, rotation=[0, TAU], spin=[-0.6, 0.6], alpha=0.8)),
       ("ring", ring("FFE7A8", 80, 340, 0.42, piece="ring-thick", alpha=0.85)),
       ("flare", flare("flare", "FFFFFF", 220, 0.24)),
       ("sparks", burst("spark-streak", "FFC95A", 14, [500, 900], [0.2, 0.36], [30, 46], alignToVelocity=True, stretch=0.05, rotation=0)))
effect("hits", "hit-claws", "Claw hit", "casual", "once",
       ("glow", halo("FF6A2E", 320, 0.32, 0.4)),
       ("claws", flare("claws", "FFB070", 240, 0.32, rotation=[-0.2, 0.2], settle=0.7)),
       ("flash", flare("flare", "FFFFFF", 140, 0.2, rotation=0.6)),
       ("chips", burst("spark-streak", "FFD0A0", 8, [300, 520], [0.2, 0.34], [20, 32], alignToVelocity=True, stretch=0.03, rotation=0)))
effect("hits", "sword-slash", "Sword slash", "casual", "once",
       ("glow", halo("7FE8FF", 300, 0.4, 0.3)),
       ("arc", emitter("slash-arc", **once(1), maxParticles=2, lifetime=0.3, speed=0, startSize=220, endSize=260, rotation=-2.2, spin=7, spinCurve="quadOut", **fade("DFFBFF", 1, 0.1))),
       ("trail", emitter("slash-arc", **once(1), maxParticles=2, lifetime=0.36, speed=0, startSize=240, endSize=290, rotation=-2.6, spin=7, spinCurve="quadOut", **fade("3CC8FF", 0.6, 0.15))),
       ("sparkles", burst("star-4point", "BFF4FF", 6, [140, 240], [0.3, 0.5], [12, 20])))

# Magic circles lie on the ground under a caster and keep turning while the spell lasts.
for circle_id, name, piece, rgb, light, spin in (
        ("circle-hexagram", "Hexagram circle", "circle-hexagram", "2E9BFF", "BDE6FF", 0.5),
        ("circle-star", "Star circle", "circle-star", "FFB52E", "FFE7A8", -0.45),
        ("circle-glyphs", "Glyph circle", "circle-glyphs", "36E07A", "C2FFD8", 0.6),
        ("circle-vortex", "Vortex circle", "circle-hooks", "FF3B5C", "FFC2CE", 2.2)):
    effect("magic-circles", circle_id, name, "casual", "loop",
           ("glow", halo(rgb, 420, 2.4, 0.35, timing=steady(1.2, 2.4), tilt=GROUND, localSpace=True)),
           ("disc", ground("disc", rgb, 300, 2.4, 1.2, 0, alpha=0.35)),
           ("circle", ground(piece, light, 300, 2.4, 1.2, spin)),
           ("ring", ground("ring-thin", rgb, 360, 2.4, 1.2, -spin * 0.6, alpha=0.6)),
           ("sparkles", rising("star-4point", light, 10, 150, [40, 90], [0.9, 1.5], [10, 20])))
effect("magic-circles", "circle-glow", "Glowing disc", "casual", "loop",
       ("glow", halo("FF9A1F", 440, 2.0, 0.4, timing=steady(1.5, 2.0), tilt=GROUND, localSpace=True)),
       ("disc", ground("disc", "FFB43C", 280, 2.0, 1.5, 0, alpha=0.8)),
       ("pulse", ring("FFD27A", 120, 340, 1.4, timing=steady(1.4, 1.4), tilt=GROUND, alpha=0.8, localSpace=True)))

# Buffs and auras stay around a character while they last.
effect("buffs", "buff-pillar", "Buff pillar", "casual", "loop",
       ("glow", halo("FFB52E", 380, 1.6, 0.35, timing=steady(1.5, 1.6), offset=[0, -60], localSpace=True)),
       ("base", ground("disc", "FFB52E", 220, 1.6, 1.5, 0, alpha=0.5)),
       ("bands", emitter("ring-thin", **steady(5, 1.2), maxParticles=16, lifetime=1.1, speed=[90, 110], direction=-QUARTER, spread=0, startSize=200, endSize=180, tilt=GROUND, localSpace=True, **fade("FFD27A", 0.75, 0.15))),
       ("sparkles", rising("star-4point", "FFF1B8", 12, 110, [60, 120], [0.8, 1.3], [10, 18])))
effect("buffs", "level-up", "Level up", "casual", "cycle",
       ("glow", halo("FFC93C", 460, 1.4, 0.5, timing={"rate": 0, "bursts": [{"time": 0, "count": 1}], "duration": 1.8, "loop": True})),
       ("flash", flare("flare", "FFFFFF", 260, 0.35, timing={"rate": 0, "bursts": [{"time": 0, "count": 1}], "duration": 1.8, "loop": True})),
       ("column", emitter("light-beam", rate=0, bursts=[{"time": 0, "count": 3}], duration=1.8, loop=True, maxParticles=6, lifetime=[0.9, 1.2], speed=0, shape="rectangle", shapeSize=[40, 0], startSize=300, endSize=340, aspect=2.4, offset=[0, -120], **fade("FFD24A", 0.8, 0.15))),
       ("ring", ring("FFE7A8", 80, 320, 0.7, timing={"rate": 0, "bursts": [{"time": 0, "count": 1}], "duration": 1.8, "loop": True}, tilt=GROUND)),
       ("stars", emitter("star-4point", rate=0, bursts=[{"time": 0.05, "count": 14}], duration=1.8, loop=True, maxParticles=32, lifetime=[0.9, 1.4], speed=[120, 260], direction=-QUARTER, spread=0.9, gravity=[0, 60], damping=1.2, startSize=[14, 30], endSize=0, sizeCurve="quadIn", spin=[-2, 2], **fade("FFF1B8", 1, 0.05))))
effect("buffs", "speed-up", "Speed up", "casual", "loop",
       ("glow", halo("2EC8C8", 340, 1.4, 0.3, timing=steady(1.5, 1.4), localSpace=True)),
       ("swirls", emitter("swirl", **steady(6, 1.0), maxParticles=12, lifetime=0.9, speed=[100, 130], direction=-QUARTER, spread=0, startSize=120, endSize=200, rotation=[0, TAU], spin=9, tilt=GROUND, localSpace=True, offset=[0, 60], **fade("C8FFFF", 0.85, 0.2))))
effect("buffs", "whirlwind", "Whirlwind", "casual", "loop",
       ("glow", halo("FFC24A", 320, 1.6, 0.3, timing=steady(1.5, 1.6), offset=[0, -20], localSpace=True)),
       ("funnel", emitter("swirl", **steady(14, 1.2), maxParticles=24, lifetime=1.1, speed=[200, 230], direction=-QUARTER, spread=0, startSize=40, endSize=300, sizeCurve="quadIn", rotation=[0, TAU], spin=11, tilt=GROUND, localSpace=True, offset=[0, 80], **ramp((0, "FFE37A", 0), (0.15, "FFD24A", 0.9), (0.7, "FFB52E", 0.7), (1, "FF8A1F", 0)))),
       ("dust", emitter("debris", blend="alpha", **steady(6, 1.0), maxParticles=12, lifetime=[0.6, 1.0], speed=[60, 120], direction=-QUARTER, spread=1.6, gravity=[0, 120], startSize=[8, 14], endSize=4, rotation=[0, TAU], spin=[-6, 6], offset=[0, 80], frameMode="random", localSpace=True, **fade("FFFFFF", 1, 0.1, 0.7))))
effect("buffs", "shield-bubble", "Shield bubble", "casual", "loop",
       ("glow", halo("FFC93C", 420, 2.0, 0.3, timing=steady(1, 2.0), localSpace=True)),
       ("inside", emitter("ring-thick", **steady(1, 2.0), maxParticles=4, lifetime=2.0, **still(290), localSpace=True, **fade("FFB52E", 0.3, 0.25, 0.75))),
       ("rim", emitter("ring-thin", **steady(1, 2.0), maxParticles=4, lifetime=2.0, **still(320), localSpace=True, **fade("FFE37A", 1, 0.25, 0.75))),
       ("bands", emitter("ring-thin", **steady(2, 1.6), maxParticles=6, lifetime=1.6, speed=[50, 60], direction=-QUARTER, spread=0, startSize=180, endSize=240, tilt=GROUND, offset=[0, 70], localSpace=True, **fade("7FE8FF", 0.5, 0.2))),
       ("shine", emitter("star-4point", **steady(2), maxParticles=6, lifetime=0.6, shape="ring", shapeSize=[150, 0], **pop(26), localSpace=True, **fade("FFFFFF", 1, 0.2))))
effect("buffs", "power-aura", "Power aura", "casual", "loop",
       ("glow", halo("FF3B1F", 380, 1.4, 0.35, timing=steady(1.5, 1.4), localSpace=True)),
       ("flames", emitter("flame-soft", **steady(26, 1.0), maxParticles=48, lifetime=[0.5, 0.8], speed=[90, 160], direction=-QUARTER, spread=0.15, shape="ellipse", shapeSize=[90, 24], startSize=[50, 70], endSize=10, sizeCurve="quadIn", offset=[0, 60], localSpace=True, **ramp((0, "FFE37A", 0), (0.15, "FFB52E", 0.85), (0.6, "FF4A1F", 0.6), (1, "B0101F", 0)))),
       ("base", ground("disc", "FF4A1F", 220, 1.4, 1.5, 0, alpha=0.45, offset=[0, 60])))

# Status icons float over a character while a state lasts.
for status_id, name, piece, rgb, size, extra in (
        ("status-charm", "Charm", "heart", "FF4FA3", [28, 42], {}),
        ("status-boost", "Boost", "bolt", "FFD21F", [30, 44], {"rotation": [-0.3, 0.3]}),
        ("status-attack-up", "Attack up", "arrow", "FF3B3B", [30, 40], {}),
        ("status-song", "Song", "notes", "B05CFF", [32, 44], {"rotation": [-0.25, 0.25]}),
        ("status-heal", "Heal over time", "plus-sign", "4CFF5E", [26, 40], {})):
    effect("status", status_id, name, "casual", "loop",
           ("glow", halo(rgb, 300, 1.4, 0.35, timing=steady(1.5, 1.4), tilt=GROUND, localSpace=True)),
           ("icons", rising(piece, rgb, 3.5, 80, [50, 80], [1.0, 1.4], size, turbulence={"strength": 40, "frequency": 0.02, "speed": 1}, **extra)),
           ("cores", rising(piece, "FFFFFF", 3.5, 80, [50, 80], [1.0, 1.4], [size[0] * 0.55, size[1] * 0.55], alpha=0.7, **extra)))
effect("status", "status-poison", "Poison", "casual", "loop",
       ("glow", halo("7CFF3B", 300, 1.6, 0.3, timing=steady(1.2, 1.6), tilt=GROUND, localSpace=True)),
       ("clouds", rising("puff", "C9A6FF", 2.5, 90, [20, 40], [1.4, 2.0], [50, 70], blend="alpha", alpha=0.8, frameMode="random", rotation=[-0.3, 0.3])),
       ("skulls", rising("skull", "FF5CD6", 2, 70, [40, 60], [1.2, 1.6], [26, 36])),
       ("bubbles", rising("bubble", "9CFF6B", 5, 90, [60, 110], [0.8, 1.2], [8, 16], blend="alpha")))
effect("status", "status-dizzy", "Dizzy", "casual", "loop",
       ("stars", emitter("star-5point", **steady(3.2, 1.6), maxParticles=8, lifetime=1.6, speed=160, shape="ring", shapeSize=[80, 0], directionMode="tangent", spread=0, radialAcceleration=-320, startSize=26, endSize=26, spin=3, tilt=GROUND, localSpace=True, **fade("FFE23C", 1, 0.15, 0.85))),
       ("orbit", ground("ring-thin", "FFF1B8", 180, 1.6, 1.25, 0, alpha=0.25)))

# Healing.
effect("healing", "heal-burst", "Heal burst", "casual", "once",
       ("glow", halo("4CFF5E", 420, 0.9, 0.45)),
       ("ring", ring("B8FFC0", 80, 300, 0.6, tilt=GROUND)),
       ("crosses", emitter("plus-sign", **once(9), maxParticles=12, lifetime=[0.8, 1.1], speed=[80, 180], direction=-QUARTER, spread=1.4, damping=1.5, gravity=[0, -40], startSize=[22, 38], endSize=0, sizeCurve={"points": [0, 1.2, 1, 0]}, **fade("7CFF8A", 1, 0.08))),
       ("sparkles", burst("star-4point", "E6FFE9", 10, [120, 240], [0.5, 0.8], [10, 20], gravity=[0, -80])))
effect("healing", "regeneration", "Regeneration", "casual", "loop",
       ("glow", halo("4CFF5E", 320, 1.6, 0.3, timing=steady(1.2, 1.6), localSpace=True)),
       ("base", ground("circle-glyphs", "7CFF8A", 220, 1.6, 1.2, 0.5, alpha=0.6, offset=[0, 70])),
       ("crosses", rising("plus-sign", "7CFF8A", 4, 100, [60, 90], [1.0, 1.4], [16, 28])),
       ("sparkles", rising("star-4point", "E6FFE9", 10, 120, [40, 100], [0.7, 1.2], [8, 16])))

# Explosions.
effect("explosions", "explosion-small", "Small explosion", "casual", "once",
       ("glow", halo("FF8A1F", 460, 0.7, 0.55)),
       ("flash", flare("flare", "FFF4C8", 260, 0.24)),
       ("rays", flare("flash-burst", "FFC266", 300, 0.3, rotation=[0, TAU], alpha=0.8)),
       ("dust", emitter("dust", blend="alpha", **once(8), maxParticles=12, lifetime=[0.5, 0.8], speed=[80, 180], spread=TAU, damping=3, startSize=[40, 60], endSize=[80, 110], sizeCurve="quadOut", rotation=[0, TAU], frameMode="random", **fade("FFFFFF", 0.95, 0.05, 0.4))),
       ("rocks", emitter("debris", blend="alpha", **once(7), maxParticles=10, lifetime=[0.6, 0.9], speed=[260, 420], direction=-QUARTER, spread=2.2, gravity=[0, 1100], startSize=[16, 28], endSizeScale=0.8, rotation=[0, TAU], spin=[-8, 8], frameMode="random", **fade("FFFFFF", 1, 0.02, 0.8))))
effect("explosions", "explosion-large", "Large explosion", "casual", "once",
       ("glow", halo("FF6A1F", 620, 1.0, 0.6)),
       ("flash", flare("flare", "FFFFFF", 340, 0.26)),
       ("fireball", emitter("fireball", **once(6), maxParticles=8, lifetime=[0.45, 0.65], speed=[40, 120], spread=TAU, damping=3, startSize=[90, 130], endSize=[200, 260], sizeCurve="quadOut", rotation=[0, TAU], frameMode="overLife", **ramp((0, "FFF4C8", 1), (0.25, "FFB52E", 0.95), (0.6, "FF4A1F", 0.6), (1, "6A1010", 0)))),
       ("smoke", emitter("puff", blend="alpha", **once(8, delay=0.12), maxParticles=12, lifetime=[0.9, 1.3], speed=[60, 140], spread=TAU, damping=2, gravity=[0, -60], startSize=[70, 100], endSize=[150, 200], rotation=[0, TAU], frameMode="random", **ramp((0, "A8968C", 0), (0.15, "948278", 0.85), (0.7, "7A6C66", 0.55), (1, "6A5E5A", 0)))),
       ("shockwave", ring("FFD27A", 120, 560, 0.5, piece="ring-thick", tilt=GROUND, alpha=0.8)),
       ("rocks", emitter("debris", blend="alpha", **once(10), maxParticles=12, lifetime=[0.7, 1.1], speed=[380, 620], direction=-QUARTER, spread=2.4, gravity=[0, 1300], startSize=[18, 34], endSizeScale=0.8, rotation=[0, TAU], spin=[-9, 9], frameMode="random", **fade("FFFFFF", 1, 0.02, 0.85))),
       ("sparks", burst("spark-streak", "FFD08A", 16, [600, 1000], [0.25, 0.45], [30, 48], alignToVelocity=True, stretch=0.05, rotation=0, gravity=[0, 400])))
effect("explosions", "poof", "Poof", "casual", "once",
       ("clouds", emitter("cloud", blend="alpha", **once(8), maxParticles=10, lifetime=[0.5, 0.7], speed=[160, 220], shape="ring", shapeSize=[20, 0], directionMode="outward", spread=0.2, damping=5, startSize=[50, 70], endSize=[80, 100], sizeCurve="quadOut", frameMode="random", rotation=[-0.3, 0.3], **ramp((0, "FFFFFF", 1), (0.6, "FFFFFF", 1), (1, "FFFFFF", 0)))),
       ("sparkles", burst("star-4point", "FFFFFF", 6, [180, 300], [0.3, 0.5], [12, 22], blend="additive")))
effect("explosions", "smoke-burst", "Smoke burst", "casual", "once",
       ("puffs", emitter("puff", blend="alpha", **once(10), maxParticles=12, lifetime=[0.7, 1.0], speed=[120, 240], spread=TAU, damping=4, startSize=[50, 80], endSize=[100, 140], sizeCurve="quadOut", frameMode="random", rotation=[0, TAU], **ramp((0, "8C8C9A", 1), (0.6, "6C6C7A", 0.9), (1, "5A5A68", 0)))))

# Portals.
effect("portals", "portal-swirl", "Swirl portal", "casual", "loop",
       ("glow", halo("FF3BD6", 420, 1.6, 0.45, timing=steady(1.2, 1.6), localSpace=True)),
       ("spiral", emitter("spiral", **steady(1.25, 1.6), maxParticles=4, lifetime=1.6, **still(280), spin=-3, localSpace=True, **fade("FF8AE6", 1, 0.25, 0.75))),
       ("rim", emitter("ring-thick", **steady(1.25, 1.6), maxParticles=4, lifetime=1.6, **still(260), spin=1, localSpace=True, **fade("FF3BD6", 0.6, 0.25, 0.75))),
       ("pull", emitter("star-4point", **steady(14, 1.0), maxParticles=24, lifetime=0.8, speed=[120, 160], shape="ring", shapeSize=[200, 0], directionMode="inward", spread=0, tangentialAcceleration=200, startSize=[12, 20], endSize=4, localSpace=True, **fade("FFD0F6", 1, 0.2))))
effect("portals", "portal-gold", "Golden portal", "casual", "loop",
       ("glow", halo("FFB52E", 460, 1.6, 0.45, timing=steady(1.2, 1.6), localSpace=True)),
       ("swirl", emitter("swirl", **steady(4, 1.0), maxParticles=8, lifetime=1.0, **still(260, 200), rotation=[0, TAU], spin=4, localSpace=True, **fade("FFE37A", 0.9, 0.2))),
       ("core", emitter("orb", **steady(1.25, 1.6), maxParticles=4, lifetime=1.6, **still(150), localSpace=True, **fade("FFF4C8", 0.5, 0.25, 0.75))),
       ("sparkles", emitter("star-4point", **steady(10, 1.0), maxParticles=16, lifetime=[0.6, 1.0], shape="ring", shapeSize=[110, 0], shapeThickness=20, **pop(20), localSpace=True, **fade("FFFFFF", 1, 0.2))))

# Elements.
effect("fire", "fire-orb", "Fire orb", "casual", "loop",
       ("glow", halo("FF8A1F", 300, 1.0, 0.45, timing=steady(2, 1.0), localSpace=True)),
       ("core", emitter("orb", **steady(2, 1.0), maxParticles=4, lifetime=1.0, **still(110), localSpace=True, **fade("FFE37A", 1, 0.25, 0.75))),
       ("flames", emitter("flame-soft", **steady(24, 0.8), maxParticles=32, lifetime=[0.4, 0.6], speed=[80, 140], direction=-QUARTER, spread=0.4, shape="circle", shapeSize=[30, 0], startSize=[60, 80], endSize=10, sizeCurve="quadIn", localSpace=True, **ramp((0, "FFF4C8", 0), (0.15, "FFB52E", 0.9), (0.6, "FF4A1F", 0.6), (1, "B0101F", 0)))))
effect("ice", "ice-shatter", "Ice shatter", "casual", "once",
       ("glow", halo("5CC8FF", 380, 0.6, 0.45)),
       ("flash", flare("flare", "E6F8FF", 220, 0.24, rotation=0.4)),
       ("ring", ring("BDEBFF", 60, 260, 0.4)),
       ("shards", emitter("shard", blend="additive", **once(12), maxParticles=16, lifetime=[0.5, 0.8], speed=[260, 480], spread=TAU, damping=2.5, gravity=[0, 500], startSize=[24, 40], endSizeScale=0.6, alignToVelocity=True, rotation=QUARTER, spin=[-4, 4], **fade("C8F0FF", 1, 0.02, 0.6))),
       ("sparkles", burst("star-4point", "FFFFFF", 8, [140, 260], [0.4, 0.6], [10, 18])))
effect("lightning", "lightning-strike", "Lightning strike", "casual", "once",
       ("glow", halo("5C8DFF", 500, 0.6, 0.55)),
       ("bolt", emitter("electric-arc", **once(2), maxParticles=4, lifetime=0.3, speed=0, startSize=[260, 320], endSize=[260, 320], rotation=[-0.4, 0.4], offset=[0, -40], **ramp((0, "FFFFFF", 1), (0.3, "BFD4FF", 1), (0.5, "6C9CFF", 0.2), (0.65, "BFD4FF", 0.9), (1, "4C7CFF", 0)))),
       ("flash", flare("flare", "FFFFFF", 260, 0.22)),
       ("ring", ring("9CC4FF", 60, 300, 0.36, tilt=GROUND)),
       ("sparks", burst("spark-streak", "BFD4FF", 12, [400, 760], [0.15, 0.3], [24, 40], alignToVelocity=True, stretch=0.04, rotation=0)))
effect("water", "water-splash", "Water splash", "casual", "once",
       ("crown", emitter("splash", blend="alpha", **once(1), maxParticles=2, lifetime=0.5, speed=0, startSize=180, endSize=290, sizeCurve="quadOut", offset=[0, -60], **ramp((0, "BFEFFF", 0.9), (0.6, "8FD8FF", 0.8), (1, "6CC4FF", 0)))),
       ("drops", emitter("droplet", blend="alpha", **once(14), maxParticles=16, lifetime=[0.5, 0.8], speed=[260, 460], direction=-QUARTER, spread=1.3, gravity=[0, 1400], startSize=[12, 22], endSizeScale=0.7, alignToVelocity=True, rotation=QUARTER, **fade("FFFFFF", 1, 0.02, 0.8))),
       ("ring", ring("BFEFFF", 60, 260, 0.6, tilt=GROUND, blend="alpha", alpha=0.8)))

# Rewards.
effect("rewards", "coin-burst", "Coin burst", "casual", "once",
       ("glow", halo("FFC93C", 340, 0.6, 0.4)),
       ("coins", emitter("coin", blend="alpha", **once(12), maxParticles=16, lifetime=[0.8, 1.1], speed=[300, 520], direction=-QUARTER, spread=1.2, gravity=[0, 1300], startSize=[26, 34], endSizeScale=1, frameMode="loopRandomStart", frameRate=8, collision={"type": "floor", "y": 60, "bounce": 0.4}, **fade("FFFFFF", 1, 0.02, 0.92))),
       ("sparkles", burst("star-4point", "FFF1B8", 8, [160, 300], [0.4, 0.6], [12, 22])))
effect("rewards", "pickup", "Pickup", "casual", "once",
       ("glow", halo("FFE37A", 260, 0.5, 0.45)),
       ("ring", ring("FFF1B8", 120, 30, 0.35)),
       ("flare", flare("star-4point", "FFFFFF", 150, 0.4, spin=3, settle=0.1, timing=once(1, delay=0.25))),
       ("sparkles", emitter("star-4point", **once(10), maxParticles=12, lifetime=0.35, speed=[220, 260], shape="ring", shapeSize=[110, 0], directionMode="inward", spread=0, startSize=[12, 20], endSize=4, **fade("FFF1B8", 1, 0.2))))
effect("rewards", "star-reward", "Star reward", "casual", "once",
       ("glow", halo("FFC93C", 420, 0.9, 0.5)),
       ("star", flare("star-5point", "FFE23C", 180, 0.8, settle=0.85, spin=[-0.5, 0.5])),
       ("rays", flare("flash-burst", "FFD24A", 340, 0.9, spin=0.6, settle=0.9, alpha=0.6)),
       ("stars", burst("star-5point", "FFF1B8", 10, [240, 420], [0.5, 0.8], [14, 26], gravity=[0, 400])))
effect("celebration", "confetti-pop", "Confetti pop", "casual", "once",
       ("flakes", emitter("hard-dot", blend="alpha", **once(40), maxParticles=48, lifetime=[1.2, 1.8], speed=[420, 760], direction=-QUARTER, spread=1.1, gravity=[0, 900], damping=2.2, startSize=[12, 18], endSizeScale=1, aspect=[0.35, 0.55], rotation=[0, TAU], spin=[-12, 12], tints=["#FFFF4F7A", "#FFFFC93C", "#FF4CC9FF", "#FF7CFF5E", "#FFB05CFF"], **fade("FFFFFF", 1, 0.02, 0.85))),
       ("confetti", emitter("confetti", blend="alpha", **once(16), maxParticles=24, lifetime=[1.2, 1.8], speed=[420, 760], direction=-QUARTER, spread=1.1, gravity=[0, 900], damping=2.2, startSize=[24, 34], endSizeScale=1, rotation=[0, TAU], spin=[-10, 10], tints=["#FFFF4F7A", "#FFFFC93C", "#FF4CC9FF", "#FF7CFF5E", "#FFB05CFF"], **fade("FFFFFF", 1, 0.02, 0.85))),
       ("flash", flare("flare", "FFFFFF", 180, 0.2)))


# Realistic fire and smoke: painted flames that flicker through their frames, embers that drift and smoke that billows above them.
FIRE = ramp((0, "FFE7A8", 0), (0.12, "FFC24A", 0.7), (0.45, "FF7A1F", 0.6), (0.8, "D8381F", 0.3), (1, "801010", 0))
SMOKE = ramp((0, "5A5650", 0), (0.2, "4A4642", 0.5), (0.7, "3A3836", 0.3), (1, "2A2826", 0))
effect("fire", "campfire-realistic", "Campfire", "realistic", "loop",
       ("glow", halo("FF8A1F", 380, 1.2, 0.35, timing=steady(2, 1.2), offset=[0, -40])),
       ("flames", emitter("flame", **steady(26, 1.0), maxParticles=48, lifetime=[0.6, 0.9], speed=[60, 120], direction=-QUARTER, spread=0.2, shape="ellipse", shapeSize=[44, 8], startSize=[80, 110], endSize=[24, 34], sizeCurve="quadIn", rotation=[-0.15, 0.15], frameMode="loopRandomStart", frameRate=12, turbulence={"strength": 60, "frequency": 0.02, "speed": 2}, light={"radius": 300, "color": "#FFFF9A40", "intensity": 1.2, "flicker": {"speed": 8, "amount": 0.25}}, emission=1, **FIRE)),
       ("core", emitter("flame-soft", **steady(10, 1.0), maxParticles=16, lifetime=[0.35, 0.5], speed=[40, 80], direction=-QUARTER, spread=0.15, shape="ellipse", shapeSize=[24, 4], startSize=[36, 50], endSize=12, emission=1, **ramp((0, "FFF4C8", 0), (0.15, "FFE7A8", 0.5), (1, "FFB52E", 0)))),
       ("embers", emitter("ember", **steady(10, 2.0), maxParticles=32, lifetime=[1.2, 2.0], speed=[80, 160], direction=-QUARTER, spread=0.5, shape="ellipse", shapeSize=[40, 6], turbulence={"strength": 120, "frequency": 0.03, "speed": 1.5}, startSize=[6, 12], endSize=2, emission=1, **ramp((0, "FFF4C8", 1), (0.5, "FF9A2E", 0.9), (1, "E0401F", 0)))),
       ("smoke", emitter("smoke", blend="alpha", **steady(5, 3.0), maxParticles=24, lifetime=[2.2, 3.0], speed=[50, 80], direction=-QUARTER, spread=0.25, offset=[0, -110], turbulence={"strength": 30, "frequency": 0.01, "speed": 0.5}, startSize=[50, 70], endSize=[160, 220], rotation=[0, TAU], spin=[-0.4, 0.4], frameMode="random", particleOrder="newestFirst", **SMOKE)))
effect("fire", "torch-flame", "Torch flame", "realistic", "loop",
       ("glow", halo("FF9A2E", 240, 1.0, 0.35, timing=steady(2, 1.0), offset=[0, -20])),
       ("flames", emitter("flame", **steady(18, 1.0), maxParticles=24, lifetime=[0.4, 0.6], speed=[50, 90], direction=-QUARTER, spread=0.15, shape="ellipse", shapeSize=[12, 4], startSize=[44, 60], endSize=[12, 18], sizeCurve="quadIn", frameMode="loopRandomStart", frameRate=14, light={"radius": 200, "color": "#FFFFA850", "intensity": 1, "flicker": {"speed": 10, "amount": 0.3}}, emission=1, **FIRE)),
       ("core", emitter("flame-soft", **steady(8, 1.0), maxParticles=12, lifetime=[0.25, 0.4], speed=[30, 50], direction=-QUARTER, spread=0.1, startSize=[20, 26], endSize=8, emission=1, **ramp((0, "FFF4C8", 0), (0.2, "FFE7A8", 0.5), (1, "FFB52E", 0)))),
       ("sparks", emitter("ember", **steady(4, 1.5), maxParticles=12, lifetime=[0.8, 1.4], speed=[60, 120], direction=-QUARTER, spread=0.4, turbulence={"strength": 100, "frequency": 0.04, "speed": 2}, startSize=[4, 8], endSize=1, emission=1, **ramp((0, "FFF4C8", 1), (1, "FF6A1F", 0)))))
effect("fire", "candle-flame", "Candle flame", "realistic", "loop",
       ("glow", halo("FFB24A", 160, 1.0, 0.4, timing=steady(2, 1.0), offset=[0, -10])),
       ("flame", emitter("flame-soft", **steady(14, 1.0), maxParticles=12, lifetime=[0.3, 0.4], speed=[30, 40], direction=-QUARTER, spread=0.05, startSize=[40, 46], endSize=10, light={"radius": 120, "color": "#FFFFC070", "intensity": 0.8, "flicker": {"speed": 6, "amount": 0.15}}, emission=1, **ramp((0, "FFFFFF", 0), (0.2, "FFF4C8", 1), (0.7, "FFB52E", 0.7), (1, "FF6A1F", 0)))))
effect("fire", "neon-flame", "Neon flame", "neon", "loop",
       ("glow", halo("FF2EC8", 280, 1.0, 0.45, timing=steady(2, 1.0), offset=[0, -30])),
       ("flames", emitter("flame-soft", **steady(30, 1.0), maxParticles=40, lifetime=[0.5, 0.8], speed=[80, 140], direction=-QUARTER, spread=0.2, shape="ellipse", shapeSize=[30, 6], startSize=[50, 70], endSize=10, sizeCurve="quadIn", turbulence={"strength": 60, "frequency": 0.03, "speed": 2}, unshaded=True, **ramp((0, "FFFFFF", 0), (0.15, "7FF6FF", 1), (0.55, "FF2EC8", 0.8), (1, "6A10FF", 0)))),
       ("sparks", emitter("hard-dot", **steady(10, 1.0), maxParticles=24, lifetime=[0.6, 1.0], speed=[100, 200], direction=-QUARTER, spread=0.6, startSize=[4, 7], endSize=1, trail={"length": 6, "lifetime": 0.15, "widthStart": 3, "widthEnd": 0}, unshaded=True, **ramp((0, "FFFFFF", 1), (0.5, "7FF6FF", 1), (1, "FF2EC8", 0)))))
effect("fire", "heat-haze", "Heat haze", "realistic", "loop",
       ("haze", emitter("soft-dot", blend="alpha", **steady(36, 1.4), maxParticles=96, lifetime=[0.9, 1.4], speed=[50, 90], direction=-QUARTER, spread=0.35, turbulence={"strength": 60, "frequency": 0.03, "speed": 1.5}, startSize=[16, 24], endSize=[36, 52], shape="circle", shapeSize=[24, 0], distortion=1, colors=["#00FFFFFF", "#C0FFFFFF", "#00FFFFFF"])))
effect("smoke", "smoke-column", "Smoke column", "realistic", "loop",
       ("smoke", emitter("smoke", blend="alpha", **steady(7, 4.0), maxParticles=40, lifetime=[3.0, 4.0], speed=[60, 90], direction=-QUARTER, spread=0.2, turbulence={"strength": 40, "frequency": 0.01, "speed": 0.5}, gravity=[20, 0], startSize=[60, 80], endSize=[220, 300], rotation=[0, TAU], spin=[-0.3, 0.3], frameMode="random", particleOrder="newestFirst", **ramp((0, "8A8680", 0), (0.15, "7A7670", 0.75), (0.7, "6A6662", 0.4), (1, "5A5652", 0)))),
       ("wisps", emitter("smoke-curl", blend="alpha", **steady(2, 3.0), maxParticles=8, lifetime=[2.0, 3.0], speed=[60, 80], direction=-QUARTER, spread=0.3, startSize=[60, 80], endSize=[140, 180], rotation=[-0.3, 0.3], **ramp((0, "B0ACA8", 0), (0.3, "A09C98", 0.4), (1, "908C88", 0)))))
effect("explosions", "explosion-realistic", "Explosion", "realistic", "once",
       ("glow", halo("FF6A1F", 640, 1.1, 0.6)),
       ("flash", flare("flare", "FFFFFF", 360, 0.24)),
       ("fireball", emitter("fireball", **once(9), maxParticles=12, lifetime=[0.55, 0.8], speed=[60, 180], spread=TAU, damping=3, startSize=[110, 150], endSize=[220, 300], sizeCurve="quadOut", rotation=[0, TAU], frameMode="overLife", emission=1, **ramp((0, "FFF4C8", 1), (0.2, "FFC24A", 0.95), (0.55, "FF5A1F", 0.7), (1, "501010", 0)))),
       ("smoke", emitter("smoke", blend="alpha", **once(10, delay=0.15), maxParticles=12, lifetime=[1.4, 2.0], speed=[60, 150], spread=TAU, damping=2, gravity=[0, -50], startSize=[100, 140], endSize=[240, 320], rotation=[0, TAU], spin=[-0.3, 0.3], frameMode="random", particleOrder="newestFirst", **ramp((0, "4A4440", 0), (0.15, "3E3834", 0.85), (0.7, "34302E", 0.5), (1, "2A2826", 0)))),
       ("debris", emitter("debris", blend="alpha", **once(12), maxParticles=14, lifetime=[0.8, 1.2], speed=[420, 700], direction=-QUARTER, spread=2.6, gravity=[0, 1400], startSize=[14, 28], endSizeScale=0.8, rotation=[0, TAU], spin=[-10, 10], frameMode="random", **fade("FFFFFF", 1, 0.02, 0.85))),
       ("sparks", burst("spark-streak", "FFD08A", 22, [700, 1200], [0.3, 0.55], [30, 50], alignToVelocity=True, stretch=0.05, rotation=0, gravity=[0, 500], emission=1)),
       ("shockwave", ring("FFE7C8", 100, 620, 0.45, piece="ring-thick", tilt=GROUND, alpha=0.6)))
effect("explosions", "explosion-neon", "Neon burst", "neon", "once",
       ("glow", halo("FF2EC8", 420, 0.6, 0.5)),
       ("ring", ring("7FF6FF", 40, 340, 0.4, unshaded=True)),
       ("flare", flare("flare-thin", "FFFFFF", 300, 0.25, rotation=[0, TAU], timing=once(2), unshaded=True)),
       ("sparks", emitter("hard-dot", **once(40), maxParticles=48, lifetime=[0.4, 0.7], speed=[300, 600], directionMode="outward", shape="ring", shapeSize=[8, 0], spread=0, damping=3, startSize=[6, 10], endSize=[2, 3], trail={"length": 8, "lifetime": 0.2, "widthStart": 4, "widthEnd": 0}, unshaded=True, **ramp((0, "FFFFFF", 1), (0.4, "FF40F0", 1), (1, "FF30C0", 0)))))
effect("explosions", "shockwave-distortion", "Shockwave", "realistic", "once",
       ("bend", emitter("ring-thin", blend="alpha", **once(1), maxParticles=1, lifetime=0.6, speed=0, startSize=40, endSize=720, sizeCurve="expoOut", distortion=1, colors=["#FFFFFFFF", "#00FFFFFF"])))
effect("scifi", "shield-ripple-distortion", "Shield ripple", "realistic", "once",
       ("bend", emitter("ring-thin", blend="alpha", rate=0, bursts=[{"time": 0, "count": 1, "cycles": 3, "interval": 0.08}], duration=0.2, maxParticles=3, lifetime=0.4, speed=0, startSize=10, endSize=140, distortion=0.6, colors=["#FFFFFFFF", "#00FFFFFF"])))
effect("scifi", "energy-charge", "Energy charge", "neon", "loop",
       ("glow", halo("2EC8FF", 300, 1.0, 0.45, timing=steady(2, 1.0), localSpace=True)),
       ("core", emitter("orb", **steady(2, 1.0), maxParticles=4, lifetime=1.0, **still(90, 120), localSpace=True, unshaded=True, **fade("BFF6FF", 1, 0.25, 0.75))),
       ("pull", emitter("spark-streak", **steady(24, 1.0), maxParticles=32, lifetime=0.45, speed=[300, 360], shape="ring", shapeSize=[170, 0], directionMode="inward", spread=0, alignToVelocity=True, startSize=[24, 34], endSize=8, localSpace=True, unshaded=True, **fade("7FE8FF", 1, 0.2))),
       ("arcs", emitter("electric-arc", **steady(3, 1.0), maxParticles=4, lifetime=0.18, **still(130), rotation=[0, TAU], localSpace=True, unshaded=True, **fade("DFF8FF", 0.9, 0.1))))
effect("scifi", "laser-impact", "Laser impact", "neon", "once",
       ("glow", halo("FF2E4D", 300, 0.4, 0.5)),
       ("flare", flare("flare-thin", "FF8A9A", 260, 0.22, rotation=0, timing=once(1), unshaded=True)),
       ("core", flare("hard-dot", "FFFFFF", 50, 0.2, unshaded=True)),
       ("sparks", burst("spark-streak", "FF6A7A", 12, [400, 700], [0.15, 0.3], [20, 34], alignToVelocity=True, stretch=0.04, rotation=0, direction=3.1416, spread=1.6, unshaded=True)))

# Sparks.
effect("sparks", "welding-sparks", "Welding sparks", "realistic", "loop",
       ("glow", halo("FFD27A", 140, 0.4, 0.2, timing=steady(6, 0.4))),
       ("sparks", emitter("spark-streak", **steady(70, 1.0), maxParticles=128, lifetime=[0.4, 0.9], speed=[260, 520], direction=-1.1, spread=1.6, gravity=[0, 1200], alignToVelocity=True, stretch=0.03, startSize=[14, 24], endSize=4, collision={"type": "floor", "y": 80, "bounce": 0.35, "friction": 0.2}, emission=1, **ramp((0, "FFFFFF", 1), (0.3, "FFE7A8", 1), (0.7, "FF9A2E", 0.8), (1, "E0401F", 0)))))
effect("sparks", "sparkler", "Sparkler", "realistic", "loop",
       ("glow", halo("FFE7A8", 180, 0.3, 0.25, timing=steady(8, 0.3))),
       ("sparks", emitter("spark-streak", **steady(90, 0.6), maxParticles=96, lifetime=[0.15, 0.35], speed=[200, 420], spread=TAU, alignToVelocity=True, stretch=0.04, startSize=[10, 20], endSize=2, emission=1, **ramp((0, "FFFFFF", 1), (0.5, "FFE37A", 1), (1, "FF9A2E", 0)))),
       ("stars", emitter("star-4point", **steady(20, 0.6), maxParticles=24, lifetime=0.2, spread=TAU, **pop(18, speed=[150, 300]), **fade("FFFFFF", 1, 0.1))))

# Weather and environment.
effect("weather", "rain", "Rain", "realistic", "loop",
       ("drops", emitter("rain-streak", blend="alpha", **steady(160, 1.5), maxParticles=400, lifetime=[0.7, 0.9], speed=[1300, 1600], direction=1.85, spread=0.05, shape="rectangle", shapeSize=[1100, 10], offset=[0, -560], alignToVelocity=True, rotation=-QUARTER, startSize=[38, 56], endSizeScale=1, **ramp((0, "BFD8FF", 0.6), (1, "BFD8FF", 0.6)))))
effect("weather", "snow", "Snow", "casual", "loop",
       ("flakes", emitter("snowflake", blend="alpha", **steady(40, 8.0), maxParticles=400, lifetime=[7, 9], speed=[60, 110], direction=QUARTER, spread=0.3, shape="rectangle", shapeSize=[1100, 10], offset=[0, -560], turbulence={"strength": 40, "frequency": 0.004, "speed": 0.4}, startSize=[10, 22], endSizeScale=1, rotation=[0, TAU], spin=[-1, 1], **fade("FFFFFF", 0.95, 0.05, 0.9))))
effect("weather", "falling-leaves", "Falling leaves", "casual", "loop",
       ("leaves", emitter("leaf", blend="alpha", **steady(4, 8.0), maxParticles=48, lifetime=[7, 10], speed=[50, 90], direction=QUARTER, spread=0.4, gravity=[20, 10], shape="rectangle", shapeSize=[1000, 10], offset=[0, -560], turbulence={"strength": 70, "frequency": 0.006, "speed": 0.5}, startSize=[22, 32], endSizeScale=1, rotation=[0, TAU], spin=[-1.5, 1.5], frameMode="random", tints=["#FFE89A3C", "#FFD0603C", "#FFE8C850", "#FF9CC84C"], **fade("FFFFFF", 1, 0.05, 0.9))))
effect("weather", "cherry-petals", "Cherry petals", "casual", "loop",
       ("petals", emitter("petal", blend="alpha", **steady(8, 8.0), maxParticles=80, lifetime=[6, 9], speed=[60, 110], direction=0.9, spread=0.4, shape="rectangle", shapeSize=[1000, 10], offset=[-200, -560], turbulence={"strength": 60, "frequency": 0.006, "speed": 0.6}, startSize=[14, 22], endSizeScale=1, rotation=[0, TAU], spin=[-2, 2], frameMode="random", tints=["#FFFFC8DC", "#FFFFB0CC", "#FFFFE0EA"], **fade("FFFFFF", 1, 0.05, 0.9))))
effect("environment", "fireflies", "Fireflies", "casual", "loop",
       ("lights", emitter("soft-dot", **steady(6, 6.0), maxParticles=48, lifetime=[4, 6], speed=[10, 30], spread=TAU, shape="rectangle", shapeSize=[500, 200], turbulence={"strength": 40, "frequency": 0.01, "speed": 0.6}, startSize=[18, 28], endSizeScale=1, emission=1, particleLights={"radius": 40, "intensity": 0.6, "max": 16}, **ramp((0, "D8FF6A", 0), (0.2, "D8FF6A", 0.9), (0.35, "D8FF6A", 0.2), (0.55, "D8FF6A", 0.9), (0.8, "D8FF6A", 0.3), (1, "D8FF6A", 0)))),
       ("cores", emitter("hard-dot", **steady(6, 6.0), maxParticles=48, lifetime=[4, 6], speed=[10, 30], spread=TAU, shape="rectangle", shapeSize=[500, 200], turbulence={"strength": 40, "frequency": 0.01, "speed": 0.6}, startSize=[4, 6], endSizeScale=1, emission=1, **fade("F4FFD0", 0.9, 0.2, 0.8))))
effect("environment", "floating-embers", "Floating embers", "realistic", "loop",
       ("embers", emitter("ember", **steady(12, 6.0), maxParticles=96, lifetime=[4, 6], speed=[40, 80], direction=-QUARTER, spread=0.6, shape="rectangle", shapeSize=[700, 40], offset=[0, 300], turbulence={"strength": 60, "frequency": 0.008, "speed": 0.6}, startSize=[6, 14], endSize=2, emission=1, **ramp((0, "FFF4C8", 0), (0.1, "FFC24A", 1), (0.7, "FF6A1F", 0.7), (1, "B0201F", 0)))))
effect("environment", "dust-motes", "Dust motes", "realistic", "loop",
       ("motes", emitter("soft-dot", blend="alpha", **steady(10, 8.0), maxParticles=96, lifetime=[6, 9], speed=[5, 15], spread=TAU, shape="rectangle", shapeSize=[600, 300], turbulence={"strength": 15, "frequency": 0.006, "speed": 0.3}, startSize=[4, 9], endSizeScale=1, **fade("FFF1D8", 0.5, 0.2, 0.8))))
effect("environment", "landing-dust", "Landing dust", "casual", "once",
       ("dust", emitter("dust", blend="alpha", **once(10), maxParticles=12, lifetime=[0.5, 0.8], speed=[160, 280], direction=-QUARTER, shape="ellipse", shapeSize=[30, 4], directionMode="outward", spread=0.2, damping=5, gravity=[0, -40], startSize=[30, 46], endSize=[60, 80], sizeCurve="quadOut", rotation=[0, TAU], frameMode="random", **fade("FFFFFF", 0.9, 0.05, 0.4))))

# Trails follow a moving emitter and leave pieces behind it as it travels.
effect("trails", "magic-trail", "Magic trail", "casual", "loop",
       ("sparkles", emitter("star-4point", rate=8, rateOverDistance=0.25, maxParticles=96, lifetime=[0.4, 0.7], spread=TAU, **pop(22, 0.0, [10, 40]), spin=[-2, 2], **fade("FFE7FF", 1, 0.1))),
       ("glow", emitter("soft-dot", rate=10, rateOverDistance=0.15, maxParticles=64, lifetime=[0.3, 0.5], speed=0, startSize=60, endSize=20, **fade("C86AFF", 0.45, 0.1))))
effect("trails", "neon-trail", "Neon trail", "neon", "loop",
       ("head", emitter("hard-dot", rate=24, rateOverDistance=0.1, maxParticles=48, lifetime=[0.3, 0.5], speed=[10, 30], spread=TAU, startSize=[8, 12], endSize=2, trail={"length": 10, "lifetime": 0.3, "widthStart": 6, "widthEnd": 0, "colors": ["#FFFFFFFF", "#007FF6FF"]}, unshaded=True, **ramp((0, "FFFFFF", 1), (0.5, "7FF6FF", 1), (1, "2E6BFF", 0)))))


# Side view: the emitter sits on the floor of a platform game, where pieces land, bounce and roll, and motion runs along the floor.
FLOOR = {"type": "floor", "y": 0, "bounce": 0.35, "friction": 0.35}
LEFT = 3.1416
DUST = {"blend": "alpha", "frameMode": "random", "rotation": [0, TAU]}


def side_puffs(count: int, speed: list[float], size: list[float], grow: float, life: list[float], rgb: str = "FFFFFF", alpha: float = 0.9) -> list[tuple[str, dict]]:
    """Dust puffs that roll away along the floor to both sides."""
    return [(name, emitter("dust", **once(count), maxParticles=count * 2, lifetime=life, speed=speed, direction=direction, spread=0.35, damping=4.5, gravity=[0, -40], startSize=size, endSize=[size[0] * grow, size[1] * grow], sizeCurve="quadOut", **DUST, **fade(rgb, alpha, 0.05, 0.4)))
            for name, direction in (("left", LEFT - 0.15), ("right", 0.15))]


def chunks(piece: str, count: int, speed: list[float], size: list[float], rgb: str = "FFFFFF", spread: float = 2.2, blend: str = "alpha", **extra) -> dict:
    """Pieces that fly up, fall back, bounce on the floor and settle, such as the bricks of a broken block."""
    options = {"maxParticles": count * 2, "lifetime": [1.1, 1.6], "speed": speed, "direction": -QUARTER, "spread": spread, "gravity": [0, 1500], "startSize": size, "endSizeScale": 1, "rotation": [0, TAU], "spin": [-9, 9], "spinCurve": "quadOut", "frameMode": "random", "collision": FLOOR, "blend": blend, **fade(rgb, 1, 0.02, 0.85)}
    options.update(extra)
    return emitter(piece, **once(count), **options)


def collect(piece: str, rgb: str, size: float, blend: str = "alpha", **extra) -> dict:
    """A collected item that pops, rises and fades away."""
    options = {"maxParticles": 2, "lifetime": 0.6, "speed": 260, "direction": -QUARTER, "spread": 0, "damping": 4, "startSize": size * 0.6, "endSize": size, "sizeCurve": {"points": [0, 1.25, 1]}, "blend": blend, **fade("FFFFFF", 1, 0.02, 0.6)}
    options.update(extra)
    return emitter(piece, **once(1), **options)


# Movement.
effect("movement", "jump-dust", "Jump dust", "casual", "once",
       *side_puffs(4, [120, 200], [22, 32], 1.8, [0.35, 0.55]),
       ("sparkles", burst("star-4point", "FFFFFF", 4, [80, 160], [0.25, 0.4], [8, 14], direction=-QUARTER, spread=2.4)), view="side")
effect("movement", "double-jump", "Double jump", "casual", "once",
       ("ring", ring("BFF4FF", 40, 180, 0.35, tilt=1.25)),
       ("flash", flare("flare", "FFFFFF", 120, 0.2)),
       ("feathers", emitter("feather", blend="alpha", **once(6), maxParticles=8, lifetime=[0.9, 1.3], speed=[120, 220], direction=QUARTER, spread=2.6, damping=3, gravity=[0, 160], turbulence={"strength": 120, "frequency": 0.02, "speed": 1.5}, startSize=[16, 24], endSizeScale=1, rotation=[0, TAU], spin=[-3, 3], **fade("FFFFFF", 1, 0.05, 0.7))),
       ("sparkles", burst("star-4point", "BFF4FF", 6, [120, 220], [0.3, 0.5], [10, 18])), view="side")
effect("movement", "land-dust", "Landing dust", "casual", "once",
       *side_puffs(6, [200, 340], [30, 44], 2.0, [0.45, 0.7]),
       ("pebbles", chunks("debris", 6, [180, 320], [6, 10], spread=1.6)), view="side")
effect("movement", "run-dust", "Run dust", "casual", "loop",
       ("puffs", emitter("dust", **steady(7, 0.8), maxParticles=12, lifetime=[0.45, 0.7], speed=[50, 100], direction=LEFT - 0.25, spread=0.3, damping=3, gravity=[0, -50], startSize=[16, 24], endSize=[36, 48], sizeCurve="quadOut", **DUST, **fade("FFFFFF", 0.85, 0.08, 0.4))), view="side")
effect("movement", "dash", "Dash", "casual", "once",
       ("glow", emitter("soft-dot", **once(1), maxParticles=2, lifetime=0.25, speed=0, startSize=90, endSize=60, aspect=3.0, offset=[-70, -60], **fade("7FE8FF", 0.35, 0.05))),
       ("streaks", emitter("spark-streak", **once(12), maxParticles=16, lifetime=[0.2, 0.35], speed=[700, 1000], direction=LEFT, spread=0.05, damping=5, shape="rectangle", shapeSize=[20, 60], offset=[0, -60], startSize=[60, 110], endSize=20, alignToVelocity=True, stretch=0.05, **fade("DFFBFF", 0.9, 0.05))),
       ("flash", flare("flare-thin", "FFFFFF", 200, 0.18, offset=[0, -60])),
       ("dust", emitter("dust", **once(4), maxParticles=6, lifetime=[0.4, 0.6], speed=[100, 200], direction=LEFT - 0.2, spread=0.3, damping=4, startSize=[24, 34], endSize=[50, 64], **DUST, **fade("FFFFFF", 0.8, 0.05, 0.4))), view="side")
effect("movement", "wall-slide", "Wall slide", "casual", "loop",
       ("dust", emitter("dust", **steady(10, 0.6), maxParticles=16, lifetime=[0.35, 0.55], speed=[40, 90], direction=LEFT - 0.6, spread=0.5, gravity=[0, 120], startSize=[12, 18], endSize=[28, 36], shape="rectangle", shapeSize=[2, 30], **DUST, **fade("FFFFFF", 0.8, 0.1, 0.4))),
       ("sparks", emitter("spark-streak", **steady(14, 0.6), maxParticles=24, lifetime=[0.25, 0.45], speed=[150, 280], direction=LEFT - 0.9, spread=0.6, gravity=[0, 900], alignToVelocity=True, stretch=0.03, startSize=[10, 18], endSize=4, emission=1, **ramp((0, "FFFFFF", 1), (0.4, "FFD27A", 1), (1, "FF6A1F", 0)))), view="side")
effect("movement", "wall-jump", "Wall jump", "casual", "once",
       ("ring", ring("FFFFFF", 30, 150, 0.3, aspect=0.4, alpha=0.9)),
       ("puffs", emitter("dust", **once(6), maxParticles=8, lifetime=[0.4, 0.6], speed=[160, 280], direction=LEFT, spread=1.2, damping=4.5, startSize=[22, 32], endSize=[46, 60], **DUST, **fade("FFFFFF", 0.85, 0.05, 0.4))),
       ("sparkles", burst("star-4point", "FFFFFF", 4, [100, 200], [0.25, 0.4], [10, 16], direction=LEFT, spread=1.4)), view="side")
effect("movement", "ground-pound", "Ground pound", "casual", "once",
       ("flash", flare("flash-burst", "FFE7A8", 260, 0.25, rotation=[0, TAU])),
       ("shockwave", ring("FFF1D8", 60, 560, 0.45, piece="ring-thick", tilt=1.38, alpha=0.9)),
       *side_puffs(8, [300, 480], [36, 52], 2.2, [0.55, 0.85]),
       ("rocks", chunks("debris", 10, [380, 620], [10, 20], spread=1.5)), view="side")
effect("movement", "spring-bounce", "Spring bounce", "casual", "once",
       ("lines", emitter("spark-streak", **once(6), maxParticles=8, lifetime=0.3, speed=[500, 700], direction=-QUARTER, spread=0.9, damping=6, startSize=[40, 60], endSize=10, alignToVelocity=True, stretch=0.03, **fade("FFFFFF", 0.9, 0.05))),
       ("stars", emitter("star-5point", **once(5), maxParticles=6, lifetime=[0.5, 0.7], speed=[300, 420], direction=-QUARTER, spread=1.6, gravity=[0, 900], startSize=[16, 24], endSize=0, sizeCurve="quadIn", spin=[-6, 6], **fade("FFE23C", 1, 0.05))),
       ("ring", ring("FFFFFF", 40, 160, 0.3, tilt=1.25)), view="side")
effect("movement", "speed-lines", "Speed lines", "casual", "loop",
       ("lines", emitter("spark-streak", **steady(30, 0.5), maxParticles=40, lifetime=[0.18, 0.3], speed=[1400, 1800], direction=LEFT, spread=0, shape="rectangle", shapeSize=[200, 150], startSize=[120, 220], endSizeScale=1, aspect=1.5, offset=[0, -80], **fade("FFFFFF", 0.5, 0.2, 0.7))), view="side")
effect("movement", "footstep-splash", "Footstep splash", "casual", "once",
       ("drops", emitter("droplet", blend="alpha", **once(8), maxParticles=10, lifetime=[0.4, 0.6], speed=[180, 300], direction=-QUARTER, spread=1.8, gravity=[0, 1300], startSize=[8, 14], endSizeScale=0.7, alignToVelocity=True, rotation=QUARTER, collision={"type": "floor", "y": 0, "result": "die"}, **fade("FFFFFF", 1, 0.02, 0.9))),
       ("ripple", ring("BFEFFF", 20, 120, 0.5, tilt=1.35, blend="alpha", alpha=0.8)), view="side")
effect("movement", "glide-feathers", "Glide feathers", "casual", "loop",
       ("feathers", emitter("feather", blend="alpha", **steady(3, 2.0), maxParticles=12, lifetime=[1.6, 2.2], speed=[40, 80], direction=LEFT + 0.6, spread=0.6, gravity=[0, 60], turbulence={"strength": 90, "frequency": 0.015, "speed": 1}, startSize=[16, 24], endSizeScale=1, rotation=[0, TAU], spin=[-2, 2], offset=[0, -60], **fade("FFFFFF", 1, 0.1, 0.75))), view="side")

# Pickups.
effect("pickups", "coin-collect", "Coin collect", "casual", "once",
       ("glow", halo("FFC93C", 220, 0.4, 0.45)),
       ("coin", collect("coin", "FFFFFF", 54, frameMode="loop", frameRate=12)),
       ("flare", flare("star-4point", "FFF1B8", 110, 0.3, spin=4)),
       ("sparkles", burst("star-4point", "FFE37A", 8, [160, 280], [0.3, 0.5], [10, 18])), view="side")
effect("pickups", "gem-collect", "Gem collect", "casual", "once",
       ("glow", halo("2ED8FF", 260, 0.45, 0.45)),
       ("gem", collect("gem-cyan", "FFFFFF", 60)),
       ("ring", ring("BFF6FF", 40, 170, 0.35)),
       ("sparkles", burst("star-4point", "BFF6FF", 10, [180, 300], [0.35, 0.55], [10, 18])), view="side")
effect("pickups", "key-collect", "Key collect", "casual", "once",
       ("glow", halo("FFC93C", 300, 0.7, 0.45)),
       ("rays", flare("flash-burst", "FFE37A", 260, 0.7, spin=0.8, settle=0.8, alpha=0.7)),
       ("key", collect("key", "FFFFFF", 70, lifetime=0.8, rotation=-0.4, spin=1.2)),
       ("sparkles", burst("star-4point", "FFF1B8", 10, [140, 260], [0.4, 0.7], [10, 18])), view="side")
effect("pickups", "heart-pickup", "Heart pickup", "casual", "once",
       ("glow", halo("FF4F7A", 260, 0.5, 0.45)),
       ("heart", collect("heart-red", "FFFFFF", 60)),
       ("hearts", emitter("heart", **once(6), maxParticles=8, lifetime=[0.6, 0.9], speed=[120, 220], direction=-QUARTER, spread=2.0, damping=2.5, gravity=[0, -60], startSize=[14, 22], endSize=0, sizeCurve="quadIn", **fade("FF6A9A", 1, 0.05))),
       ("ring", ring("FFC2D2", 40, 160, 0.35)), view="side")
effect("pickups", "power-up", "Power up", "casual", "once",
       ("glow", halo("FFC93C", 360, 1.0, 0.35, offset=[0, -80])),
       ("column", emitter("light-beam", **once(2), maxParticles=4, lifetime=[0.7, 0.9], speed=0, shape="rectangle", shapeSize=[30, 0], startSize=320, endSize=360, aspect=2.0, offset=[0, -150], **fade("FFD24A", 0.5, 0.15))),
       ("spiral", emitter("star-4point", **once(16), maxParticles=20, lifetime=[0.7, 1.0], speed=[60, 90], shape="ring", shapeSize=[60, 0], directionMode="tangent", spread=0, gravity=[0, -500], radialAcceleration=-80, startSize=[12, 22], endSize=0, sizeCurve="quadIn", **fade("FFF1B8", 1, 0.05))),
       ("flash", flare("flare", "FFFFFF", 150, 0.22, offset=[0, -60])), view="side")
effect("pickups", "checkpoint", "Checkpoint", "casual", "once",
       ("glow", halo("FFC93C", 320, 0.9, 0.45, offset=[0, -60])),
       ("flag", collect("flag", "FFFFFF", 80, speed=0, lifetime=0.9, offset=[0, -60])),
       ("confetti", emitter("hard-dot", blend="alpha", **once(30), maxParticles=36, lifetime=[1.0, 1.4], speed=[360, 560], direction=-QUARTER, spread=1.0, gravity=[0, 900], damping=2, startSize=[10, 14], endSizeScale=1, aspect=[0.35, 0.55], rotation=[0, TAU], spin=[-12, 12], tints=["#FFFF4F7A", "#FFFFC93C", "#FF4CC9FF", "#FF7CFF5E"], offset=[0, -60], **fade("FFFFFF", 1, 0.02, 0.85))),
       ("sparkles", rising("star-4point", "FFF1B8", 10, 60, [80, 140], [0.6, 0.9], [10, 16])), view="side")
effect("pickups", "star-collect", "Star collect", "casual", "once",
       ("glow", halo("FFC93C", 300, 0.6, 0.5)),
       ("star", collect("star-gold", "FFFFFF", 70, spin=6, spinCurve="quadOut")),
       ("rays", flare("flash-burst", "FFE37A", 240, 0.4, spin=1, alpha=0.7)),
       ("sparkles", burst("star-5point", "FFF1B8", 8, [200, 320], [0.4, 0.6], [10, 16], gravity=[0, 500])), view="side")
effect("pickups", "extra-life", "Extra life", "casual", "once",
       ("glow", halo("4CFF5E", 320, 0.8, 0.5)),
       ("flash", flare("flare", "FFFFFF", 200, 0.25)),
       ("ring", ring("B8FFC0", 50, 220, 0.4)),
       ("crosses", emitter("plus-sign", **once(8), maxParticles=10, lifetime=[0.7, 1.0], speed=[120, 220], direction=-QUARTER, spread=1.6, damping=2, gravity=[0, -80], startSize=[16, 26], endSize=0, sizeCurve="quadIn", **fade("7CFF8A", 1, 0.05))), view="side")
effect("pickups", "chest-open", "Chest open", "casual", "once",
       ("light", emitter("light-cone", **once(1), maxParticles=2, lifetime=1.2, speed=0, startSize=220, endSize=280, offset=[0, -110], **fade("FFE37A", 0.8, 0.15, 0.6))),
       ("glow", halo("FFC93C", 320, 1.0, 0.45, offset=[0, -40])),
       ("coins", chunks("coin", 10, [420, 620], [22, 30], spread=0.9, frameMode="loopRandomStart", frameRate=10, spin=0, rotation=0)),
       ("sparkles", rising("star-4point", "FFF1B8", 14, 60, [120, 220], [0.5, 0.9], [10, 18])), view="side")
effect("pickups", "coin-magnet", "Coin magnet", "casual", "loop",
       ("coins", emitter("coin", blend="alpha", **steady(4, 1.0), maxParticles=12, lifetime=1.2, speed=[40, 80], spread=TAU, shape="ring", shapeSize=[240, 0], attractors=[{"x": 0, "y": -60, "strength": 2400, "radius": 400, "killRadius": 20}], startSize=26, endSizeScale=1, frameMode="loopRandomStart", frameRate=10, **fade("FFFFFF", 1, 0.1, 0.9))),
       ("glow", halo("FFC93C", 200, 1.0, 0.35, timing=steady(2, 1.0), offset=[0, -60])), view="side")

# Combat from the side.
effect("combat", "slash-side", "Side slash", "casual", "once",
       ("arc", emitter("slash-arc", **once(1), maxParticles=2, lifetime=0.26, speed=0, startSize=230, endSize=270, rotation=-1.2, spin=8, spinCurve="quadOut", offset=[40, -70], **fade("FFFFFF", 1, 0.05))),
       ("trail", emitter("slash-arc", **once(1), maxParticles=2, lifetime=0.32, speed=0, startSize=250, endSize=300, rotation=-1.6, spin=8, spinCurve="quadOut", offset=[40, -70], **fade("7FE8FF", 0.6, 0.1))),
       ("sparks", emitter("spark-streak", **once(6), maxParticles=8, lifetime=[0.15, 0.3], speed=[300, 520], direction=0, spread=0.8, damping=4, startSize=[20, 32], endSize=6, alignToVelocity=True, stretch=0.03, offset=[110, -70], **fade("DFFBFF", 1, 0.05))), view="side")
effect("combat", "stomp-stars", "Stomp stars", "casual", "once",
       ("flash", flare("impact-star", "FFF1B8", 140, 0.2)),
       ("stars", emitter("star-5point", **once(6), maxParticles=8, lifetime=[0.6, 0.8], speed=[260, 380], direction=-QUARTER, spread=2.0, gravity=[0, 900], startSize=[18, 26], endSize=6, spin=[-6, 6], **fade("FFE23C", 1, 0.05, 0.8))),
       *side_puffs(4, [140, 220], [22, 30], 1.8, [0.35, 0.5]), view="side")
effect("combat", "enemy-defeat", "Enemy defeat", "casual", "once",
       ("flash", flare("flare", "FFFFFF", 200, 0.22, offset=[0, -60])),
       ("clouds", emitter("cloud", blend="alpha", **once(7), maxParticles=8, lifetime=[0.45, 0.65], speed=[160, 240], shape="ring", shapeSize=[20, 0], directionMode="outward", spread=0.2, damping=5, startSize=[44, 60], endSize=[70, 90], sizeCurve="quadOut", frameMode="random", offset=[0, -60], **ramp((0, "FFFFFF", 1), (0.6, "FFFFFF", 1), (1, "FFFFFF", 0)))),
       ("stars", burst("star-5point", "FFE23C", 6, [200, 320], [0.4, 0.6], [12, 20], gravity=[0, 600], offset=[0, -60])), view="side")
effect("combat", "muzzle-flash", "Muzzle flash", "casual", "once",
       ("flare", flare("flare-thin", "FFE7A8", 160, 0.1, rotation=0, settle=0.2)),
       ("burst", flare("impact-star", "FFC24A", 90, 0.1, rotation=[0, TAU])),
       ("sparks", emitter("spark-streak", **once(6), maxParticles=8, lifetime=[0.1, 0.2], speed=[500, 800], direction=0, spread=0.5, startSize=[20, 30], endSize=6, alignToVelocity=True, stretch=0.03, emission=1, **fade("FFE7A8", 1, 0.05))),
       ("smoke", emitter("puff", blend="alpha", **once(3, delay=0.05), maxParticles=4, lifetime=[0.6, 0.9], speed=[40, 80], direction=-0.4, spread=0.5, damping=2, gravity=[0, -80], startSize=[16, 24], endSize=[40, 56], rotation=[0, TAU], frameMode="random", **fade("C8C8D0", 0.6, 0.1))), view="side")
effect("combat", "bullet-impact", "Bullet impact", "casual", "once",
       ("flash", flare("star-4point", "FFF1B8", 90, 0.12)),
       ("sparks", emitter("spark-streak", **once(10), maxParticles=12, lifetime=[0.15, 0.35], speed=[300, 600], direction=LEFT, spread=1.4, gravity=[0, 900], startSize=[14, 24], endSize=4, alignToVelocity=True, stretch=0.03, emission=1, **ramp((0, "FFFFFF", 1), (0.4, "FFD27A", 1), (1, "FF6A1F", 0)))),
       ("dust", emitter("dust", **once(3), maxParticles=4, lifetime=[0.4, 0.6], speed=[60, 120], direction=LEFT, spread=0.8, damping=3, startSize=[16, 24], endSize=[36, 48], **DUST, **fade("FFFFFF", 0.8, 0.05, 0.4))),
       ("bits", emitter("stone", blend="alpha", **once(4), maxParticles=6, lifetime=[0.5, 0.8], speed=[160, 300], direction=LEFT - 0.4, spread=1.0, gravity=[0, 1300], startSize=[6, 10], endSizeScale=1, rotation=[0, TAU], spin=[-10, 10], **fade("FFFFFF", 1, 0.02, 0.85))), view="side")
effect("combat", "shell-casing", "Shell casing", "casual", "once",
       ("casing", emitter("casing", blend="alpha", **once(1), maxParticles=2, lifetime=1.4, speed=[300, 360], direction=-2.2, spread=0.2, gravity=[0, 1500], startSize=28, endSize=28, rotation=[0, TAU], spin=[14, 18], spinCurve="quadOut", collision={"type": "floor", "y": 60, "bounce": 0.45, "friction": 0.4}, **fade("FFFFFF", 1, 0.02, 0.9))), view="side")
effect("combat", "fireball-shot", "Fireball shot", "casual", "loop",
       ("glow", halo("FF8A1F", 220, 0.8, 0.45, timing=steady(3, 0.8), localSpace=True)),
       ("core", emitter("orb", **steady(3, 0.6), maxParticles=4, lifetime=0.6, **still(80), localSpace=True, **fade("FFE37A", 1, 0.2, 0.8))),
       ("trail", emitter("flame-soft", **steady(40, 0.5), maxParticles=40, lifetime=[0.25, 0.4], speed=[300, 420], direction=LEFT, spread=0.25, startSize=[50, 66], endSize=10, sizeCurve="quadIn", rotation=-QUARTER, **ramp((0, "FFF4C8", 0), (0.15, "FFB52E", 0.9), (0.6, "FF4A1F", 0.6), (1, "B0101F", 0)))),
       ("sparks", emitter("ember", **steady(14, 0.6), maxParticles=16, lifetime=[0.3, 0.5], speed=[200, 300], direction=LEFT, spread=0.5, startSize=[6, 10], endSize=2, emission=1, **ramp((0, "FFF4C8", 1), (1, "FF6A1F", 0)))), view="side")
effect("combat", "energy-shot", "Energy shot", "neon", "loop",
       ("glow", halo("2EC8FF", 200, 0.8, 0.5, timing=steady(3, 0.8), localSpace=True)),
       ("core", emitter("hard-dot", **steady(3, 0.6), maxParticles=4, lifetime=0.6, **still(40), localSpace=True, unshaded=True, **fade("FFFFFF", 1, 0.2, 0.8))),
       ("trail", emitter("spark-streak", **steady(30, 0.4), maxParticles=24, lifetime=[0.15, 0.25], speed=[500, 640], direction=LEFT, spread=0.08, startSize=[60, 90], endSize=20, alignToVelocity=True, unshaded=True, **fade("7FE8FF", 1, 0.1))), view="side")
effect("combat", "death-burst", "Defeat burst", "casual", "once",
       ("flash", flare("flare", "FFFFFF", 260, 0.25, offset=[0, -60])),
       ("orbs", emitter("hard-dot", **once(12), maxParticles=12, lifetime=[0.5, 0.7], speed=[260, 300], shape="ring", shapeSize=[10, 0], directionMode="outward", spread=0, damping=3, startSize=[22, 28], endSize=0, sizeCurve="quadIn", offset=[0, -60], **fade("FFFFFF", 1, 0.02))),
       ("ring", ring("FFFFFF", 40, 260, 0.4, offset=[0, -60])),
       ("sparkles", burst("star-4point", "FFFFFF", 8, [160, 280], [0.4, 0.6], [10, 18], offset=[0, -60])), view="side")
effect("combat", "respawn", "Respawn", "casual", "once",
       ("beam", emitter("light-beam", **once(2), maxParticles=4, lifetime=[0.6, 0.8], speed=0, startSize=360, endSize=300, aspect=1.6, offset=[0, -170], **fade("BFF4FF", 0.9, 0.1))),
       ("ring", ring("BFF4FF", 160, 40, 0.5, tilt=1.25)),
       ("gather", emitter("star-4point", **once(14), maxParticles=16, lifetime=0.5, speed=[220, 260], shape="ring", shapeSize=[130, 0], directionMode="inward", spread=0, startSize=[12, 18], endSize=4, offset=[0, -60], **fade("FFFFFF", 1, 0.2))),
       ("flash", flare("flare", "FFFFFF", 220, 0.3, timing=once(1, delay=0.45), offset=[0, -60])), view="side")
effect("combat", "hurt", "Hurt", "casual", "once",
       ("flash", flare("impact-star", "FF4A4A", 150, 0.2, rotation=[0, TAU])),
       ("core", flare("star-4point", "FFFFFF", 90, 0.15)),
       ("sparks", burst("spark-streak", "FF8A8A", 6, [300, 480], [0.15, 0.3], [18, 28], alignToVelocity=True, stretch=0.03, rotation=0)), view="side")
effect("combat", "shield-block", "Shield block", "casual", "once",
       ("shield", emitter("ring-thin", **once(1), maxParticles=2, lifetime=0.3, speed=0, startSize=160, endSize=190, aspect=0.45, offset=[30, -60], **fade("7FE8FF", 1, 0.05))),
       ("flash", flare("flare", "FFFFFF", 150, 0.18, offset=[30, -60])),
       ("sparks", emitter("spark-streak", **once(10), maxParticles=12, lifetime=[0.2, 0.35], speed=[300, 520], direction=0, spread=1.6, gravity=[0, 700], startSize=[16, 26], endSize=4, alignToVelocity=True, stretch=0.03, offset=[30, -60], **fade("DFFBFF", 1, 0.05))), view="side")
effect("combat", "explosion-side", "Explosion on the ground", "casual", "once",
       ("glow", halo("FF6A1F", 520, 0.9, 0.55, offset=[0, -80])),
       ("flash", flare("flare", "FFFFFF", 300, 0.24, offset=[0, -80])),
       ("fireball", emitter("fireball", **once(7), maxParticles=8, lifetime=[0.45, 0.65], speed=[100, 220], direction=-QUARTER, spread=1.6, damping=3, startSize=[90, 120], endSize=[180, 230], sizeCurve="quadOut", rotation=[0, TAU], frameMode="overLife", offset=[0, -40], **ramp((0, "FFF4C8", 1), (0.25, "FFB52E", 0.95), (0.6, "FF4A1F", 0.6), (1, "6A1010", 0)))),
       ("smoke", emitter("puff", blend="alpha", **once(8, delay=0.12), maxParticles=10, lifetime=[1.0, 1.4], speed=[80, 160], direction=-QUARTER, spread=1.2, damping=2, gravity=[0, -120], startSize=[60, 90], endSize=[140, 180], rotation=[0, TAU], frameMode="random", offset=[0, -80], **ramp((0, "A8968C", 0), (0.15, "948278", 0.85), (0.7, "7A6C66", 0.55), (1, "6A5E5A", 0)))),
       *side_puffs(6, [300, 460], [40, 56], 2.0, [0.6, 0.9]),
       ("rocks", chunks("debris", 10, [500, 760], [14, 26], spread=1.4)), view="side")
effect("combat", "bomb-fuse", "Bomb fuse", "casual", "loop",
       ("sparks", emitter("spark-streak", **steady(60, 0.5), maxParticles=48, lifetime=[0.1, 0.25], speed=[160, 320], spread=TAU, startSize=[8, 16], endSize=2, alignToVelocity=True, stretch=0.03, emission=1, **ramp((0, "FFFFFF", 1), (0.5, "FFE37A", 1), (1, "FF9A2E", 0)))),
       ("glow", halo("FFD27A", 90, 0.2, 0.6, timing=steady(10, 0.2))),
       ("smoke", emitter("smoke-curl", blend="alpha", **steady(3, 1.0), maxParticles=6, lifetime=[0.8, 1.2], speed=[40, 60], direction=-QUARTER, spread=0.3, startSize=[24, 32], endSize=[50, 64], **fade("A09C98", 0.5, 0.2))), view="side")

# Level pieces that break, flow and blow in a side view.
effect("level", "block-break", "Block break", "casual", "once",
       ("bricks", chunks("brick", 8, [420, 640], [20, 32], spread=1.4)),
       ("dust", emitter("dust", **once(5), maxParticles=6, lifetime=[0.4, 0.6], speed=[80, 160], spread=TAU, damping=4, startSize=[30, 40], endSize=[56, 70], offset=[0, -30], **DUST, **fade("FFE6C8", 0.85, 0.05, 0.4))),
       ("flash", flare("star-4point", "FFF1D8", 120, 0.15, offset=[0, -30])), view="side")
effect("level", "crate-break", "Crate break", "casual", "once",
       ("planks", chunks("plank", 9, [380, 600], [22, 34], spread=1.6)),
       ("chips", chunks("plank", 8, [300, 520], [8, 12], spread=2.0)),
       ("dust", emitter("dust", **once(5), maxParticles=6, lifetime=[0.4, 0.6], speed=[80, 160], spread=TAU, damping=4, startSize=[30, 40], endSize=[56, 70], offset=[0, -30], **DUST, **fade("FFFFFF", 0.85, 0.05, 0.4))), view="side")
effect("level", "rock-break", "Rock break", "casual", "once",
       ("stones", chunks("stone", 9, [360, 580], [14, 26], spread=1.6)),
       ("dust", emitter("dust", **once(6), maxParticles=8, lifetime=[0.5, 0.7], speed=[80, 180], spread=TAU, damping=4, startSize=[34, 44], endSize=[60, 76], offset=[0, -30], **DUST, **fade("E8E0D8", 0.85, 0.05, 0.4))), view="side")
effect("level", "ice-break", "Ice break", "casual", "once",
       ("cubes", chunks("ice-cube", 8, [380, 600], [14, 24], spread=1.6)),
       ("shards", chunks("shard", 8, [400, 640], [16, 24], spread=1.8, blend="additive", rgb="C8F0FF")),
       ("sparkles", burst("star-4point", "FFFFFF", 8, [140, 260], [0.4, 0.6], [10, 18], offset=[0, -30])), view="side")
effect("level", "lava-bubbles", "Lava bubbles", "casual", "loop",
       ("glow", emitter("soft-dot", **steady(6, 1.0), maxParticles=12, lifetime=1.0, speed=0, shape="rectangle", shapeSize=[200, 0], startSize=160, endSize=200, aspect=1.6, **fade("FF6A1F", 0.35, 0.3, 0.6))),
       ("blobs", emitter("lava-blob", blend="alpha", **steady(5, 1.2), maxParticles=12, lifetime=[0.6, 0.9], speed=[260, 380], direction=-QUARTER, spread=0.6, gravity=[0, 1100], shape="rectangle", shapeSize=[180, 0], startSize=[14, 22], endSizeScale=0.8, rotation=[0, TAU], collision={"type": "floor", "y": 0, "result": "die"}, emission=1, **fade("FFFFFF", 1, 0.05, 0.9))),
       ("embers", emitter("ember", **steady(8, 2.0), maxParticles=24, lifetime=[1.0, 1.6], speed=[60, 120], direction=-QUARTER, spread=0.6, shape="rectangle", shapeSize=[200, 0], turbulence={"strength": 60, "frequency": 0.02, "speed": 1}, startSize=[6, 10], endSize=2, emission=1, **ramp((0, "FFF4C8", 1), (0.6, "FF9A2E", 0.9), (1, "E0401F", 0)))), view="side")
effect("level", "waterfall-splash", "Waterfall splash", "casual", "loop",
       ("mist", emitter("soft-dot", blend="alpha", **steady(14, 2.0), maxParticles=40, lifetime=[1.2, 1.8], speed=[40, 90], direction=-QUARTER, spread=1.4, shape="rectangle", shapeSize=[90, 0], gravity=[0, -20], startSize=[40, 60], endSize=[100, 140], **fade("EAF8FF", 0.4, 0.2))),
       ("foam", emitter("cloud", blend="alpha", **steady(8, 1.0), maxParticles=16, lifetime=[0.5, 0.8], speed=[60, 120], direction=-QUARTER, spread=1.6, shape="rectangle", shapeSize=[80, 0], startSize=[30, 44], endSize=[50, 64], frameMode="random", **fade("FFFFFF", 0.9, 0.1, 0.5))),
       ("drops", emitter("droplet", blend="alpha", **steady(24, 1.0), maxParticles=40, lifetime=[0.5, 0.8], speed=[200, 340], direction=-QUARTER, spread=1.4, gravity=[0, 1200], shape="rectangle", shapeSize=[80, 0], startSize=[8, 14], endSizeScale=0.7, alignToVelocity=True, rotation=QUARTER, collision={"type": "floor", "y": 0, "result": "die"}, **fade("FFFFFF", 1, 0.02, 0.9))), view="side")
effect("level", "water-splash-side", "Water splash", "casual", "once",
       ("crown", emitter("splash", blend="alpha", **once(1), maxParticles=2, lifetime=0.5, speed=0, startSize=160, endSize=240, sizeCurve="quadOut", offset=[0, -50], **ramp((0, "BFEFFF", 0.9), (0.6, "8FD8FF", 0.8), (1, "6CC4FF", 0)))),
       ("drops", emitter("droplet", blend="alpha", **once(18), maxParticles=20, lifetime=[0.6, 0.9], speed=[300, 520], direction=-QUARTER, spread=1.5, gravity=[0, 1400], startSize=[10, 18], endSizeScale=0.7, alignToVelocity=True, rotation=QUARTER, collision={"type": "floor", "y": 0, "result": "die"}, **fade("FFFFFF", 1, 0.02, 0.9))),
       ("ripple", ring("BFEFFF", 40, 300, 0.7, tilt=1.38, blend="alpha", alpha=0.8)), view="side")
effect("level", "jetpack", "Jetpack", "realistic", "loop",
       ("glow", halo("FF9A2E", 160, 0.4, 0.45, timing=steady(6, 0.4), offset=[0, 20])),
       ("flames", emitter("flame", **steady(40, 0.5), maxParticles=32, lifetime=[0.2, 0.32], speed=[300, 420], direction=QUARTER, spread=0.15, startSize=[40, 54], endSize=12, sizeCurve="quadIn", rotation=3.1416, frameMode="loopRandomStart", frameRate=16, emission=1, **FIRE)),
       ("smoke", emitter("smoke", blend="alpha", **steady(10, 1.0), maxParticles=20, lifetime=[0.8, 1.2], speed=[200, 280], direction=QUARTER, spread=0.3, damping=2.5, startSize=[30, 40], endSize=[90, 120], rotation=[0, TAU], frameMode="random", offset=[0, 60], **ramp((0, "8A8680", 0), (0.2, "7A7670", 0.6), (1, "6A6662", 0)))), view="side")
effect("level", "rocket-exhaust", "Rocket exhaust", "realistic", "loop",
       ("flames", emitter("flame", **steady(46, 0.5), maxParticles=40, lifetime=[0.2, 0.3], speed=[460, 600], direction=LEFT, spread=0.08, startSize=[46, 60], endSize=14, sizeCurve="quadIn", rotation=-QUARTER, frameMode="loopRandomStart", frameRate=16, emission=1, **FIRE)),
       ("core", emitter("flame-soft", **steady(24, 0.5), maxParticles=16, lifetime=0.15, speed=[300, 360], direction=LEFT, spread=0.05, startSize=30, endSize=10, rotation=-QUARTER, emission=1, **fade("BFE8FF", 0.9, 0.1))),
       ("smoke", emitter("smoke", blend="alpha", **steady(12, 2.0), maxParticles=40, lifetime=[1.2, 1.8], speed=[300, 380], direction=LEFT, spread=0.12, damping=1.8, gravity=[0, -40], startSize=[30, 40], endSize=[120, 160], rotation=[0, TAU], frameMode="random", offset=[-80, 0], **ramp((0, "8A8680", 0), (0.2, "7A7670", 0.55), (1, "6A6662", 0)))), view="side")
effect("level", "steam-vent", "Steam vent", "realistic", "loop",
       ("steam", emitter("smoke", blend="alpha", **steady(9, 2.0), maxParticles=24, lifetime=[1.2, 1.7], speed=[160, 240], direction=-QUARTER, spread=0.18, damping=1.2, turbulence={"strength": 40, "frequency": 0.01, "speed": 0.8}, startSize=[30, 40], endSize=[130, 170], rotation=[0, TAU], frameMode="random", **ramp((0, "FFFFFF", 0), (0.15, "F2F6FA", 0.7), (1, "E8EEF4", 0)))), view="side")
effect("level", "drip-splash", "Drip splash", "casual", "once",
       ("drops", emitter("droplet", blend="alpha", **once(6), maxParticles=8, lifetime=[0.3, 0.45], speed=[120, 220], direction=-QUARTER, spread=1.8, gravity=[0, 1300], startSize=[5, 8], endSizeScale=0.7, **fade("FFFFFF", 1, 0.02, 0.8))), view="side")
effect("level", "dripping-water", "Dripping water", "casual", "loop",
       ("drop", emitter("droplet", blend="alpha", **steady(1.2, 2.0), maxParticles=4, lifetime=2.0, speed=0, gravity=[0, 1400], startSize=14, endSize=14, rotation=0, offset=[0, -320], collision={"type": "floor", "y": 320, "result": "die"},
                        subEmitters=[{"effect": "drip-splash.particles", "trigger": "collision", "count": [5, 7]}],
                        **fade("FFFFFF", 1, 0.02, 0.98))),
       ("ripple", ring("BFEFFF", 10, 90, 0.5, timing=steady(1.2, 0.5), tilt=1.38, blend="alpha", alpha=0.7, localSpace=True)), view="side")
effect("level", "electric-wire", "Electric wire", "neon", "loop",
       ("arcs", emitter("electric-arc", **steady(14, 0.3), maxParticles=8, lifetime=0.12, speed=0, shape="rectangle", shapeSize=[220, 0], startSize=[70, 110], endSize=[70, 110], rotation=[-0.3, 0.3], aspect=2.2, unshaded=True, **fade("BFD4FF", 1, 0.05))),
       ("sparks", emitter("spark-streak", **steady(16, 0.6), maxParticles=24, lifetime=[0.25, 0.45], speed=[120, 260], direction=QUARTER, spread=1.6, gravity=[0, 900], shape="rectangle", shapeSize=[220, 0], startSize=[10, 18], endSize=4, alignToVelocity=True, unshaded=True, **fade("DFF8FF", 1, 0.05))), view="side")
effect("level", "wind-gust", "Wind gust", "casual", "loop",
       ("streaks", emitter("swirl", blend="alpha", **steady(4, 1.0), maxParticles=8, lifetime=[0.6, 0.9], speed=[500, 700], direction=0, spread=0.05, shape="rectangle", shapeSize=[0, 160], startSize=[90, 130], endSizeScale=1, aspect=2.5, offset=[-300, -80], **fade("FFFFFF", 0.35, 0.2, 0.7))),
       ("leaves", emitter("leaf", blend="alpha", **steady(3, 1.5), maxParticles=8, lifetime=[1.0, 1.4], speed=[500, 700], direction=-0.1, spread=0.2, shape="rectangle", shapeSize=[0, 140], turbulence={"strength": 160, "frequency": 0.01, "speed": 1}, startSize=[16, 22], endSizeScale=1, rotation=[0, TAU], spin=[-6, 6], frameMode="random", tints=["#FF9CC84C", "#FFE8C850"], offset=[-300, -80], **fade("FFFFFF", 1, 0.05, 0.85))), view="side")
effect("level", "ceiling-dust", "Ceiling dust", "casual", "once",
       ("pebbles", emitter("debris", blend="alpha", **once(10), maxParticles=12, lifetime=[0.8, 1.1], speed=[0, 40], direction=QUARTER, spread=0.6, gravity=[0, 1200], shape="rectangle", shapeSize=[120, 0], startSize=[6, 12], endSizeScale=1, rotation=[0, TAU], spin=[-6, 6], frameMode="random", offset=[0, -360], collision={"type": "floor", "y": 360, "bounce": 0.3}, **fade("FFFFFF", 1, 0.02, 0.85))),
       ("dust", emitter("dust", **once(8), maxParticles=10, lifetime=[0.9, 1.3], speed=[20, 60], direction=QUARTER, spread=0.6, gravity=[0, 200], shape="rectangle", shapeSize=[120, 0], startSize=[20, 30], endSize=[50, 70], offset=[0, -360], **DUST, **fade("FFFFFF", 0.7, 0.1, 0.5))), view="side")
effect("level", "geyser", "Geyser", "casual", "loop",
       ("column", emitter("droplet", blend="alpha", **steady(60, 1.4), maxParticles=96, lifetime=[0.9, 1.3], speed=[700, 900], direction=-QUARTER, spread=0.12, gravity=[0, 1100], startSize=[12, 22], endSizeScale=0.8, alignToVelocity=True, rotation=QUARTER, collision={"type": "floor", "y": 0, "result": "die"}, **fade("FFFFFF", 1, 0.02, 0.9))),
       ("mist", emitter("soft-dot", blend="alpha", **steady(12, 2.0), maxParticles=32, lifetime=[1.0, 1.6], speed=[300, 500], direction=-QUARTER, spread=0.3, damping=2, startSize=[40, 60], endSize=[100, 140], **fade("EAF8FF", 0.35, 0.2))),
       ("foam", emitter("cloud", blend="alpha", **steady(8, 1.0), maxParticles=12, lifetime=[0.4, 0.6], speed=[60, 120], direction=-QUARTER, spread=2.0, startSize=[26, 36], endSize=[44, 56], frameMode="random", **fade("FFFFFF", 0.9, 0.1, 0.5))), view="side")
effect("level", "teleport-pad", "Teleport pad", "casual", "loop",
       ("pad", ground("disc", "2EC8FF", 200, 1.6, 1.25, 0, alpha=0.6)),
       ("pulse", ring("BFF6FF", 60, 220, 0.9, timing=steady(1.6, 0.9), tilt=GROUND, localSpace=True)),
       ("beam", emitter("light-beam", **steady(2, 1.2), maxParticles=4, lifetime=1.2, speed=0, startSize=300, endSize=320, aspect=1.6, offset=[0, -140], localSpace=True, **fade("7FE8FF", 0.6, 0.25, 0.75))),
       ("sparkles", rising("star-4point", "DFFBFF", 14, 80, [100, 180], [0.7, 1.1], [8, 14])), view="side")
effect("level", "slime-splat", "Slime splat", "casual", "once",
       ("blobs", emitter("slime", blend="alpha", **once(10), maxParticles=12, lifetime=[1.0, 1.4], speed=[260, 460], direction=-QUARTER, spread=1.8, gravity=[0, 1300], startSize=[14, 26], endSizeScale=1, rotation=[0, TAU], collision={"type": "floor", "y": 0, "result": "stick"}, **fade("FFFFFF", 1, 0.02, 0.8))),
       ("puddle", emitter("slime", blend="alpha", **once(1), maxParticles=2, lifetime=1.2, speed=0, startSize=40, endSize=90, aspect=2.6, sizeCurve="expoOut", **fade("FFFFFF", 0.9, 0.05, 0.7))), view="side")


# The pixel art effects draw from the pixel atlas and keep their own files, which the catalog lists after the effects of this tool.
PIXEL_EFFECTS = [
    ("fire", "campfire-pixel", "Campfire", "loop"), ("smoke", "smoke-puff-pixel", "Smoke poof", "once"), ("explosions", "explosion-pixel", "Explosion", "once"),
    ("explosions", "explosion-pixel-debris", "Explosion debris", "once"), ("explosions", "explosion-pixel-full", "Full explosion", "once"), ("sparks", "spark-pixel", "Sparks", "once"),
    ("weather", "rain-pixel", "Rain", "loop"), ("weather", "snow-pixel", "Snow", "loop"), ("dust", "landing-dust-pixel", "Landing dust", "once"), ("blood", "blood-pixel", "Blood", "once"),
    ("magic", "pixel-sparkle", "Sparkles", "loop"), ("healing", "heal-pixel", "Healing", "loop"), ("poison", "poison-pixel", "Poison", "loop"), ("ice", "ice-shatter-pixel", "Ice shatter", "once"),
    ("lightning", "electric-pixel", "Electricity", "loop"), ("portals", "portal-pixel", "Portal", "loop"), ("rewards", "coin-pixel", "Coin burst", "once"), ("celebration", "confetti-pixel", "Confetti", "once"),
    ("nature", "leaf-pixel", "Falling leaves", "loop"), ("bubbles", "bubble-pixel", "Bubbles", "loop"), ("trails", "pixel-trail", "Trail", "loop"), ("impacts", "hit-pixel", "Hit ring", "once"),
    ("impacts", "hit-pixel-full", "Full hit", "once"), ("auras", "aura-pixel", "Aura", "loop"), ("water", "splash-pixel", "Splash", "once"), ("vehicles", "exhaust-pixel", "Exhaust", "loop"),
    ("scifi", "data-bits", "Data bits", "loop"),
]

def largest(value: object) -> float:
    """The largest number of a value that is a number or a list of numbers."""
    if isinstance(value, (int, float)):
        return float(value)
    if isinstance(value, list) and value and all(isinstance(item, (int, float)) for item in value):
        return float(max(value))
    return 0.0


def brightness(value: str) -> float:
    red, green, blue = (int(value[index:index + 2], 16) for index in (3, 5, 7))
    return 0.2126 * red + 0.7152 * green + 0.0722 * blue


def light_glowing(layers: list[tuple[str, dict]]) -> list[tuple[str, dict]]:
    """Gives an effect that glows, one with layers that add their light, emission on those layers and a light in the brightest color of the largest of them, which fades with its particles, so it lights what stands around it at night."""
    glowing = [layer for _, layer in layers if layer.get("blend", "additive") == "additive" and "colors" in layer]
    if not glowing:
        return layers
    lit = [(name, dict(layer)) for name, layer in layers]
    for _, layer in lit:
        if layer.get("blend", "additive") == "additive" and "colors" in layer:
            layer.setdefault("emission", 1)
    if any("light" in layer or "particleLights" in layer for _, layer in lit):
        return lit
    candidates = [layer for _, layer in lit if layer.get("blend", "additive") == "additive" and "colors" in layer]
    main = max(candidates, key=lambda layer: max(largest(layer.get("startSize", 0)), largest(layer.get("endSize", 0))))
    size = max(largest(main.get("startSize", 0)), largest(main.get("endSize", 0)))
    brightest = max(main["colors"], key=lambda value: (int(value[1:3], 16) > 0, brightness(value)))
    main["light"] = {"radius": round(min(max(size * 2.5, 220.0), 520.0)), "color": "#FF" + brightest[3:9], "intensity": 1.4, "fade": "count"}
    return lit


def write_effect(entry: Effect) -> None:
    folder = LIBRARY / entry.category
    folder.mkdir(exist_ok=True)
    # An offset belongs to an entry of a composite file, so a single layer with one stays in a composite.
    layers = light_glowing(entry.layers)
    if len(layers) == 1 and "offset" not in layers[0][1]:
        document = layers[0][1]
    else:
        document = {"emitters": [{"name": name, **layer} for name, layer in layers]}
    (folder / f"{entry.id}.particles").write_text(json.dumps(document, indent=4) + "\n")


def main() -> None:
    titles = {"hits": "Hits", "magic-circles": "Magic circles", "buffs": "Buffs and auras", "status": "Status icons", "healing": "Healing", "explosions": "Explosions", "portals": "Portals", "fire": "Fire", "ice": "Ice", "lightning": "Lightning", "water": "Water", "rewards": "Rewards", "celebration": "Celebration", "smoke": "Smoke", "scifi": "Science fiction", "sparks": "Sparks", "weather": "Weather", "environment": "Environment", "trails": "Trails", "dust": "Dust and debris", "blood": "Blood", "magic": "Magic", "poison": "Poison", "nature": "Leaves and petals", "bubbles": "Bubbles", "impacts": "Impacts", "auras": "Auras", "vehicles": "Vehicles", "movement": "Movement", "pickups": "Pickups", "combat": "Combat", "level": "Level"}
    categories: dict[str, list[dict]] = {}
    for entry in EFFECTS:
        write_effect(entry)
        categories.setdefault(entry.category, []).append({"id": entry.id, "name": entry.name, "style": entry.style, "kind": entry.kind, "view": entry.view})
    for category, id, name, kind in PIXEL_EFFECTS:
        categories.setdefault(category, []).append({"id": id, "name": name, "style": "pixel", "kind": kind, "view": "any"})
    catalog = {"categories": [{"id": key, "title": titles[key], "effects": value} for key, value in categories.items()]}
    (LIBRARY / "catalog.json").write_text(json.dumps(catalog, indent=1) + "\n")


if __name__ == "__main__":
    main()
