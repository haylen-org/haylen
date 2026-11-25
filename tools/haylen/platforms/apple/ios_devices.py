from haylen.core.app_manifest import AppManifest
from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.platforms.apple.apple_target import AppleTarget


class IosDevices:
    """The devices that an iOS app runs on, iPhones, iPads or both, which "platforms.ios.devices" of `haylen.json` names and which are both when it is missing. They give the Xcode project its device family, the Info.plist the orientations of each device and `haylen app run` the simulators and devices it takes. The Mac Catalyst app is the iPad app on a Mac, so it needs iPads among them."""

    SECTION = "ios"
    KEY = "devices"
    IPHONE = "iphone"
    IPAD = "ipad"
    DEFAULT = (IPHONE, IPAD)
    # The values of `TARGETED_DEVICE_FAMILY` that name each device.
    FAMILIES = {IPHONE: "1", IPAD: "2"}
    SCHEMA = {
        "type": "array",
        "items": {"enum": list(DEFAULT)},
        "minItems": 1,
        "uniqueItems": True,
        "default": list(DEFAULT),
        "description": 'The devices that the iOS app runs on, "iphone", "ipad" or both, both by default. An app for iPhones alone also runs on iPads in the iPhone compatibility mode of iPadOS, and the Mac Catalyst app needs "ipad".',
    }

    @staticmethod
    def of(app: AppManifest) -> tuple[str, ...]:
        """The devices of the app in a fixed order, whatever order `haylen.json` names them in."""
        named = app.platforms.get(IosDevices.SECTION, {}).get(IosDevices.KEY, IosDevices.DEFAULT)
        return tuple(device for device in IosDevices.DEFAULT if device in named)

    @staticmethod
    def check(app: AppManifest, target: AppleTarget) -> None:
        """Fails for a target that the devices of the app contradict, which is the Mac Catalyst app of an app without iPads."""
        if target.name == "catalyst" and IosDevices.IPAD not in IosDevices.of(app):
            raise UnsupportedTarget(
                f'The Mac Catalyst app is the iPad app on a Mac, and "platforms.ios.devices" of `{app.origin}` names iPhones alone. Add "ipad" to it to build the app for Mac Catalyst.',
                origin=app.origin,
            )

    @staticmethod
    def device_family(app: AppManifest, target: AppleTarget) -> str:
        """The value of `TARGETED_DEVICE_FAMILY` of a target: the devices of the app on iPhones, iPads and Mac Catalyst, and the family of the target elsewhere."""
        if target.folder != "ios":
            return target.device_family
        return ",".join(IosDevices.FAMILIES[device] for device in IosDevices.of(app))
