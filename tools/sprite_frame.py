from png_image import Image


class SpriteFrame:
    """A frame of a clip of an animated sheet: its region of the image, where that region sits in the untrimmed frame, and the frame it is painted over, when only what differs from that one counts."""

    def __init__(self, x: int, y: int, width: int, height: int, left: int = 0, top: int = 0, full: tuple[int, int] | None = None, behind: SpriteFrame | None = None) -> None:
        self.x = x
        self.y = y
        self.width = width
        self.height = height
        self.left = left
        self.top = top
        self.full = full or (width, height)
        self.behind = behind

    def shows(self, image: Image, x: int, y: int) -> bool:
        """Tells whether a pixel of the frame is part of what it draws, which over a background is what differs from it."""
        offset = ((self.y + y) * image.width + self.x + x) * 4
        if self.behind is None:
            return image.pixels[offset + 3] > 96
        under = ((self.behind.y + y) * image.width + self.behind.x + x) * 4
        return sum(abs(image.pixels[offset + channel] - image.pixels[under + channel]) for channel in range(3)) >= 40

    def measure(self, image: Image) -> tuple[float, float, float, float, float] | None:
        """Returns the line of the feet, the middle of the head and torso, the middle of the visible box, the middle of its height and the middle of its lowest part, in the untrimmed frame."""
        lit = [(x, y) for y in range(self.height) for x in range(self.width) if self.shows(image, x, y)]
        if not lit:
            return None
        xs = [x for x, _ in lit]
        ys = [y for _, y in lit]
        top, bottom = min(ys), max(ys)
        upper = [x for x, y in lit if y <= top + (bottom - top) * 0.55]
        lower = [x for x, y in lit if y >= bottom - (bottom - top) * 0.15]
        return bottom + self.top, sum(upper) / len(upper) + self.left, (min(xs) + max(xs)) / 2 + self.left, (top + bottom) / 2 + self.top, sum(lower) / len(lower) + self.left
