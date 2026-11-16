import json
from pathlib import Path

from haylen.core.app_manifest import AppManifest
from haylen.platforms.generated_tree import GeneratedTree


class SplashCatalog:
    """Writes `Splash.xcassets`, whose `splash-logo` image and `splash-background` color the launch screens of iOS, iPadOS, Mac Catalyst and tvOS show: the logo of `splash.logo` in `haylen.json` or the logo of the engine, and the background of the splash, or the clear color of the app, with the dark variant of `splash.darkBackground` when the app gives one."""

    FOLDER = "Splash.xcassets"
    INFO = {"info": {"author": "xcode", "version": 1}}

    @staticmethod
    def contents(document: dict) -> bytes:
        return (json.dumps(document, indent=2) + "\n").encode("utf-8")

    @staticmethod
    def color(text: str) -> tuple[int, int, int, int]:
        """Reads `#RRGGBB` or `#AARRGGBB`, which the schema of `haylen.json` already checked, like the engine does, and returns red, green, blue and alpha."""
        digits = text.removeprefix("#")
        value = int(digits, 16)
        if len(digits) == 6:
            return (value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF, 0xFF
        return (value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF, (value >> 24) & 0xFF

    @staticmethod
    def entry(color: tuple[int, int, int, int]) -> dict:
        red, green, blue, alpha = color
        components = {"red": f"{red / 255:.3f}", "green": f"{green / 255:.3f}", "blue": f"{blue / 255:.3f}", "alpha": f"{alpha / 255:.3f}"}
        return {"color": {"color-space": "srgb", "components": components}, "idiom": "universal"}

    @staticmethod
    def colorset(background: tuple[int, int, int, int], dark: tuple[int, int, int, int] | None) -> dict:
        """Describes the color set of the background, with a variant for the dark appearance when there is one."""
        colors = [SplashCatalog.entry(background)]
        if dark is not None:
            colors.append({**SplashCatalog.entry(dark), "appearances": [{"appearance": "luminosity", "value": "dark"}]})
        return {"colors": colors, **SplashCatalog.INFO}

    @staticmethod
    def background(app: AppManifest) -> tuple[int, int, int, int]:
        transparent = app.section("window").get("transparent", False)
        fallback = app.document.get("clearColor", "#00000000" if transparent else "#FF000000")
        return SplashCatalog.color(str(app.section("splash").get("background", fallback)))

    @staticmethod
    def write(tree: GeneratedTree, app: AppManifest, engine_logo: Path) -> None:
        splash = app.section("splash")
        logo = app.folder / "content" / str(splash["logo"]) if "logo" in splash else engine_logo
        name = f"splash-logo{logo.suffix.lower()}"
        image: dict = {"images": [{"filename": name, "idiom": "universal"}], **SplashCatalog.INFO}
        if logo.suffix.lower() == ".svg":
            image["properties"] = {"preserves-vector-representation": True}
        dark = SplashCatalog.color(str(splash["darkBackground"])) if "darkBackground" in splash else None

        folder = SplashCatalog.FOLDER
        tree.copy(f"{folder}/splash-logo.imageset/{name}", logo)
        tree.write_bytes(f"{folder}/splash-logo.imageset/Contents.json", SplashCatalog.contents(image))
        tree.write_bytes(f"{folder}/splash-background.colorset/Contents.json", SplashCatalog.contents(SplashCatalog.colorset(SplashCatalog.background(app), dark)))
        tree.write_bytes(f"{folder}/Contents.json", SplashCatalog.contents(SplashCatalog.INFO))
