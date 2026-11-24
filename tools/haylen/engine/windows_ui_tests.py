import re
import sys
from collections.abc import Sequence
from pathlib import Path

from haylen.cli.report_printer import ReportPrinter
from haylen.core.app_pipeline import AppPipeline
from haylen.core.configuration import Configuration
from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.core.folders import Folders
from haylen.core.host import Host
from haylen.core.product_request import ProductRequest
from haylen.core.repository import Repository
from haylen.core.tooling_context import ToolingContext
from haylen.engine.ui_test_run import UiTestRun
from haylen.platforms.product_folders import ProductFolders


class WindowsUiTests:
    """Runs the UI tests of an app on this Windows machine: the app builds through the pipeline for the processor of this machine, and pytest runs the Python tests of the folder `ui-tests/windows` of the app, which derive from `HaylenUiTestCase` of `engine/tests/ui/windows`. They drive the app through UI Automation, which reads the accessibility tree that the engine gives the screen readers of Windows, and through the mouse and the keys of `SendInput`, which reach the window in front of the desktop of the person who is signed in, so the machine runs nothing else meanwhile. The run fails on a failed test and keeps the screenshots of the tests."""

    SUPPORT = Repository.ENGINE / "tests" / "ui" / "windows"
    # The line of pytest that starts the tests, which the bound of the start of the run waits for.
    STARTED = re.compile(r"^collected \d+ items?")

    def __init__(self, context: ToolingContext, pipeline: AppPipeline) -> None:
        self.context = context
        self.pipeline = pipeline

    @staticmethod
    def command(tests: Path, classes: Sequence[str] = ()) -> list[str]:
        """The command of pytest that runs the test classes of the folder with the settings of the runner, the classes that `classes` names alone or every class, with each test on a line of its own."""
        command = [sys.executable, "-m", "pytest", str(tests), "-c", str(WindowsUiTests.SUPPORT / "pytest.ini"), "--rootdir", str(tests), "-v"]
        if classes:
            command += ["-k", " or ".join(classes)]
        return command

    @staticmethod
    def environment(base: dict[str, str], executable: Path, output: Path) -> dict[str, str]:
        """The environment of the tests, which names the executable of the app, the folder of the screenshots and the folder of `HaylenUiTestCase`, which the tests import."""
        return {**base, "HAYLEN_UI_APP": str(executable), "HAYLEN_UI_OUTPUT": str(output), "PYTHONPATH": str(WindowsUiTests.SUPPORT)}

    def run(self, folder: Path, tests: Path, configuration: str, device: str | None, classes: Sequence[str]) -> dict[str, object]:
        if Host.system() != "windows":
            raise UnsupportedTarget(f'The UI tests of Windows drive the desktop of a Windows machine, and this machine runs "{Host.system()}".')
        if device is not None:
            raise UnsupportedTarget('The UI tests of "windows" run on this machine, so they take no "--device".')

        request = ProductRequest(folder, "windows", Configuration(configuration), Configuration.RELEASE, (Host.architecture(),))
        product = self.pipeline.build(self.pipeline.prepare(self.pipeline.resolve(request, build_sdk=True)))
        ReportPrinter.verification(self.pipeline.verify(product), self.context.terminal, f"`{Repository.shown(product.path)}`")
        output = ProductFolders.app_folder(folder) / "ui-tests" / "windows"
        Folders.remove(output)
        output.mkdir(parents=True)

        self.context.terminal.step(f'Running the UI tests of "{product.prepared.product.app.name}" on this machine')
        executable = product.components[0].path
        UiTestRun(self.context.terminal, WindowsUiTests.STARTED).run(WindowsUiTests.command(tests, classes), WindowsUiTests.environment(dict(self.context.environment), executable, output))
        if any(output.iterdir()):
            self.context.terminal.info(f"The screenshots of the UI tests are in `{Repository.shown(output)}`.")
        self.context.terminal.success("The UI tests passed on this machine.")
        return {"app": str(folder), "target": "windows", "device": "this machine", "screenshots": str(output)}
