import json
from pathlib import Path

import pytest

from haylen.core.app_colors import AppColors
from haylen.core.app_kind import AppKind
from haylen.core.errors.build_failed import BuildFailed
from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.platforms.apple.apple_engine_port import AppleEnginePort
from haylen.platforms.apple.apple_project import AppleProject
from haylen.platforms.apple.apple_targets import AppleTargets
from haylen.platforms.apple.splash_catalog import SplashCatalog
from haylen.toolchains.apple_sdk import AppleSdk
from haylen.toolchains.apple_simulators import AppleSimulators
from haylen.toolchains.simulator_diagnostics import SimulatorDiagnostics
from tests.platforms.apple.apple_fixture import AppleFixture
from tests.support.recorded_commands import RecordedCommands


class AppleTargetsTest:
    """The targets of the Apple backend and the slices of its engine port, which match by what they describe, the template of the targets and the splash of their launch screens."""

    @pytest.fixture
    def apple(self, tmp_path, monkeypatch) -> AppleFixture:
        return AppleFixture(tmp_path, monkeypatch)

    def test_every_target_has_the_slice_of_the_engine_port_that_serves_it(self, apple):
        for target in AppleTargets.ALL:
            port, port_target = apple.registry.port_for(apple.registry.target(target.name).description)
            assert (port.name, port_target.name) == ("apple", target.slice)

    def test_the_mac_target_is_the_default_of_cpp_apps_on_macos(self, apple, monkeypatch):
        monkeypatch.setattr("haylen.core.host.Host.system", staticmethod(lambda: "macos"))
        assert apple.registry.default_target(AppKind.CPP).description.name == "macos"

    def test_visionos_builds_its_own_slices_and_takes_the_platform_visionos(self):
        device = AppleSdk.cmake_options("xros", "arm64")
        assert "-DCMAKE_SYSTEM_NAME=visionOS" in device
        assert "-DCMAKE_OSX_SYSROOT=xros" in device
        assert "-DCMAKE_OSX_SYSROOT=xrsimulator" in AppleSdk.cmake_options("xros-simulator", "arm64")
        assert (AppleTargets.named("visionos").slice, AppleTargets.named("visionos-simulator").slice) == ("xros", "xros-simulator")
        assert AppleTargets.named("visionos-simulator").platform == "visionos"
        assert AppleTargets.named("visionos-simulator").architectures == ("arm64",)

    def test_a_request_builds_the_architectures_of_a_slice_that_it_names(self):
        simulator = AppleTargets.of_slice("ios-simulator")
        assert AppleEnginePort.architectures(simulator, None) == ("arm64", "x86_64")
        assert AppleEnginePort.architectures(simulator, ("arm64",)) == ("arm64",)
        with pytest.raises(UnsupportedTarget, match='"xros-simulator" builds "arm64"'):
            AppleEnginePort.architectures(AppleTargets.of_slice("xros-simulator"), ("x86_64",))

    def test_simulators_and_this_mac_build_the_architecture_of_this_mac(self):
        assert AppleTargets.named("ios-simulator").default_architectures("x86_64") == ("x86_64",)
        assert AppleTargets.named("visionos-simulator").default_architectures("x86_64") == ("arm64",)
        assert AppleTargets.named("ios").default_architectures("x86_64") == ("arm64",)

    def test_extensions_build_for_every_apple_platform_but_mac_catalyst(self, apple):
        app = apple.app({"plugins": {"shelf": {}}})
        extension = {"type": "appExtension", "platforms": ["catalyst"], "name": "Shelf", "kind": "appExtension", "point": "com.apple.tv-top-shelf", "sources": "shelf"}
        apple.plugin(app, "shelf", {"implementations": {"catalyst": "native"}, "contributions": [extension]})
        with pytest.raises(UnsupportedTarget, match=r'"appExtension" .* no backend of "catalyst" reads'):
            apple.resolve(app, "catalyst")

    def test_a_missing_runtime_names_the_platform_that_xcodebuild_downloads(self, tmp_path):
        recorded = RecordedCommands(tmp_path, answer=lambda command: json.dumps({"devices": {}}))
        with pytest.raises(UnsupportedTarget, match='"xcodebuild -downloadPlatform visionOS"'):
            AppleSimulators(recorded.context).find("xrOS", None)

    @staticmethod
    def simulator(tmp_path, monkeypatch, report: str, stops: bool = False, stopped: dict[str, bool] | None = None) -> RecordedCommands:
        """Commands of a phone simulator that is off until it boots, and with `stops` shuts down by itself after its boot, so reading its log fails, as a runner of CI showed, or shuts down once `stopped` says so. The diagnostics of the simulator read the folders of this test instead of those of the Mac."""
        monkeypatch.setattr(SimulatorDiagnostics, "SERVICE_LOG", tmp_path / "CoreSimulator.log")
        monkeypatch.setattr(SimulatorDiagnostics, "CRASH_REPORTS", tmp_path / "DiagnosticReports")
        recorded: RecordedCommands | None = None

        def answer(command):
            running = ["xcrun", "simctl", "boot", "PHONE"] in recorded.commands and not stops and not (stopped or {}).get("now", False)
            if command[0] == "log":
                return "The log of the store of the simulator."
            if "log" in command:
                if not running:
                    raise BuildFailed('The command "xcrun" failed with exit code 149.', details="Unable to lookup in current state: Shutdown")
                return report
            phone = {"udid": "PHONE", "name": "iPhone 18 Pro", "state": "Booted" if running else "Shutdown", "dataPath": str(tmp_path / "data")}
            return json.dumps({"devices": {"com.apple.CoreSimulator.SimRuntime.iOS-27-2": [phone]}})

        recorded = RecordedCommands(tmp_path, answer=answer)
        return recorded

    def test_a_simulator_that_is_off_boots_then_waits_and_shuts_down_afterwards(self, tmp_path, monkeypatch):
        # The flag of `bootstatus` that boots fails with "Invalid argument" on the foldable iPhone, so booting and waiting are two commands.
        report = "2026-11-19 21:38:13.506 Df SpringBoard[14233:b04a] [com.apple.UserNotifications:DataProviderFactory] notificationSourcesDidInstall: 99 sources, initialLoad=1"
        recorded = AppleTargetsTest.simulator(tmp_path, monkeypatch, report)
        with AppleSimulators(recorded.context).booted("iOS", "PHONE"):
            pass
        boot = [["xcrun", "simctl", "boot", "PHONE"], ["xcrun", "simctl", "bootstatus", "PHONE"]]
        assert recorded.commands[1:3] == boot
        assert recorded.commands[3][:6] == ["xcrun", "simctl", "spawn", "PHONE", "log", "show"]
        assert recorded.commands[3][-1] == AppleSimulators.NOTIFICATION_SOURCES
        assert recorded.commands[-1] == ["xcrun", "simctl", "shutdown", "PHONE"]

    def test_a_booted_ios_simulator_waits_for_its_notification_center_and_shuts_down_when_it_never_loads(self, tmp_path, monkeypatch):
        # An app that installs before the notification center loaded its apps never shows a notification, which a runner of CI that builds while the simulator boots showed for minutes.
        recorded = AppleTargetsTest.simulator(tmp_path, monkeypatch, "")
        monkeypatch.setattr(AppleSimulators, "NOTIFICATION_SECONDS", 0)
        with pytest.raises(BuildFailed, match=r'did not load its apps .* The simulator is "Booted"'), AppleSimulators(recorded.context).booted("iOS", "PHONE"):
            pass
        assert recorded.commands[-1] == ["xcrun", "simctl", "shutdown", "PHONE"]
        tvs = {"udid": "TV", "name": "Apple TV", "state": "Shutdown"}
        recorded = RecordedCommands(tmp_path, answer=lambda command: json.dumps({"devices": {"com.apple.CoreSimulator.SimRuntime.tvOS-27-0": [tvs]}}))
        with AppleSimulators(recorded.context).booted("tvOS", "TV"):
            pass
        assert not any("log" in command for command in recorded.commands)

    def test_a_simulator_that_stops_during_the_wait_says_so_keeps_its_logs_and_is_not_shut_down_again(self, tmp_path, monkeypatch):
        # A runner of CI shut the simulator down during the wait, and shutting it down again failed with "Unable to shutdown device in current state: Shutdown", which hid why the wait ended.
        recorded = AppleTargetsTest.simulator(tmp_path, monkeypatch, "", stops=True)
        (tmp_path / "CoreSimulator.log").write_text("Before the run.\n", encoding="utf-8")
        simulators = AppleSimulators(recorded.context)
        simulator = simulators.find("iOS", "PHONE")
        kept = tmp_path / "results"
        with pytest.raises(BuildFailed, match=r'"iPhone 18 Pro" stopped \d+ seconds after its boot, .* the Mac lists it as "Shutdown"') as failure, simulators.use(simulator, "iOS", kept):
            pass
        assert failure.value.details == "Unable to lookup in current state: Shutdown"
        assert ["xcrun", "simctl", "shutdown", "PHONE"] not in recorded.commands
        assert json.loads((kept / SimulatorDiagnostics.SIMULATOR_FILE).read_text(encoding="utf-8"))["stateAtTheEnd"] == "Shutdown"
        log = next(command for command in recorded.commands if command[:2] == ["log", "show"])
        assert log[:4] == ["log", "show", "-D", str(tmp_path / "data" / "var" / "db" / "diagnostics")]
        assert (kept / SimulatorDiagnostics.LOG_FILE).read_text(encoding="utf-8") == "The log of the store of the simulator."
        assert (kept / SimulatorDiagnostics.SERVICE_FILE).read_bytes() == b""

    def test_a_failure_of_a_simulator_that_stopped_stays_the_error_of_the_run(self, tmp_path, monkeypatch):
        stopped = {"now": False}
        recorded = AppleTargetsTest.simulator(tmp_path, monkeypatch, "initialLoad=1", stopped=stopped)
        simulators = AppleSimulators(recorded.context)
        with pytest.raises(BuildFailed, match=r"The UI tests failed\.$"), simulators.use(simulators.find("iOS", "PHONE"), "iOS"):
            stopped["now"] = True
            raise BuildFailed("The UI tests failed.")
        assert ["xcrun", "simctl", "shutdown", "PHONE"] not in recorded.commands

    def test_the_diagnostics_keep_what_the_service_wrote_during_the_run_and_the_crash_reports_of_the_simulator(self, tmp_path):
        assert SimulatorDiagnostics.service_log(b"before\nduring\n", 7) == b"during\n"
        assert SimulatorDiagnostics.service_log(b"new log\n", 100) == b"new log\n"
        reports = tmp_path / "reports"
        reports.mkdir()
        (reports / "SpringBoard.ips").write_text("/Devices/PHONE/data/Containers", encoding="utf-8")
        (reports / "Other.ips").write_text("/Devices/OTHER/data/Containers", encoding="utf-8")
        assert [path.name for path in SimulatorDiagnostics.crash_reports(reports, "PHONE", 0.0)] == ["SpringBoard.ips"]
        assert SimulatorDiagnostics.crash_reports(reports, "PHONE", 4102444800.0) == []
        assert SimulatorDiagnostics.log_command(Path("/data"), 1764190000.4) == ["log", "show", "-D", "/data/var/db/diagnostics", "--start", "@1764190000", "--style", "compact"]

    def test_the_template_holds_no_generated_project(self):
        files = [path.relative_to(AppleProject.TEMPLATE).as_posix() for path in AppleProject.TEMPLATE.rglob("*") if path.is_file()]
        assert not any(".xcodeproj" in name or name.endswith("project.yml") for name in files)

    def test_storyboards_of_apple_tv_are_documents_of_apple_tv(self):
        # The interface compiler of tvOS fails without a message on a storyboard of the iOS document type.
        for path in AppleProject.TEMPLATE.rglob("*.storyboard"):
            text = path.read_text(encoding="utf-8")
            assert ('targetRuntime="AppleTV"' in text) == ('type="com.apple.InterfaceBuilder.AppleTV.Storyboard"' in text), path

    def test_launch_screens_center_the_logo_in_the_whole_view(self):
        # The engine covers the window with the same storyboard, whose safe area differs from the one of the system launch screen on an iPad, so a logo placed by the safe area would move as the cover takes over.
        for path in AppleProject.TEMPLATE.rglob("*.storyboard"):
            text = path.read_text(encoding="utf-8")
            assert 'secondItem="safe"' not in text, path
            assert 'firstItem="logo" firstAttribute="centerY" secondItem="root" secondAttribute="centerY"' in text, path

    def test_the_dark_background_becomes_the_dark_appearance(self):
        colorset = SplashCatalog.colorset((16, 20, 24, 255), (0, 0, 0, 255))
        assert len(colorset["colors"]) == 2
        assert "appearances" not in colorset["colors"][0]
        assert colorset["colors"][1]["appearances"] == [{"appearance": "luminosity", "value": "dark"}]
        assert colorset["colors"][1]["color"]["components"]["red"] == "0.000"
        assert len(SplashCatalog.colorset((16, 20, 24, 255), None)["colors"]) == 1
        assert AppColors.components("#80102030") == (16, 32, 48, 128)
        assert AppColors.components("#102030") == (16, 32, 48, 255)

    def test_launch_screens_show_the_logo_and_background_of_the_splash(self, apple):
        app = apple.app({"splash": {"background": "#FF101418"}, "clearColor": "#FF000000"})
        project = apple.prepare(app).folders.project
        background = json.loads((project / "Splash.xcassets" / "splash-background.colorset" / "Contents.json").read_text(encoding="utf-8"))
        assert background["colors"][0]["color"]["components"]["green"] == "0.078"
        assert (project / "Splash.xcassets" / "splash-logo.imageset" / "splash-logo.svg").is_file()
        assert not (apple.prepare(app, "macos").folders.project / "Splash.xcassets").exists()
