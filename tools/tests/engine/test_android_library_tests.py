from pathlib import Path

from haylen.engine.android_library_tests import AndroidLibraryTests
from haylen.engine.android_test_device import AndroidTestDevice
from haylen.engine.engine_tree import EngineTree
from haylen.native.cmake_tree import CMakeTree
from haylen.platforms.android.android_platform import AndroidPlatform
from haylen.toolchains.gradle import Gradle
from haylen.toolchains.shader_compiler import ShaderCompiler
from tests.support.recorded_commands import RecordedCommands


class AndroidLibraryTestsTest:
    """The tests of the Android libraries build the player library alone for the ABI of the device and run the tests of the Java of this machine and the instrumented tests through Gradle on that one device, with the player and the sources of Varn that the libraries compile."""

    @staticmethod
    def recorded(tmp_path: Path) -> RecordedCommands:
        sdk = tmp_path / "sdk"
        (sdk / "ndk" / "30.0.16248370").mkdir(parents=True)
        (sdk / "platform-tools").mkdir()
        return RecordedCommands(tmp_path, {"ANDROID_HOME": str(sdk)})

    def test_the_tree_builds_the_player_alone_into_the_folder_of_its_abi(self, tmp_path, monkeypatch):
        monkeypatch.setattr(ShaderCompiler, "found", {"compiler": tmp_path / "haylen-shaderc"})
        monkeypatch.setattr(EngineTree, "uv", lambda self: tmp_path / "uv")
        recorded = AndroidLibraryTestsTest.recorded(tmp_path)
        tree = EngineTree(recorded.context, "android-library-tests")
        command = [str(part) for part in AndroidLibraryTests(recorded.context).configure_command(tree, "x86_64", "Debug")]
        assert "-DANDROID_ABI=x86_64" in command
        assert "-DHAYLEN_BUILD_PLAYER=ON" in command
        assert "-DHAYLEN_BUILD_TESTS=OFF" in command
        assert f"-DCMAKE_LIBRARY_OUTPUT_DIRECTORY={(tree.folder / 'players' / 'x86_64').as_posix()}" in command

    def test_gradle_runs_the_tests_of_the_libraries_on_the_device_with_the_player_and_the_sources_of_varn(self, tmp_path, monkeypatch):
        monkeypatch.setattr(AndroidTestDevice, "abi", lambda device: "x86_64")
        monkeypatch.setattr(AndroidTestDevice, "__enter__", lambda device: "emulator-5554")
        monkeypatch.setattr(AndroidTestDevice, "__exit__", lambda device, *_: None)
        monkeypatch.setattr(EngineTree, "command", lambda tree, configuration, *options: ["cmake"])
        monkeypatch.setattr(EngineTree, "ensure", lambda tree, command, env=None: None)
        monkeypatch.setattr(EngineTree, "build", lambda tree, targets, configuration, *arguments: None)
        monkeypatch.setattr(CMakeTree, "cache_value", lambda tree, name: "/sources/varn" if name == "varn_SOURCE_DIR" else None)
        monkeypatch.setattr(Gradle, "path", lambda context: Path("/tools/gradle"))
        recorded = AndroidLibraryTestsTest.recorded(tmp_path)

        result = AndroidLibraryTests(recorded.context).run("Debug", None)
        tree = Path(str(result["tree"]))
        command = recorded.commands[-1]
        assert command[:5] == ["/tools/gradle", "-p", str(AndroidPlatform.LIBRARY_PROJECT), ":haylen:testDebugUnitTest", ":haylen:connectedDebugAndroidTest"]
        assert f"-PhaylenNativeLibraries={tree / 'players'}" in command
        assert "-PhaylenVarnSourceDir=/sources/varn" in command
        assert recorded.options[-1]["env"]["ANDROID_SERIAL"] == "emulator-5554"
        assert result["abi"] == "x86_64"
