"""Draws the clip of the video tests: three seconds of a blue square that crosses a dark screen over a yellow bar that fills with the time, and a tone that rises every second.

Run it from anywhere with `python3 samples/tests/tools/video/generate-content.py`, with `ffmpeg` on the path. The clip is H.264 in the baseline profile with AAC sound in an MP4 file, the format that the decoder of every platform plays.
"""

from __future__ import annotations

import subprocess
from pathlib import Path

VIDEO = Path(__file__).resolve().parents[2] / "content" / "video"

SQUARE = "between(X,20+80*T,80+80*T)*between(Y,50,110)"
BAR = "gte(Y,168)*lt(X,8+104*T)"


def channel(square: int, bar: int, background: int) -> str:
    return f"if({SQUARE},{square},if({BAR},{bar},{background}))"


def main() -> None:
    VIDEO.mkdir(parents=True, exist_ok=True)
    picture = f"nullsrc=s=320x180:r=30:d=3,format=gbrp,geq=r='{channel(76, 242, 27)}':g='{channel(125, 178, 30)}':b='{channel(255, 58, 43)}',format=yuv420p"
    tone = "0.3*sin(2*PI*(330+110*floor(t))*t)"
    sound = f"aevalsrc='{tone}|{tone}':s=44100:d=3"
    subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-y", "-f", "lavfi", "-i", picture, "-f", "lavfi", "-i", sound, "-c:v", "libx264", "-profile:v", "baseline", "-pix_fmt", "yuv420p", "-crf", "28", "-c:a", "aac", "-b:a", "64k", "-movflags", "+faststart", "-map_metadata", "-1", "-fflags", "+bitexact", str(VIDEO / "clip.mp4")], check=True)


if __name__ == "__main__":
    main()
