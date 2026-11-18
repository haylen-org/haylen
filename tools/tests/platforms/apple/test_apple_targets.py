import json

import pytest

from haylen.core.app_colors import AppColors
from haylen.core.app_kind import AppKind
from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.platforms.apple.apple_engine_port import AppleEnginePort
from haylen.platforms.apple.apple_project import AppleProject
from haylen.platforms.apple.apple_targets import AppleTargets
from haylen.platforms.apple.splash_catalog import SplashCatalog
from haylen.toolchains.apple_sdk import AppleSdk
from haylen.toolchains.apple_simulators import AppleSimulators
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

    def test_a_simulator_that_is_off_boots_then_waits_and_shuts_down_afterwards(self, tmp_path):
        # The flag of `bootstatus` that boots fails with "Invalid argument" on the foldable iPhone, so booting and waiting are two commands.
        duo = {"udid": "DUO", "name": "iPhone Duo", "state": "Shutdown"}
        recorded = RecordedCommands(tmp_path, answer=lambda command: json.dumps({"devices": {"com.apple.CoreSimulator.SimRuntime.iOS-27-2": [duo]}}))
        with AppleSimulators(recorded.context).booted("iOS", "DUO"):
            pass
        assert recorded.commands[1:] == [["xcrun", "simctl", "boot", "DUO"], ["xcrun", "simctl", "bootstatus", "DUO"], ["xcrun", "simctl", "shutdown", "DUO"]]

    def test_the_template_holds_no_generated_project(self):
        files = [path.relative_to(AppleProject.TEMPLATE).as_posix() for path in AppleProject.TEMPLATE.rglob("*") if path.is_file()]
        assert not any(".xcodeproj" in name or name.endswith("project.yml") for name in files)

    def test_storyboards_of_apple_tv_are_documents_of_apple_tv(self):
        # The interface compiler of tvOS fails without a message on a storyboard of the iOS document type.
        for path in AppleProject.TEMPLATE.rglob("*.storyboard"):
            text = path.read_text(encoding="utf-8")
            assert ('targetRuntime="AppleTV"' in text) == ('type="com.apple.InterfaceBuilder.AppleTV.Storyboard"' in text), path

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
