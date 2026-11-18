from dataclasses import dataclass
from pathlib import Path

from haylen.core.app_colors import AppColors
from haylen.core.resolved_product import ResolvedProduct


@dataclass(frozen=True)
class WebSplash:
    """The splash of the page of an app, as the engine reads it from `haylen.json`: the logo of `splash.logo`, or the logo of the engine from the web SDK, and the background of `splash.background`, or else the clear color of the app, transparent for a transparent window, with its dark variant. The page takes the colors as `rgba(r, g, b, a)` and the logo as `splash` with the extension of its image."""

    ENGINE_LOGO = "haylen-logo.svg"

    logo: Path
    background: str
    dark_background: str | None
    transparent: bool

    @staticmethod
    def of(product: ResolvedProduct) -> WebSplash:
        app = product.app
        splash = app.section("splash")
        transparent = app.section("window").get("transparent", False) is True
        logo = app.folder / "content" / str(splash["logo"]) if splash.get("logo") else product.primary.sdk.file("platform") / WebSplash.ENGINE_LOGO
        dark = splash.get("darkBackground")
        return WebSplash(logo, WebSplash.css(AppColors.splash_background(app)), WebSplash.css(str(dark)) if dark else None, transparent)

    @staticmethod
    def css(color: str) -> str:
        """Turns a color of `haylen.json`, `#RRGGBB` or `#AARRGGBB`, which its schema checks, into the color of CSS."""
        red, green, blue, alpha = AppColors.components(color)
        return f"rgba({red}, {green}, {blue}, {alpha / 255:.3f})"

    @property
    def file(self) -> str:
        return f"splash{self.logo.suffix.lower()}"

    def describe(self) -> dict[str, str]:
        described = {"logo": self.file, "background": self.background}
        if self.dark_background is not None:
            described["darkBackground"] = self.dark_background
        return described
