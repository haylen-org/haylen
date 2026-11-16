from pathlib import Path

import pytest

from haylen.core.errors.missing_toolchain import MissingToolchain
from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.core.host import Host
from haylen.engine.apple_device_gpu_tests import AppleDeviceGpuTests
from haylen.engine.apple_gpu_tests import AppleGpuTests
from haylen.engine.engine_tree import EngineTree
from haylen.engine.gpu_test_results import GpuTestResults
from haylen.toolchains.apple_sdk import AppleSdk
from haylen.toolchains.shader_compiler import ShaderCompiler
from tests.support.recorded_commands import RecordedCommands


class AppleGpuTestsTest:
    """The GPU tests of the Apple simulators and Mac Catalyst build alone for the SDK of each, and those of devices as an app that Xcode signs for the team. A simulator or a device takes only the variables of the tests, and the results of a device say whether the tests failed or never finished."""

    @pytest.fixture
    def tree(self, tmp_path, monkeypatch) -> EngineTree:
        monkeypatch.setattr(ShaderCompiler, "found", {"compiler": tmp_path / "haylen-shaderc"})
        monkeypatch.setattr(EngineTree, "uv", lambda self: tmp_path / "uv")
        monkeypatch.setattr(Host, "architecture", staticmethod(lambda: "arm64"))
        return EngineTree(RecordedCommands(tmp_path).context, "apple")

    def test_the_tests_build_alone_for_the_sdk_of_each_simulator_and_for_mac_catalyst(self, tree):
        for platform, system, sdk in (("ios-simulator", "iOS", "iphonesimulator"), ("tvos-simulator", "tvOS", "appletvsimulator"), ("visionos-simulator", "visionOS", "xrsimulator")):
            command = [str(part) for part in AppleGpuTests.configure_command(tree, platform, "Debug")]
            assert f"-DCMAKE_SYSTEM_NAME={system}" in command
            assert f"-DCMAKE_OSX_SYSROOT={sdk}" in command
            assert "-DCMAKE_OSX_ARCHITECTURES=arm64" in command
            assert "-DHAYLEN_BUILD_TESTS=ON" in command
            assert "-DHAYLEN_BUILD_PLAYER=OFF" in command
        assert f"-DCMAKE_TOOLCHAIN_FILE={AppleSdk.CATALYST_TOOLCHAIN}" in [str(part) for part in AppleGpuTests.configure_command(tree, "catalyst", "Debug")]

    def test_a_simulator_spawns_the_tests_and_hands_them_the_variables_of_the_tests_alone(self):
        environment = {"GTEST_FILTER": "GpuBlendTest.*", "HAYLEN_TEST_GPU_LOW_POWER": "1", "HOME": "/Users/me"}
        command, env = AppleGpuTests.command(Path("/build/haylen-gpu-tests"), "SIMULATOR-ID", "metal", environment)
        assert [str(part) for part in command] == ["xcrun", "simctl", "spawn", "SIMULATOR-ID", "/build/haylen-gpu-tests"]
        assert env["SIMCTL_CHILD_HAYLEN_TEST_GRAPHICS_API"] == "metal"
        assert env["SIMCTL_CHILD_GTEST_FILTER"] == "GpuBlendTest.*"
        assert env["SIMCTL_CHILD_HAYLEN_TEST_GPU_LOW_POWER"] == "1"
        assert "SIMCTL_CHILD_HOME" not in env

    def test_mac_catalyst_runs_the_tests_on_this_mac(self):
        command, env = AppleGpuTests.command(Path("/build/haylen-gpu-tests"), None, None, {"HOME": "/Users/me"})
        assert command == [Path("/build/haylen-gpu-tests")]
        assert "HAYLEN_TEST_GRAPHICS_API" not in env

    def test_a_device_builds_the_tests_as_an_app_that_xcode_signs_for_the_team(self, tmp_path, tree):
        for platform, system, sdk in (("ios", "iOS", "iphoneos"), ("tvos", "tvOS", "appletvos"), ("visionos", "visionOS", "xros")):
            tests = AppleDeviceGpuTests(RecordedCommands(tmp_path, {"HAYLEN_APPLE_TEAM": "TEAM123456"}).context)
            command = [str(part) for part in tests.configure_command(tree, platform, "Debug")]
            assert command[command.index("-G") + 1] == "Xcode"
            assert f"-DCMAKE_SYSTEM_NAME={system}" in command
            assert f"-DCMAKE_OSX_SYSROOT={sdk}" in command
            assert "-DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM=TEAM123456" in command
        with pytest.raises(MissingToolchain, match="HAYLEN_APPLE_TEAM"):
            AppleDeviceGpuTests(RecordedCommands(tmp_path).context).configure_command(tree, "ios", "Debug")

    def test_a_device_launches_the_app_and_hands_it_the_variables_of_the_tests_alone(self):
        command, env = AppleDeviceGpuTests.command("iPhone", "metal", {"GTEST_FILTER": "GpuBlendTest.*", "HOME": "/Users/me"})
        assert command == ["xcrun", "devicectl", "device", "process", "launch", "--console", "--terminate-existing", "--device", "iPhone", AppleDeviceGpuTests.APP]
        assert env["DEVICECTL_CHILD_HAYLEN_TEST_GRAPHICS_API"] == "metal"
        assert env["DEVICECTL_CHILD_GTEST_FILTER"] == "GpuBlendTest.*"
        assert "DEVICECTL_CHILD_HOME" not in env

    def test_a_device_needs_its_name(self, tmp_path):
        if Host.system() != "macos":
            pytest.skip("The GPU tests of Apple devices build on macOS.")
        with pytest.raises(UnsupportedTarget, match="Name the device"):
            AppleDeviceGpuTests(RecordedCommands(tmp_path).context).run("ios", "Debug", None, None)

    def test_the_results_of_a_device_say_whether_the_tests_failed_or_never_finished(self, tmp_path):
        results = tmp_path / "results.xml"
        assert "quit before it wrote its results" in GpuTestResults.problem(results)
        results.write_text('<testsuites tests="40" failures="2" errors="1"></testsuites>', encoding="utf-8")
        assert GpuTestResults.problem(results) == "3 of the 40 GPU tests failed on the device."
        results.write_text('<testsuites tests="40" failures="0" errors="0"></testsuites>', encoding="utf-8")
        assert GpuTestResults.problem(results) is None
