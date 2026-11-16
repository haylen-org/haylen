from pathlib import Path

from haylen.core.app_manifest import AppManifest
from haylen.core.errors.invalid_manifest import InvalidManifest
from haylen.core.repository import Repository


class AndroidSplash:
    """The splash of `haylen.json` as resources of the app module, which replace the defaults of the `haylen` library that show the engine logo: the background, the dark background in the resources of the night mode when the app gives one, and the logo."""

    RESOURCES = "app/src/main/res"
    LOGO_SUFFIXES = (".png", ".webp", ".jpg", ".jpeg")

    @staticmethod
    def colors(color: str) -> str:
        """The resource file of a background, which Android reads as `#AARRGGBB`, from a color of `haylen.json` as `#RRGGBB` or `#AARRGGBB`."""
        digits = color.removeprefix("#").upper()
        argb = digits if len(digits) == 8 else f"FF{digits}"
        lines = [
            '<?xml version="1.0" encoding="utf-8"?>',
            "<!-- The splash background of haylen.json. -->",
            "<resources>",
            f'    <color name="haylen_splash_background">#{argb}</color>',
            "</resources>",
            "",
        ]
        return "\n".join(lines)

    @staticmethod
    def files(app: AppManifest) -> dict[str, bytes | Path]:
        splash = app.section("splash")
        window = app.section("window")
        transparent = window.get("transparent", False) is True
        background = splash.get("background", app.document.get("clearColor", "#00000000" if transparent else "#FF000000"))
        files: dict[str, bytes | Path] = {f"{AndroidSplash.RESOURCES}/values/haylen_splash.xml": AndroidSplash.colors(str(background)).encode()}
        if "darkBackground" in splash:
            files[f"{AndroidSplash.RESOURCES}/values-night/haylen_splash.xml"] = AndroidSplash.colors(str(splash["darkBackground"])).encode()

        logo = splash.get("logo")
        if logo is None:
            return files
        path = app.folder / "content" / str(logo)
        if path.suffix.lower() not in AndroidSplash.LOGO_SUFFIXES:
            raise InvalidManifest(
                f'Android splash logos are PNG, WebP or JPEG images, so `{Repository.shown(path)}` does not fit. Give "splash.logo" of `{app.origin}` an image in one of those formats.',
                origin=app.origin,
            )
        files[f"{AndroidSplash.RESOURCES}/drawable/haylen_splash_logo{path.suffix.lower()}"] = path
        return files
