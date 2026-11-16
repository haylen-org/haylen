import pytest

from haylen.cli.application import Application
from haylen.cli.command_session import CommandSession
from haylen.cli.commands.test import TestCommand
from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.core.host import Host
from haylen.engine.engine_tests import EngineTests
from haylen.engine.web_app_check import WebAppCheck
from haylen.engine.web_checks import WebChecks
from haylen.engine.web_gpu_tests import WebGpuTests
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
        for platform in ("ios-simulator", "tvos-simulator", "visionos-simulator", "catalyst", "android", "ios", "tvos", "visionos"):
            with pytest.raises(UnsupportedTarget, match="runs only the GPU tests"):
                TestCommandTest.run(tmp_path, "--platform", platform)

    def test_a_desktop_that_is_not_this_machine_runs_nothing(self, tmp_path, monkeypatch):
        other = next(name for name in TestCommand.DESKTOPS if name != Host.system())
        monkeypatch.setattr(EngineTests, "run", lambda *arguments: pytest.fail("The engine tests ran for another desktop."))
        for extra in ([], ["--gpu"]):
            with pytest.raises(UnsupportedTarget, match=f'The tests of "{other}" run on a "{other}" machine'):
                TestCommandTest.run(tmp_path, "--platform", other, *extra)

    def test_on_the_web_a_graphics_api_names_the_api_of_the_checks_unless_the_gpu_tests_run(self, tmp_path, monkeypatch):
        called = []
        monkeypatch.setattr(WebChecks, "run", lambda self, api: called.append(("checks", api)) or {})
        monkeypatch.setattr(WebGpuTests, "run", lambda self, configuration, api: called.append(("gpu", api)) or {})
        TestCommandTest.run(tmp_path, "--platform", "web", "--graphics-api", "webGl2")
        TestCommandTest.run(tmp_path, "--gpu", "--platform", "web", "--graphics-api", "webGl2")
        assert called == [("checks", "webGl2"), ("gpu", "webGl2")]

    def test_the_checks_of_the_web_draw_only_through_the_apis_of_the_web(self, tmp_path):
        with pytest.raises(UnsupportedTarget, match='draw through "webGpu" or "webGl2", not "metal"'):
            TestCommandTest.run(tmp_path, "--platform", "web", "--graphics-api", "metal")

    def test_the_ui_tests_and_the_port_take_no_gpu_tests(self, tmp_path):
        with pytest.raises(UnsupportedTarget, match="neither"):
            TestCommandTest.run(tmp_path, "--ui", "samples/tests", "--gpu", "--platform", "ios-simulator")
        with pytest.raises(UnsupportedTarget, match="neither"):
            TestCommandTest.run(tmp_path, "--port", "--gpu")

    def test_an_app_starts_only_on_the_web_and_alone(self, tmp_path, monkeypatch):
        monkeypatch.setattr(WebAppCheck, "run", lambda *arguments: pytest.fail("The start check of the app ran."))
        with pytest.raises(UnsupportedTarget, match='takes "--platform web"'):
            TestCommandTest.run(tmp_path, "--app", "samples/tests")
        for extra in (["--gpu"], ["--ui", "samples/tests"], ["--port"], ["--sanitizers", "address"]):
            with pytest.raises(UnsupportedTarget, match="neither"):
                TestCommandTest.run(tmp_path, "--app", "samples/tests", "--platform", "web", *extra)

    def test_an_app_on_the_web_starts_in_the_configuration_and_on_the_api_that_the_command_names(self, tmp_path, monkeypatch):
        called = []
        monkeypatch.setattr(WebAppCheck, "run", lambda self, app, configuration, api: called.append((app, configuration, api)) or {})
        TestCommandTest.run(tmp_path, "--app", "samples/tests", "--platform", "web", "--config", "Release", "--graphics-api", "webGl2")
        TestCommandTest.run(tmp_path, "--app", "samples/tests", "--platform", "web")
        assert called == [("samples/tests", "Release", "webGl2"), ("samples/tests", "Debug", None)]
