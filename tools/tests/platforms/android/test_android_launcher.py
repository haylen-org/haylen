import re

import pytest

from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.platforms.android.android_launcher import AndroidLauncher
from haylen.platforms.build_product import BuildProduct
from haylen.platforms.launch_request import LaunchRequest
from tests.platforms.android.android_fixture import AndroidFixture


class AndroidLauncherTest:
    @staticmethod
    def adb(command: list[object]) -> str:
        """Answers the commands of adb that list devices and read their properties, for an emulator and a phone that waits for its authorization."""
        arguments = [str(part) for part in command]
        if arguments[1:] == ["devices"]:
            return "List of devices attached\nemulator-5554\tdevice\nR58M\tunauthorized\n\n"
        if arguments[-2:] == ["avd", "name"]:
            return "Pixel_10a\nOK\n"
        if arguments[-1] == "ro.build.version.release":
            return "17\n"
        if arguments[-1] == "ro.product.cpu.abi":
            return "x86_64\n"
        raise AssertionError(f"Unexpected command {arguments}.")

    def test_the_log_shows_the_engine_as_it_writes_and_other_tags_with_their_level(self):
        assert AndroidLauncher.log_line("I/haylen  : The app started.") == "The app started."
        assert AndroidLauncher.log_line("W/camera  : No camera.") == "W/camera  : No camera."
        assert AndroidLauncher.log_line("--------- beginning of main") is None

    def test_devices_name_the_emulators_and_the_devices_that_are_not_ready(self, tmp_path, monkeypatch):
        fixture = AndroidFixture(tmp_path)
        monkeypatch.setattr(fixture.context.process, "capture", AndroidLauncherTest.adb)

        devices = [device.describe() for device in AndroidLauncher(fixture.context).devices()]

        assert devices == [
            {"identifier": "emulator-5554", "name": "Pixel_10a, Android 17", "kind": "emulator", "state": "ready", "targets": ["android"]},
            {"identifier": "R58M", "name": "R58M", "kind": "device", "state": "unauthorized", "targets": ["android"]},
        ]

    def test_an_app_built_without_the_abi_of_the_device_stops_before_it_installs(self, tmp_path, monkeypatch):
        fixture = AndroidFixture(tmp_path)
        monkeypatch.setattr(fixture.context.process, "capture", AndroidLauncherTest.adb)
        prepared = fixture.prepare(fixture.resolve(fixture.app({}), architectures=("arm64-v8a",)))
        product = BuildProduct(prepared, tmp_path / "app.apk", "apk", (), ())
        launch = LaunchRequest("emulator-5554", True, "127.0.0.1", 8000, 4711, {})

        with pytest.raises(UnsupportedTarget, match=re.escape('runs "x86_64", and the app was built for "arm64-v8a". Build it for the device with "--architecture x86_64"')):
            AndroidLauncher(fixture.context).run(product, launch)
