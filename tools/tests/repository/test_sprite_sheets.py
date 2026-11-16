import json

from png_image import Image
from sprite_frame import SpriteFrame
from tests.support.tool_script import ToolScript


class SpriteSheetTest:
    """Every animated sheet of the samples is declared and its frames hold one pivot, so no animation dances."""

    def test_every_animated_sheet_of_the_samples_is_declared_and_holds_its_pivots(self):
        sheets = ToolScript.load("check-sprite-sheets.py").SpriteSheets
        declared = json.loads(sheets.MANIFEST.read_text(encoding="utf-8"))["sheets"]
        assert sheets.undeclared(declared) + sheets.check(declared) == []

    def test_a_frame_off_the_pivot_of_its_clip_drifts_and_aligned_frames_hold_it(self):
        sheets = ToolScript.load("check-sprite-sheets.py").SpriteSheets
        frames = [SpriteFrame(0, 0, 32, 32), SpriteFrame(32, 0, 32, 32)]

        def square(image: Image, left: int, top: int) -> None:
            for y in range(top, top + 8):
                for x in range(left, left + 8):
                    image.pixels[(y * 64 + x) * 4 + 3] = 255

        drifting = Image.blank(64, 32)
        square(drifting, 12, 12)
        square(drifting, 32 + 18, 12)
        assert "6.0 pixels sideways" in sheets.drift(frames, drifting, "center")

        aligned = Image.blank(64, 32)
        square(aligned, 12, 12)
        square(aligned, 32 + 12, 12)
        assert sheets.drift(frames, aligned, "center") is None
