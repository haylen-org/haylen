"""Draws the clip of the video tests: three seconds of the walker of the Sprites tests running over the island, with the clouds, the hills and the ground of the Camera tests scrolling behind it at their own speeds, and a note that rises every second.

Run it from anywhere with `python3 samples/tests/tools/video/generate-content.py`, with `ffmpeg` on the path. Every strip moves by whole copies of itself in the three seconds and the run cycle fits them six times, so the clip loops without a seam. The clip is H.264 in the baseline profile with AAC sound in an MP4 file, the format that the decoder of every platform plays.
"""

from __future__ import annotations

import math
import struct
import subprocess
from pathlib import Path

CONTENT = Path(__file__).resolve().parents[2] / "content"
VIDEO = CONTENT / "video"
WIDTH, HEIGHT, SECONDS, RATE = 640, 360, 3, 30
SKY = "0x8FD3F5"
# Each strip with its picture, its height in the clip, its top and how many copies of itself it moves by in the clip.
STRIPS = (("camera/clouds.png", 110, 20, 1), ("camera/hills.png", 250, 70, 1), ("camera/ground.png", 150, 250, 2))
WALKER = "sprites/sheets/walker.png"
# The run is the second row of the walker sheet, eight cells of 256 pixels played at sixteen frames a second, and the walker stands this tall with its feet on the grass.
RUN_RATE = 16
HERO = 190
HERO_FEET = 324


def size(path: Path) -> tuple[int, int]:
    with path.open("rb") as file:
        return struct.unpack(">II", file.read(24)[16:24])


def main() -> None:
    VIDEO.mkdir(parents=True, exist_ok=True)
    inputs, graph = [], [f"color=c={SKY}:s={WIDTH}x{HEIGHT}:r={RATE}:d={SECONDS}[l0]"]
    for index, (picture, height, top, laps) in enumerate(STRIPS):
        width, full = size(CONTENT / picture)
        scaled = round(width * height / full / 2) * 2
        copies = math.ceil(WIDTH / scaled) + 1
        speed = scaled * laps / SECONDS
        inputs += ["-loop", "1", "-framerate", str(RATE), "-t", str(SECONDS), "-i", str(CONTENT / picture)]
        labels = "".join(f"[c{index}_{copy}]" for copy in range(copies))
        graph.append(f"[{index}:v]scale={scaled}:{height},split={copies}{labels}")
        graph.append(f"{labels}hstack=inputs={copies}[s{index}]")
        graph.append(f"[l{index}][s{index}]overlay=x='-mod(t*{speed:.4f},{scaled})':y={top}:shortest=1[l{index + 1}]")
    walker = len(STRIPS)
    inputs += ["-loop", "1", "-framerate", str(RATE), "-t", str(SECONDS), "-i", str(CONTENT / WALKER)]
    graph.append(f"[{walker}:v]crop=256:256:'256*mod(floor(t*{RUN_RATE}),8)':256,scale={HERO}:{HERO}[hero]")
    graph.append(f"[l{walker}][hero]overlay=x={WIDTH // 2 - HERO // 2 - 80}:y={HERO_FEET - round(HERO * 248 / 256)}:shortest=1,format=yuv420p[video]")
    note = "0.25*sin(2*PI*(330+110*floor(t))*t)*exp(-3*mod(t,1))"
    subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-y", *inputs, "-f", "lavfi", "-i", f"aevalsrc='{note}|{note}':s=44100:d={SECONDS}", "-filter_complex", ";".join(graph), "-map", "[video]", "-map", f"{walker + 1}:a", "-c:v", "libx264", "-profile:v", "baseline", "-pix_fmt", "yuv420p", "-crf", "30", "-c:a", "aac", "-b:a", "64k", "-movflags", "+faststart", "-map_metadata", "-1", "-fflags", "+bitexact", "-t", str(SECONDS), str(VIDEO / "clip.mp4")], check=True)
    print("Wrote clip.mp4.")


if __name__ == "__main__":
    main()
