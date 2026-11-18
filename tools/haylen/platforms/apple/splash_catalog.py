import json
from pathlib import Path

from haylen.core.app_colors import AppColors
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
        return AppColors.components(AppColors.splash_background(app))

    @staticmethod
    def write(tree: GeneratedTree, app: AppManifest, engine_logo: Path) -> None:
        splash = app.section("splash")
        logo = app.folder / "content" / str(splash["logo"]) if "logo" in splash else engine_logo
        name = f"splash-logo{logo.suffix.lower()}"
        image: dict = {"images": [{"filename": name, "idiom": "universal"}], **SplashCatalog.INFO}
        if logo.suffix.lower() == ".svg":
            image["properties"] = {"preserves-vector-representation": True}
        dark = AppColors.components(str(splash["darkBackground"])) if "darkBackground" in splash else None

        folder = SplashCatalog.FOLDER
        tree.copy(f"{folder}/splash-logo.imageset/{name}", logo)
        tree.write_bytes(f"{folder}/splash-logo.imageset/Contents.json", SplashCatalog.contents(image))
        tree.write_bytes(f"{folder}/splash-background.colorset/Contents.json", SplashCatalog.contents(SplashCatalog.colorset(SplashCatalog.background(app), dark)))
        tree.write_bytes(f"{folder}/Contents.json", SplashCatalog.contents(SplashCatalog.INFO))
