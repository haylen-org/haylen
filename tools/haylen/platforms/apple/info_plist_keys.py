from haylen.core.app_category import AppCategory
from haylen.core.app_manifest import AppManifest
from haylen.platforms.apple.apple_target import AppleTarget
from haylen.platforms.apple.ios_devices import IosDevices


class InfoPlistKeys:
    """The keys that the engine gives the Info.plist of an app from `haylen.json`: on iPhones and iPads the orientations of `orientation` for each device of "platforms.ios.devices", which the system launches the app in, the refresh rates above 60 hertz that iPhones allow only with `CADisableMinimumFrameDurationOnPhone` and the status bar the app launches with, on macOS whether the app shows in the Dock, and on macOS and Mac Catalyst the category of the Mac App Store and the Finder from `category`, which an app without one leaves out."""

    LANDSCAPE = ("UIInterfaceOrientationLandscapeLeft", "UIInterfaceOrientationLandscapeRight")
    PHONE_ORIENTATIONS = {"landscape": LANDSCAPE, "portrait": ("UIInterfaceOrientationPortrait",), "any": ("UIInterfaceOrientationPortrait", *LANDSCAPE)}
    PAD_ORIENTATIONS = {
        "landscape": LANDSCAPE,
        "portrait": ("UIInterfaceOrientationPortrait", "UIInterfaceOrientationPortraitUpsideDown"),
        "any": ("UIInterfaceOrientationPortrait", "UIInterfaceOrientationPortraitUpsideDown", *LANDSCAPE),
    }
    STATUS_BAR_STYLES = {"auto": "UIStatusBarStyleDefault", "light": "UIStatusBarStyleLightContent", "dark": "UIStatusBarStyleDarkContent"}
    CATEGORIES = {
        AppCategory.GAME: "public.app-category.games",
        AppCategory.ENTERTAINMENT: "public.app-category.entertainment",
        AppCategory.EDUCATION: "public.app-category.education",
        AppCategory.PRODUCTIVITY: "public.app-category.productivity",
        AppCategory.UTILITY: "public.app-category.utilities",
        AppCategory.GRAPHICS: "public.app-category.graphics-design",
        AppCategory.MUSIC: "public.app-category.music",
        AppCategory.VIDEO: "public.app-category.video",
        AppCategory.SOCIAL: "public.app-category.social-networking",
        AppCategory.DEVELOPER: "public.app-category.developer-tools",
    }

    @staticmethod
    def of(app: AppManifest, target: AppleTarget) -> dict[str, object]:
        window = app.section("window")
        keys: dict[str, object] = {}
        if target.runs_on_mac and app.category is not None:
            keys["LSApplicationCategoryType"] = InfoPlistKeys.CATEGORIES[app.category]
        if target.folder == "macos":
            if not window.get("showInTaskbar", True):
                keys["LSUIElement"] = True
            return keys

        if target.folder == "ios":
            status_bar = str(window.get("statusBar", "hidden"))
            orientation = str(app.document.get("orientation", "landscape"))
            devices = IosDevices.of(app)
            # The key without a device names the orientations of every device that no key of its own names, so an app for iPads alone lists theirs in it.
            if IosDevices.IPHONE in devices:
                keys["UISupportedInterfaceOrientations"] = list(InfoPlistKeys.PHONE_ORIENTATIONS[orientation])
                if IosDevices.IPAD in devices:
                    keys["UISupportedInterfaceOrientations~ipad"] = list(InfoPlistKeys.PAD_ORIENTATIONS[orientation])
            else:
                keys["UISupportedInterfaceOrientations"] = list(InfoPlistKeys.PAD_ORIENTATIONS[orientation])
            keys["CADisableMinimumFrameDurationOnPhone"] = True
            keys["UIStatusBarHidden"] = status_bar == "hidden"
            if status_bar != "hidden":
                keys["UIStatusBarStyle"] = InfoPlistKeys.STATUS_BAR_STYLES[status_bar]
        return keys
