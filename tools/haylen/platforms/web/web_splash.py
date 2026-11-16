from dataclasses import dataclass
from pathlib import Path

from haylen.core.resolved_product import ResolvedProduct


@dataclass(frozen=True)
class WebSplash:
    """The splash of the page of an app, as the engine reads it from `haylen.json`: the logo of `splash.logo`, or the logo of the engine from the web SDK, and the background of `splash.background`, or else the clear color of the app, transparent for a transparent window, with its dark variant. The page takes the colors as `rgba(r, g, b, a)` and the logo as `splash` with the extension of its image."""

    OPAQUE = "#FF000000"
    TRANSPARENT = "#00000000"
    ENGINE_LOGO = "haylen-logo.svg"

    logo: Path
    background: str
    dark_background: str | None
    transparent: bool

    @staticmethod
    def of(product: ResolvedProduct) -> WebSplash:
        app = product.app
        splash = app.section("splash")
        transparent = bool(app.section("window").get("transparent", False))
        clear = str(app.document.get("clearColor", WebSplash.TRANSPARENT if transparent else WebSplash.OPAQUE))
        logo = app.folder / "content" / str(splash["logo"]) if splash.get("logo") else product.primary.sdk.file("platform") / WebSplash.ENGINE_LOGO
        dark = splash.get("darkBackground")
        return WebSplash(logo, WebSplash.css(str(splash.get("background", clear))), WebSplash.css(str(dark)) if dark else None, transparent)

    @staticmethod
    def css(color: str) -> str:
        """Turns a color of `haylen.json`, `#RRGGBB` or `#AARRGGBB`, which its schema checks, into the color of CSS."""
        digits = color.removeprefix("#")
        value = int(digits, 16)
        alpha = 0xFF if len(digits) == 6 else (value >> 24) & 0xFF
        return f"rgba({(value >> 16) & 0xFF}, {(value >> 8) & 0xFF}, {value & 0xFF}, {alpha / 255:.3f})"

    @property
    def file(self) -> str:
        return f"splash{self.logo.suffix.lower()}"

    def describe(self) -> dict[str, str]:
        described = {"logo": self.file, "background": self.background}
        if self.dark_background is not None:
            described["darkBackground"] = self.dark_background
        return described
