from pathlib import Path

import pytest

from haylen.cli.application import Application
from haylen.cli.command_session import CommandSession
from haylen.cli.commands.test import TestCommand
from haylen.core.errors.invalid_option import InvalidOption
from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.core.host import Host
from haylen.core.repository import Repository
from haylen.engine.android_app_check import AndroidAppCheck
from haylen.engine.android_library_tests import AndroidLibraryTests
from haylen.engine.engine_tests import EngineTests
from haylen.engine.engine_tree import EngineTree
from haylen.engine.mac_app_check import MacAppCheck
from haylen.engine.suite_shard import SuiteShard
from haylen.engine.web_app_check import WebAppCheck
from haylen.engine.web_checks import WebChecks
from haylen.engine.web_gpu_tests import WebGpuTests
from haylen.toolchains.build_tools import BuildTools
from tests.support.recorded_commands import RecordedCommands


class TestCommandTest:
    """The command "haylen test" sends each request to the tests it names, and refuses what a platform cannot run before anything builds."""

    @staticmethod
    def run(tmp_path, *words: str) -> RecordedCommands:
        parser, commands = Application.parser()
        arguments = parser.parse_args(["test", *words])
        recorded = RecordedCommands(tmp_path)
        commands[arguments.command_path].run(arguments, CommandSession(recorded.context, False))
        return recorded

    def test_the_simulators_mac_catalyst_android_and_devices_take_only_the_gpu_tests(self, tmp_path):
        for target in ("ios-simulator", "tvos-simulator", "visionos-simulator", "catalyst", "android", "ios", "tvos", "visionos"):
            with pytest.raises(UnsupportedTarget, match=f'The target "{target}" runs only the GPU tests'):
                TestCommandTest.run(tmp_path, "--target", target)

    def test_a_desktop_that_is_not_this_machine_runs_nothing(self, tmp_path, monkeypatch):
        other = next(name for name in TestCommand.DESKTOPS if name != Host.system())
        monkeypatch.setattr(EngineTests, "run", lambda *arguments: pytest.fail("The engine tests ran for another desktop."))
        for extra in ([], ["--gpu"]):
            with pytest.raises(UnsupportedTarget, match=f'The tests of "{other}" run on a "{other}" machine'):
                TestCommandTest.run(tmp_path, "--target", other, *extra)

    def test_on_the_web_a_graphics_api_names_the_api_of_the_checks_unless_the_gpu_tests_run(self, tmp_path, monkeypatch):
        called = []
        monkeypatch.setattr(WebChecks, "run", lambda self, api, shard: called.append(("checks", api)) or {})
        monkeypatch.setattr(WebGpuTests, "run", lambda self, configuration, api: called.append(("gpu", api)) or {})
        TestCommandTest.run(tmp_path, "--target", "web", "--graphics-api", "webGl2")
        TestCommandTest.run(tmp_path, "--gpu", "--target", "web", "--graphics-api", "webGl2")
        assert called == [("checks", "webGl2"), ("gpu", "webGl2")]

    def test_the_checks_of_the_web_draw_only_through_the_apis_of_the_web(self, tmp_path):
        with pytest.raises(UnsupportedTarget, match='draw through "webGpu" or "webGl2", not "metal"'):
            TestCommandTest.run(tmp_path, "--target", "web", "--graphics-api", "metal")

    def test_the_tests_of_the_android_libraries_run_alone_on_android(self, tmp_path, monkeypatch):
        called = []
        monkeypatch.setattr(AndroidLibraryTests, "run", lambda self, configuration, device: called.append((configuration, device)) or {})
        TestCommandTest.run(tmp_path, "--libraries", "--target", "android", "--device", "emulator-5554")
        assert called == [("Debug", "emulator-5554")]
        for extra in (["--target", Host.system()], ["--target", "android", "--gpu"], ["--target", "android", "--ui", "app"]):
            with pytest.raises(UnsupportedTarget, match='The option "--libraries" runs the tests of the Android libraries of the engine alone'):
                TestCommandTest.run(tmp_path, "--libraries", *extra)

    def test_the_ui_tests_and_the_port_take_no_gpu_tests(self, tmp_path):
        with pytest.raises(UnsupportedTarget, match='The option "--ui" runs the UI tests of an app, so it takes neither "--gpu"'):
            TestCommandTest.run(tmp_path, "--ui", "samples/tests", "--gpu", "--target", "ios-simulator")
        with pytest.raises(UnsupportedTarget, match='The option "--port" builds the test port for this machine, so it takes neither "--gpu"'):
            TestCommandTest.run(tmp_path, "--port", "--gpu")
        with pytest.raises(UnsupportedTarget, match='The option "--ui-test" picks classes of the UI tests of an app, so it takes "--ui"'):
            TestCommandTest.run(tmp_path, "--ui-test", "CameraTests")

    def test_an_app_starts_only_on_the_web_android_or_a_mac_and_alone(self, tmp_path, monkeypatch):
        monkeypatch.setattr(WebAppCheck, "run", lambda *arguments: pytest.fail("The start check of the app ran."))
        monkeypatch.setattr(AndroidAppCheck, "run", lambda *arguments: pytest.fail("The start check of the app ran."))
        monkeypatch.setattr(MacAppCheck, "run", lambda *arguments: pytest.fail("The quit check of the app ran."))
        with pytest.raises(UnsupportedTarget, match='takes "--target web", "android", "macos" or "catalyst"'):
            TestCommandTest.run(tmp_path, "--app", "samples/tests", "--target", "ios-simulator")
        for target in ("web", "android", "macos", "catalyst"):
            for extra in (["--gpu"], ["--ui", "samples/tests"], ["--port"], ["--sanitizers", "address"]):
                with pytest.raises(UnsupportedTarget, match='The option "--app" checks that an app starts, so it takes neither'):
                    TestCommandTest.run(tmp_path, "--app", "samples/tests", "--target", target, *extra)
        with pytest.raises(UnsupportedTarget, match='takes no "--graphics-api"'):
            TestCommandTest.run(tmp_path, "--app", "samples/tests", "--target", "android", "--graphics-api", "vulkan")
        with pytest.raises(UnsupportedTarget, match='takes no "--graphics-api"'):
            TestCommandTest.run(tmp_path, "--app", "samples/tests", "--target", "macos", "--graphics-api", "metal")

    def test_an_app_starts_in_the_configuration_on_the_api_or_the_device_that_the_command_names(self, tmp_path, monkeypatch):
        called = []
        monkeypatch.setattr(WebAppCheck, "run", lambda self, app, configuration, api: called.append((app, configuration, api)) or {})
        monkeypatch.setattr(AndroidAppCheck, "run", lambda self, app, configuration, device: called.append((app, configuration, device)) or {})
        monkeypatch.setattr(MacAppCheck, "run", lambda self, app, target, configuration: called.append((app, configuration, target)) or {})
        TestCommandTest.run(tmp_path, "--app", "samples/tests", "--target", "web", "--config", "Release", "--graphics-api", "webGl2")
        TestCommandTest.run(tmp_path, "--app", "samples/tests", "--target", "web")
        TestCommandTest.run(tmp_path, "--app", "samples/tests", "--target", "android", "--config", "Release", "--device", "emulator-5554")
        TestCommandTest.run(tmp_path, "--app", "samples/tests", "--target", "catalyst", "--config", "Release")
        expected = [("samples/tests", "Release", "webGl2"), ("samples/tests", "Debug", None), ("samples/tests", "Release", "emulator-5554"), ("samples/tests", "Release", "catalyst")]
        assert called == expected

    def test_the_engine_tests_build_both_programs_and_run_them_in_one_run_of_ctest(self, tmp_path, monkeypatch):
        built = []
        monkeypatch.setattr(Repository, "ENGINE_BUILDS", tmp_path)
        monkeypatch.setattr(EngineTree, "ensure", lambda self, command, env=None: None)
        monkeypatch.setattr(EngineTree, "command", lambda self, configuration, *options: ["cmake", *options])
        monkeypatch.setattr(EngineTree, "build", lambda self, targets, configuration, *arguments: built.append(list(targets)))
        monkeypatch.setattr(type(BuildTools.CTEST), "require", lambda self: Path(self.name))
        recorded = RecordedCommands(tmp_path)
        EngineTests(recorded.context).run("Debug", None, list(EngineTests.TARGETS), None)
        assert built == [["haylen-tests", "haylen-lua-tests"]]
        assert [command[0] for command in recorded.commands] == ["ctest"]

    def test_a_shard_runs_its_part_of_the_engine_tests_through_the_numbers_of_ctest(self, tmp_path, monkeypatch):
        called = []
        monkeypatch.setattr(EngineTests, "run", lambda self, configuration, sanitizers, targets, shard: called.append((sanitizers, targets, shard)) or {})
        TestCommandTest.run(tmp_path, "--sanitizers", "address", "--shard", "2/4")
        assert called == [("address", ["haylen-tests", "haylen-lua-tests"], SuiteShard(2, 4))]
        assert SuiteShard(2, 4).ctest_arguments() == ["--tests-information", "2,,4"]

    def test_a_shard_names_a_part_that_exists_of_the_engine_tests_alone(self, tmp_path):
        for text, problem in (("0/4", "names no part"), ("5/4", "names no part"), ("2", "is not a part"), ("two/four", "is not a part")):
            with pytest.raises(InvalidOption, match=f'The shard "{text}" {problem} of the tests.'):
                SuiteShard.parse(text)
        with pytest.raises(UnsupportedTarget, match='"--shard" splits the engine tests'):
            TestCommandTest.run(tmp_path, "--gpu", "--shard", "1/2")
        with pytest.raises(UnsupportedTarget, match='"--shard" splits the engine tests'):
            TestCommandTest.run(tmp_path, "--target", "android", "--shard", "1/2")
        with pytest.raises(UnsupportedTarget, match='"--shard" splits the engine tests'):
            TestCommandTest.run(tmp_path, "--graphics-api", "vulkan", "--shard", "1/2")

    def test_the_shards_of_the_checks_of_the_web_run_each_check_once(self, tmp_path, monkeypatch):
        ran = []
        for name in ("error_screen", "wake_lock", "fullscreen", "orientation", "plugins", "editor", "memory", "screens", "canvas", "text_input", "gamepads", "loading"):
            monkeypatch.setattr(WebChecks, name, lambda self, api, name=name: ran.append(name) or {})
        for index in (1, 2):
            TestCommandTest.run(tmp_path, "--target", "web", "--shard", f"{index}/2")
        assert ran == ["error_screen", "fullscreen", "plugins", "memory", "canvas", "gamepads", "wake_lock", "orientation", "editor", "screens", "text_input", "loading"]
