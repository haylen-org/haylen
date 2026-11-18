from haylen.core.app_manifest import AppManifest


class AppColors:
    """The colors of `haylen.json`, `#RRGGBB` or `#AARRGGBB` as its schema checks them, read the way the engine reads them, and the background of the splash, which every platform shows before the engine starts: the background of `splash`, or else the clear color of the app, which is transparent for a transparent window and opaque black otherwise."""

    OPAQUE = "#FF000000"
    TRANSPARENT = "#00000000"

    @staticmethod
    def components(text: str) -> tuple[int, int, int, int]:
        """The red, green, blue and alpha of a color, each from 0 to 255."""
        digits = text.removeprefix("#")
        value = int(digits, 16)
        alpha = 0xFF if len(digits) == 6 else (value >> 24) & 0xFF
        return (value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF, alpha

    @staticmethod
    def splash_background(app: AppManifest) -> str:
        transparent = app.section("window").get("transparent", False) is True
        clear = str(app.document.get("clearColor", AppColors.TRANSPARENT if transparent else AppColors.OPAQUE))
        return str(app.section("splash").get("background", clear))
