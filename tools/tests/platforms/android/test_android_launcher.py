import re
import subprocess

import pytest

from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.development.app_run import AppRun
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
            {"identifier": "emulator-5554", "name": "Pixel_10a", "kind": "emulator", "system": "Android", "version": "17", "state": "ready", "targets": ["android"]},
            {"identifier": "R58M", "name": "R58M", "kind": "device", "system": "Android", "version": "", "state": "unauthorized", "targets": ["android"]},
        ]

    def test_an_app_built_without_the_abi_of_the_device_stops_before_it_installs(self, tmp_path, monkeypatch):
        fixture = AndroidFixture(tmp_path)
        monkeypatch.setattr(fixture.context.process, "capture", AndroidLauncherTest.adb)
        prepared = fixture.prepare(fixture.resolve(fixture.app({}), architectures=("arm64-v8a",)))
        product = BuildProduct(prepared, tmp_path / "app.apk", "apk", (), ())
        launch = LaunchRequest("emulator-5554", True, "127.0.0.1", 8000, 4711, {})

        with pytest.raises(UnsupportedTarget, match=re.escape('runs "x86_64", and the app was built for "arm64-v8a". Build it for the device with "--architecture x86_64"')):
            AndroidLauncher(fixture.context).run(product, launch)

    def test_a_run_follows_the_log_of_the_engine_the_plugins_and_the_java_crashes_and_quitting_stops_the_app(self, tmp_path, monkeypatch):
        fixture = AndroidFixture(tmp_path)
        app = fixture.app({"plugins": {"camera": {}}})
        fixture.plugin(app, "camera", {})
        prepared = fixture.prepare(fixture.resolve(app, architectures=("x86_64",)))
        product = BuildProduct(prepared, tmp_path / "app.apk", "apk", (), ())
        launch = LaunchRequest("emulator-5554", False, "127.0.0.1", 8000, 4711, {})
        ran: list[list[str]] = []
        followed: list[list[str]] = []
        stops = []
        monkeypatch.setattr(fixture.context.process, "capture", AndroidLauncherTest.adb)
        monkeypatch.setattr(fixture.context.process, "run", lambda command, **options: ran.append([str(part) for part in command[1:]]))
        monkeypatch.setattr(AndroidLauncher, "pid", lambda self, serial, package: "4242")
        monkeypatch.setattr(AndroidLauncher, "watch", lambda self, run, serial, package, pid: None)
        monkeypatch.setattr(AppRun, "show", lambda self, device, stop=None: stops.append(stop))
        monkeypatch.setattr(AppRun, "follow", lambda self, command, **options: followed.append([str(part) for part in command[1:]]))

        AndroidLauncher(fixture.context).run(product, launch)

        assert followed == [["-s", "emulator-5554", "logcat", "-v", "tag", "--pid=4242", "haylen:V", "camera:V", "AndroidRuntime:E", "*:S"]]
        assert ran[0] == ["-s", "emulator-5554", "install", "-r", str(tmp_path / "app.apk")]

        stopped: list[list[str]] = []
        monkeypatch.setattr(subprocess, "run", lambda command, **options: stopped.append(command[1:]))
        stops[0]()
        assert stopped == [["-s", "emulator-5554", "shell", "am", "force-stop", "com.haylen.tests"]]
