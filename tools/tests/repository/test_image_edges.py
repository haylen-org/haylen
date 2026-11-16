from png_image import Image
from tests.support.tool_script import ToolScript


class ImageEdgeTest:
    """Every image of the samples and the templates has clean edges, without the magenta it was painted on and without jagged outlines."""

    def test_every_image_of_the_samples_and_templates_has_clean_edges(self):
        assert ToolScript.load("check-image-edges.py").ImageEdges.check() == []

    def test_magenta_left_from_the_background_and_jagged_edges_are_found(self):
        edges = ToolScript.load("check-image-edges.py").ImageEdges
        image = Image.blank(16, 16)
        image.pixels[(8 * 16 + 8) * 4 : (8 * 16 + 8) * 4 + 4] = bytes((250, 10, 245, 90))
        assert edges.leftovers(image) == [8 * 16 + 8]

        smooth = Image.blank(32, 32)
        stairs = Image.blank(32, 32)
        for y in range(32):
            for x in range(32):
                inside = (x - 16) ** 2 + (y - 16) ** 2
                smooth.pixels[(y * 32 + x) * 4 + 3] = 255 if inside < 100 else (128 if inside < 144 else 0)
                stairs.pixels[(y * 32 + x) * 4 + 3] = 255 if x + y < 32 and x > 2 and y > 2 else 0
        assert edges.jagged(smooth) == 0.0
        assert edges.jagged(stairs) > edges.JAGGED_SHARE
