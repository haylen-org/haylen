import json
import plistlib
import re
from pathlib import Path

import pytest

from haylen.core.configuration import Configuration
from haylen.core.errors.invalid_option import InvalidOption
from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.core.repository import Repository
from haylen.native.engine_sdk_tree import EngineSdkTree
from haylen.platforms.apple.apple_engine_port import AppleEnginePort
from haylen.platforms.apple.apple_framework_check import AppleFrameworkCheck
from haylen.platforms.apple.apple_sdk_files import AppleSdkFiles
from haylen.platforms.apple.apple_targets import AppleTargets
from haylen.platforms.sdk_join_request import SdkJoinRequest
from haylen.platforms.sdk_request import SdkRequest
from haylen.sdk.sdk_slice import SdkSlice
from tests.support.recorded_commands import RecordedCommands
from tests.support.tooling_fixture import ToolingFixture


class AppleEnginePortTest:
    """The SDK that the Apple engine port installs: two frameworks with a library per slice, the shared files, and a manifest with one slice per system and environment, where a slice built later joins the slices of the same sources and configuration."""

    # The slice of a library that `xcodebuild -create-xcframework` takes, from the folder that the port built or kept it in, or from the framework of an SDK that joins others.
    SLICE = re.compile(r"apple-(?P<built>[a-z-]+?)-(arm64|x86_64|release|debug)|\.slices/(?P<kept>[a-z-]+)/|xcframework/[a-z]+-(?P<joined>[a-z-]+)/")

    @pytest.fixture
    def port(self, tmp_path, monkeypatch) -> tuple[AppleEnginePort, RecordedCommands]:
        monkeypatch.setattr(Repository, "ENGINE_BUILDS", tmp_path / "engine")
        monkeypatch.setattr(EngineSdkTree, "is_current", lambda tree: True)
        recorded = RecordedCommands(tmp_path)
        recorded.context.process.run = lambda command, **options: self.run(recorded, [str(part) for part in command])
        monkeypatch.setattr(AppleFrameworkCheck, "check", lambda check, library, sdk, frameworks: recorded.commands.append(["check", sdk, *frameworks]))
        return AppleEnginePort(recorded.context), recorded

    def run(self, recorded: RecordedCommands, command: list[str]) -> None:
        """Records a command and writes what the tool would, so the port finds the files of each step."""
        recorded.commands.append(command)
        if command[:2] == ["cmake", "--install"]:
            prefix = Path(command[command.index("--prefix") + 1])
            if command[command.index("--component") + 1] == "haylen-sdk":
                (prefix / "include" / "haylen").mkdir(parents=True)
                return
            for name in ("lib/libhaylen.a", "lib/libhaylen-lua.a", "lib/cmake/haylen/haylen-config.cmake"):
                (prefix / name).parent.mkdir(parents=True, exist_ok=True)
                (prefix / name).write_text(name, encoding="utf-8")
            frameworks = {platform: [f"{platform}Kit.framework"] for platform in ("iOS", "macCatalyst", "tvOS", "visionOS", "macOS")}
            (prefix / "share" / "haylen").mkdir(parents=True, exist_ok=True)
            (prefix / "share" / "haylen" / AppleSdkFiles.SETTINGS_NAME).write_text(json.dumps({"frameworks": frameworks}), encoding="utf-8")
        elif command[0] == "lipo":
            Path(command[-1]).parent.mkdir(parents=True, exist_ok=True)
            Path(command[-1]).write_text("joined", encoding="utf-8")
        elif command[:2] == ["xcodebuild", "-create-xcframework"]:
            output = Path(command[-1])
            libraries = []
            for index, part in enumerate(command):
                if part != "-library":
                    continue
                library = Path(command[index + 1])
                match = AppleEnginePortTest.SLICE.search(library.as_posix())
                target = AppleTargets.of_slice(match["built"] or match["kept"] or match["joined"])
                identifier = f"{target.xcframework_platform}-{target.slice}"
                (output / identifier).mkdir(parents=True)
                (output / identifier / library.name).write_text(target.slice, encoding="utf-8")
                entry = {"LibraryIdentifier": identifier, "LibraryPath": library.name, "SupportedPlatform": target.xcframework_platform}
                if target.xcframework_variant:
                    entry["SupportedPlatformVariant"] = target.xcframework_variant
                libraries.append(entry)
            (output / "Info.plist").write_bytes(plistlib.dumps({"AvailableLibraries": libraries}))

    def request(self, tmp_path: Path, targets: tuple[str, ...], folder: str = "sdk") -> SdkRequest:
        return SdkRequest(targets, Configuration.RELEASE, ("arm64",), ToolingFixture.REVISION, tmp_path / folder)

    def test_every_built_slice_checks_the_system_frameworks_of_its_platform(self, port, tmp_path):
        engine_port, recorded = port
        engine_port.build_sdk(self.request(tmp_path, ("macos", "ios-simulator")))
        assert [command for command in recorded.commands if command[0] == "check"] == [["check", "macosx", "macOSKit.framework"], ["check", "iphonesimulator", "iOSKit.framework"]]

    def test_the_sdk_holds_both_frameworks_and_a_slice_per_system_and_environment(self, port, tmp_path):
        engine_port, recorded = port
        manifest = engine_port.build_sdk(self.request(tmp_path, ("macos", "ios-simulator")))
        assert [(entry.system, entry.environment.value, entry.architectures) for entry in manifest.slices] == [("macos", "host", ("arm64",)), ("ios", "simulator", ("arm64",))]
        assert manifest.slices[1].libraries[SdkSlice.LUA] == f"{AppleSdkFiles.LUA_NAME}/ios-ios-simulator/{AppleSdkFiles.LUA_LIBRARY_NAME}"
        for name in (AppleSdkFiles.FRAMEWORK, AppleSdkFiles.LUA, AppleSdkFiles.SETTINGS, AppleSdkFiles.PRIVACY, AppleSdkFiles.SWIFT_HELPERS, AppleSdkFiles.CMAKE):
            assert manifest.file(name).exists(), name
        signed = [command[-1] for command in recorded.commands if command[:2] == ["codesign", "--sign"]]
        assert signed == [str(tmp_path / "sdk" / AppleSdkFiles.FRAMEWORK_NAME), str(tmp_path / "sdk" / AppleSdkFiles.LUA_NAME)]

    def test_a_slice_built_later_joins_the_slices_of_the_same_sources(self, port, tmp_path):
        engine_port, _ = port
        engine_port.build_sdk(self.request(tmp_path, ("macos",)))
        manifest = engine_port.build_sdk(self.request(tmp_path, ("tvos-simulator",)))
        assert [entry.system for entry in manifest.slices] == ["macos", "tvos"]
        assert (manifest.folder / manifest.slices[0].libraries[SdkSlice.ENGINE]).read_text(encoding="utf-8") == "macos"
        assert not (tmp_path / "sdk.slices").exists()

    def test_only_the_architectures_of_a_slice_build(self, port, tmp_path):
        engine_port, recorded = port
        engine_port.build_sdk(self.request(tmp_path, ("ios-simulator",)))
        built = [command for command in recorded.commands if command[:2] == ["cmake", "--build"]]
        assert [command[2] for command in built] == [str(tmp_path / "engine" / "apple-ios-simulator-arm64-release")]
        assert [command for command in recorded.commands if command[0] == "lipo"] == []

    def test_sdks_that_machines_built_side_by_side_join_into_one(self, port, tmp_path):
        engine_port, _ = port
        mac = engine_port.build_sdk(self.request(tmp_path, ("macos",), "mac"))
        ios = engine_port.build_sdk(self.request(tmp_path, ("ios", "ios-simulator"), "ios"))
        joined = engine_port.join_sdks(SdkJoinRequest((mac, ios), tmp_path / "joined"))
        assert [(entry.system, entry.environment.value) for entry in joined.slices] == [("macos", "host"), ("ios", "device"), ("ios", "simulator")]
        assert (joined.folder / joined.slices[2].libraries[SdkSlice.ENGINE]).read_text(encoding="utf-8") == "ios-simulator"
        for name in (AppleSdkFiles.SETTINGS, AppleSdkFiles.PRIVACY, AppleSdkFiles.SWIFT_HELPERS, AppleSdkFiles.CMAKE, AppleSdkFiles.INCLUDE):
            assert joined.file(name).exists(), name

    def test_sdks_join_only_into_a_folder_of_their_own_and_with_different_slices(self, port, tmp_path):
        engine_port, _ = port
        mac = engine_port.build_sdk(self.request(tmp_path, ("macos",), "mac"))
        again = engine_port.build_sdk(self.request(tmp_path, ("macos",), "again"))
        with pytest.raises(UnsupportedTarget, match='holds the slice "macos"'):
            engine_port.join_sdks(SdkJoinRequest((mac, again), tmp_path / "joined"))
        with pytest.raises(InvalidOption, match="holds one of the SDKs to join"):
            engine_port.join_sdks(SdkJoinRequest((mac,), tmp_path / "mac"))
