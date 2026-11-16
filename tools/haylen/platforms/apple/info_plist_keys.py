from haylen.core.app_manifest import AppManifest
from haylen.platforms.apple.apple_target import AppleTarget


class InfoPlistKeys:
    """The keys that the engine gives the Info.plist of an app from `haylen.json`: on iPhones and iPads every orientation, since the runtime allows the ones of `orientation` and a foldable iPhone shows an app that lists fewer in a box, the refresh rates above 60 hertz that iPhones allow only with `CADisableMinimumFrameDurationOnPhone` and the status bar the app launches with, the background mode of the audio that plays in the background, and on macOS whether the app shows in the Dock."""

    PHONE_ORIENTATIONS = ("UIInterfaceOrientationPortrait", "UIInterfaceOrientationLandscapeLeft", "UIInterfaceOrientationLandscapeRight")
    PAD_ORIENTATIONS = ("UIInterfaceOrientationPortrait", "UIInterfaceOrientationPortraitUpsideDown", "UIInterfaceOrientationLandscapeLeft", "UIInterfaceOrientationLandscapeRight")
    STATUS_BAR_STYLES = {"auto": "UIStatusBarStyleDefault", "light": "UIStatusBarStyleLightContent", "dark": "UIStatusBarStyleDarkContent"}

    @staticmethod
    def of(app: AppManifest, target: AppleTarget) -> dict[str, object]:
        window = app.section("window")
        if target.folder == "macos":
            return {} if window.get("showInTaskbar", True) else {"LSUIElement": True}

        keys: dict[str, object] = {}
        if target.folder == "ios":
            status_bar = str(window.get("statusBar", "hidden"))
            keys["UISupportedInterfaceOrientations"] = list(InfoPlistKeys.PHONE_ORIENTATIONS)
            keys["UISupportedInterfaceOrientations~ipad"] = list(InfoPlistKeys.PAD_ORIENTATIONS)
            keys["CADisableMinimumFrameDurationOnPhone"] = True
            keys["UIStatusBarHidden"] = status_bar == "hidden"
            if status_bar != "hidden":
                keys["UIStatusBarStyle"] = InfoPlistKeys.STATUS_BAR_STYLES[status_bar]
        if app.section("audio").get("background", False):
            keys["UIBackgroundModes"] = ["audio"]
        return keys
