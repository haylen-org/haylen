import json
import re

import pytest

from haylen.core.errors.invalid_manifest import InvalidManifest
from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.platforms.apple.apple_project import AppleProject
from haylen.toolchains.apple_simulators import AppleSimulators
from tests.platforms.apple.apple_fixture import AppleFixture
from tests.support.recorded_commands import RecordedCommands


class IosDevicesTest:
    """The devices that an iOS app runs on, which "platforms.ios.devices" of `haylen.json` names: the device family of the Xcode project, the orientations of each device in the Info.plist, the simulator that a run takes and the Mac Catalyst app, which needs iPads."""

    LANDSCAPE = ["UIInterfaceOrientationLandscapeLeft", "UIInterfaceOrientationLandscapeRight"]
    PAD_PORTRAIT = ["UIInterfaceOrientationPortrait", "UIInterfaceOrientationPortraitUpsideDown"]

    @pytest.fixture
    def apple(self, tmp_path, monkeypatch) -> AppleFixture:
        return AppleFixture(tmp_path, monkeypatch)

    @staticmethod
    def project(apple: AppleFixture, devices: list[str] | None, target: str = "ios-simulator", orientation: str = "portrait") -> tuple[dict, dict]:
        """The build settings of the app target and the Info.plist of an app with these devices."""
        document: dict = {"orientation": orientation}
        if devices is not None:
            document["platforms"] = {"ios": {"devices": devices}}
        prepared = apple.prepare(apple.app(document), target)
        settings = AppleFixture.spec(prepared)["targets"][AppleProject.TARGET]["settings"]["base"]
        return settings, AppleFixture.plist(prepared, AppleProject.INFO_PLIST)

    def test_an_app_runs_on_iphones_and_ipads_by_default(self, apple):
        settings, info = IosDevicesTest.project(apple, None)
        assert settings["TARGETED_DEVICE_FAMILY"] == "1,2"
        assert info["UISupportedInterfaceOrientations"] == ["UIInterfaceOrientationPortrait"]
        assert info["UISupportedInterfaceOrientations~ipad"] == IosDevicesTest.PAD_PORTRAIT

        product = apple.resolve(apple.app({}))
        assert product.primary.setting("ios", "devices") == ("iphone", "ipad")
        assert "the default of the backend" in product.primary.setting_origins()["ios"]["devices"]["origin"]

    def test_an_app_for_iphones_alone_lists_the_orientations_of_iphones(self, apple):
        settings, info = IosDevicesTest.project(apple, ["iphone"])
        assert settings["TARGETED_DEVICE_FAMILY"] == "1"
        assert info["UISupportedInterfaceOrientations"] == ["UIInterfaceOrientationPortrait"]
        assert "UISupportedInterfaceOrientations~ipad" not in info

        product = apple.resolve(apple.app({"platforms": {"ios": {"devices": ["iphone"]}}}))
        assert product.primary.setting_origins()["ios"]["devices"] == {"value": ["iphone"], "origin": f"{product.app.origin} at platforms.ios.devices"}

    def test_an_app_for_ipads_alone_lists_the_orientations_of_ipads_for_every_device(self, apple):
        settings, info = IosDevicesTest.project(apple, ["ipad"])
        assert settings["TARGETED_DEVICE_FAMILY"] == "2"
        assert info["UISupportedInterfaceOrientations"] == IosDevicesTest.PAD_PORTRAIT
        assert "UISupportedInterfaceOrientations~ipad" not in info

    def test_the_order_of_the_devices_never_changes_the_project(self, apple):
        assert IosDevicesTest.project(apple, ["ipad", "iphone"], orientation="landscape") == IosDevicesTest.project(apple, None, orientation="landscape")

    def test_the_mac_catalyst_app_is_the_ipad_app(self, apple):
        settings, _ = IosDevicesTest.project(apple, ["ipad"], "catalyst")
        assert settings["TARGETED_DEVICE_FAMILY"] == "2"
        app = apple.app({"platforms": {"ios": {"devices": ["iphone"]}}}, "phone")
        expected = (
            f'The Mac Catalyst app is the iPad app on a Mac, and "platforms.ios.devices" of `{apple.resolve(app).app.origin}` names iPhones alone. Add "ipad" to it to build the app for Mac Catalyst.'
        )
        with pytest.raises(UnsupportedTarget, match=re.escape(expected)):
            apple.resolve(app, "catalyst")

    def test_the_devices_name_iphones_or_ipads_at_least_once(self, apple):
        for devices in ([], ["iphone", "iphone"], ["watch"]):
            with pytest.raises(InvalidManifest, match=r'In "platforms\.ios"'):
                apple.resolve(apple.app({"platforms": {"ios": {"devices": devices}}}))

    def test_the_device_family_of_the_build_settings_follows_the_devices_of_the_app(self, apple):
        repeated = apple.app({"platforms": {"ios": {"devices": ["iphone"]}, "apple": {"buildSettings": {"TARGETED_DEVICE_FAMILY": "1"}}}}, "repeated")
        assert AppleFixture.spec(apple.prepare(repeated))["targets"][AppleProject.TARGET]["settings"]["base"]["TARGETED_DEVICE_FAMILY"] == "1"
        contradicting = apple.app({"platforms": {"apple": {"buildSettings": {"TARGETED_DEVICE_FAMILY": "1"}}}}, "contradicting")
        origin = apple.resolve(contradicting).app.origin
        expected = f'The key "platforms.apple.buildSettings.TARGETED_DEVICE_FAMILY" is "1", while "platforms.ios.devices" of `{origin}` gives "1,2".'
        with pytest.raises(InvalidManifest, match=re.escape(expected)):
            apple.prepare(contradicting)

    @staticmethod
    def simulators(tmp_path, *entries: tuple[str, str, str]) -> AppleSimulators:
        devices = [{"udid": udid, "name": name, "state": state, "deviceTypeIdentifier": f"com.apple.CoreSimulator.SimDeviceType.{name.replace(' ', '-')}"} for udid, name, state in entries]
        listing = json.dumps({"devices": {"com.apple.CoreSimulator.SimRuntime.iOS-27-2": devices}})
        return AppleSimulators(RecordedCommands(tmp_path, answer=lambda command: listing).context)

    def test_a_run_takes_a_simulator_of_the_devices_of_the_app(self, tmp_path):
        phone = ("PHONE", "iPhone 18", "Shutdown")
        pad = ("PAD", "iPad Air 13-inch", "Booted")
        simulators = IosDevicesTest.simulators(tmp_path, phone, pad)
        assert simulators.find("iOS", None)["udid"] == "PAD"
        assert simulators.find("iOS", None, ("iphone",))["udid"] == "PHONE"
        assert simulators.find("iOS", None, ("ipad",))["udid"] == "PAD"
        assert simulators.find("iOS", "PAD", ("iphone",))["udid"] == "PAD", "iPadOS runs the apps of iPhones."
        with pytest.raises(
            UnsupportedTarget, match=re.escape('The app runs on iPads alone, as "platforms.ios.devices" of its `haylen.json` says, so it does not install on the iPhone simulator "iPhone 18".')
        ):
            simulators.find("iOS", "PHONE", ("ipad",))

        phones = IosDevicesTest.simulators(tmp_path, phone)
        assert phones.find("iOS", None, ("iphone",))["udid"] == "PHONE"
        with pytest.raises(UnsupportedTarget, match="no iPad simulator is installed"):
            phones.find("iOS", None, ("ipad",))
        assert IosDevicesTest.simulators(tmp_path, pad).find("iOS", None, ("iphone",))["udid"] == "PAD"
