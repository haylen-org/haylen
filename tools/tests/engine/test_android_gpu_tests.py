import shlex
import subprocess
import tarfile
from pathlib import Path

from haylen.engine.android_gpu_tests import AndroidGpuTests
from haylen.engine.android_test_device import AndroidTestDevice
from haylen.engine.engine_tree import EngineTree
from haylen.toolchains.android_sdk import AndroidSdk
from haylen.toolchains.shader_compiler import ShaderCompiler
from tests.support.recorded_commands import RecordedCommands


class AndroidGpuTestsTest:
    """The GPU tests of Android build alone for the ABI of the device as the library of a test app, which starts with the variables of the tests and leaves results that the run copies back only when it wrote them."""

    @staticmethod
    def sdk(tmp_path: Path) -> Path:
        sdk = tmp_path / "sdk"
        (sdk / "ndk" / "30.0.16248370").mkdir(parents=True)
        (sdk / "platform-tools").mkdir()
        return sdk

    def test_the_tests_build_alone_for_the_abi_as_the_library_of_the_test_app(self, tmp_path, monkeypatch):
        recorded = RecordedCommands(tmp_path, {"ANDROID_HOME": str(AndroidGpuTestsTest.sdk(tmp_path))})
        monkeypatch.setattr(ShaderCompiler, "found", {"compiler": tmp_path / "haylen-shaderc"})
        monkeypatch.setattr(EngineTree, "uv", lambda self: tmp_path / "uv")
        tests = AndroidGpuTests(recorded.context)
        command = [str(part) for part in tests.configure_command(EngineTree(recorded.context, "android"), "x86_64", "Debug")]
        assert "-DANDROID_ABI=x86_64" in command
        assert "-DHAYLEN_BUILD_TESTS=ON" in command
        assert "-DHAYLEN_BUILD_PLAYER=OFF" in command

    def test_the_app_starts_with_the_variables_of_the_tests_alone_and_escapes_their_commas(self, tmp_path):
        recorded = RecordedCommands(tmp_path, {"ANDROID_HOME": str(AndroidGpuTestsTest.sdk(tmp_path))})
        tests = AndroidGpuTests(recorded.context)
        environment = {"GTEST_FILTER": "GpuBlendTest.*:Gpu*Test.Draws*", "HAYLEN_PROBE": "a,b", "HOME": "/home/me"}
        command = [str(part) for part in tests.launch_command("SERIAL", "vulkan", environment)]
        assert command[1:10] == ["-s", "SERIAL", "shell", "am", "start", "-S", "-W", "-n", f"{AndroidGpuTests.APP}/android.app.NativeActivity"]
        assert command[10:12] == ["--esa", "environment"]
        assert shlex.split(command[12]) == ["GTEST_FILTER=GpuBlendTest.*:Gpu*Test.Draws*,HAYLEN_PROBE=a\\,b,HAYLEN_TEST_GRAPHICS_API=vulkan"]
        assert "--esa" not in tests.launch_command("SERIAL", None, {"HOME": "/home/me"})

    def test_a_run_that_wrote_no_results_copies_nothing(self, tmp_path, monkeypatch):
        # The command `exec-out` of adb ends with status 0 even when the command on the device fails, and hands its error text as the output.
        def device(command, **_):
            missing = "test" in command
            return subprocess.CompletedProcess(command, 1 if missing else 0, b"" if missing else b"tar: gpu-test-output: No such file or directory", b"")

        opened = []
        monkeypatch.setattr(subprocess, "run", device)
        monkeypatch.setattr(tarfile, "open", lambda *arguments, **options: opened.append(arguments))
        recorded = RecordedCommands(tmp_path, {"ANDROID_HOME": str(AndroidGpuTestsTest.sdk(tmp_path))})
        AndroidGpuTests(recorded.context).copy_results("SERIAL", tmp_path / "gpu-test-output")
        assert opened == []

    def test_the_app_compiles_against_the_newest_platform_of_the_sdk(self, tmp_path):
        sdk = AndroidGpuTestsTest.sdk(tmp_path)
        for name in ("android-34", "android-37.0", "android-37.2", "android-Baklava"):
            (sdk / "platforms" / name).mkdir(parents=True)
            (sdk / "platforms" / name / "android.jar").write_bytes(b"")
        jar, level = AndroidSdk({"ANDROID_HOME": str(sdk)}).platform_jar()
        assert (jar.parent.name, level) == ("android-37.2", 37)

    def test_a_virtual_device_builds_for_the_abi_of_its_configuration(self, tmp_path):
        avd = tmp_path / "avd"
        (avd / "Phone.avd").mkdir(parents=True)
        (avd / "Phone.avd" / "config.ini").write_text("hw.gpu.enabled=yes\nabi.type=arm64-v8a\n", encoding="utf-8")
        recorded = RecordedCommands(tmp_path, {"ANDROID_HOME": str(AndroidGpuTestsTest.sdk(tmp_path)), "ANDROID_AVD_HOME": str(avd)})
        device = AndroidTestDevice(recorded.context, AndroidSdk(recorded.context.environment), None)
        assert device.virtual_device_abi("Phone") == "arm64-v8a"
