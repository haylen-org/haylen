import plistlib
import subprocess
from collections.abc import Mapping
from pathlib import Path

from haylen.cli.report_printer import ReportPrinter
from haylen.core.app_manifest import AppManifest
from haylen.core.app_pipeline import AppPipeline
from haylen.core.configuration import Configuration
from haylen.core.errors.build_failed import BuildFailed
from haylen.core.errors.invalid_manifest import InvalidManifest
from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.core.folders import Folders
from haylen.core.host import Host
from haylen.core.product_request import ProductRequest
from haylen.core.repository import Repository
from haylen.core.tooling_context import ToolingContext
from haylen.engine.ui_test_run import UiTestRun
from haylen.engine.unified_log import UnifiedLog
from haylen.platforms.product_folders import ProductFolders
from haylen.toolchains.apple_simulators import AppleSimulators
from haylen.toolchains.xcodegen import XcodeGen


class UiTests:
    """Runs the UI tests of an app on an iOS, tvOS or visionOS simulator: the app builds through the pipeline for the simulator target, and the XCUITest runner of `engine/tests/ui/apple/project.yml` builds with the Swift tests of the folder `ui-tests` of the app and drives the installed app, taps, drags and the system UI included. The run fails on a failed test or on an error that the engine logged meanwhile, uninstalls the app and the runner, and shuts down a simulator that it booted."""

    PROJECT = Repository.ENGINE / "tests" / "ui" / "apple"
    FOLDER = "ui-tests"
    RUNNER = "com.haylen.uitests.xctrunner"
    # Each platform of the UI tests with the scheme of the runner and the family of its simulators.
    PLATFORMS = {"ios-simulator": ("iOS", "iOS"), "tvos-simulator": ("tvOS", "tvOS"), "visionos-simulator": ("visionOS", "xrOS")}
    # The longest that one UI test runs before xcodebuild stops it, so a gesture that the system never finishes fails the test instead of the run waiting forever.
    TEST_SECONDS = 600

    def __init__(self, context: ToolingContext, pipeline: AppPipeline) -> None:
        self.context = context
        self.pipeline = pipeline

    @staticmethod
    def command(action: str, project: Path, platform: str, simulator: str, derived: Path) -> list[object]:
        """The command that builds the runner of a platform with "build-for-testing", or runs its tests on the simulator of that identifier with "test-without-building", whose output holds only the tests."""
        scheme = UiTests.PLATFORMS[platform][0]
        command: list[object] = ["xcodebuild", action, "-project", project, "-scheme", scheme, "-destination", f"id={simulator}", "-derivedDataPath", derived]
        if action == "test-without-building":
            command += ["-test-timeouts-enabled", "YES", "-maximum-test-execution-time-allowance", str(UiTests.TEST_SECONDS), "-collect-test-diagnostics", "never"]
        return command

    @staticmethod
    def environment(environment: Mapping[str, str], identifier: str, output: Path) -> dict[str, str]:
        """The environment of the tests, which xcodebuild hands to the runner through the prefix `TEST_RUNNER_`: the identifier of the app they drive and the folder of their screenshots."""
        return {**environment, "TEST_RUNNER_HAYLEN_UI_APP": identifier, "TEST_RUNNER_HAYLEN_UI_OUTPUT": str(output)}

    @staticmethod
    def tests(app: str) -> tuple[Path, Path]:
        """The folder of the app and the folder of its Swift UI tests, which must hold at least one test."""
        folder = Path(app).expanduser().resolve()
        if not AppManifest.is_app(folder):
            raise InvalidManifest(f'The folder `{Repository.shown(folder)}` holds no "{AppManifest.FILE}". Name the folder of an app with UI tests.')
        tests = folder / UiTests.FOLDER
        if not any(tests.glob("*.swift")):
            raise InvalidManifest(
                f'The app `{Repository.shown(folder)}` has no UI tests. Write them in Swift in `{Repository.shown(tests)}`, as the guide of testing describes for "haylen test --ui".'
            )
        return folder, tests

    def run(self, app: str, platform: str, configuration: str, device: str | None) -> dict[str, object]:
        if platform not in UiTests.PLATFORMS:
            names = ", ".join(f'"{name}"' for name in UiTests.PLATFORMS)
            raise UnsupportedTarget(f'The UI tests run on {names}. Pick one with "--platform".')
        if Host.system() != "macos":
            raise UnsupportedTarget(f'The UI tests run on the simulators of Xcode, which need macOS, and this machine runs "{Host.system()}".')
        folder, tests = UiTests.tests(app)

        request = ProductRequest(folder, platform, Configuration(configuration), Configuration.RELEASE, None)
        product = self.pipeline.build(self.pipeline.prepare(self.pipeline.resolve(request)))
        ReportPrinter.verification(self.pipeline.verify(product), self.context.terminal, f"`{Repository.shown(product.path)}`")
        info = plistlib.loads((product.path / "Info.plist").read_bytes())

        runner = Repository.ENGINE_BUILDS / f"{platform}-ui-tests"
        output = ProductFolders.app_folder(folder) / "ui-tests" / platform
        Folders.remove(output)
        output.mkdir(parents=True)
        runner.mkdir(parents=True, exist_ok=True)
        self.context.terminal.step(f"Generating the UI test runner in `{Repository.shown(runner)}`")
        generate = [XcodeGen.path(self.context), "generate", "--quiet", "--spec", UiTests.PROJECT / "project.yml", "--project", runner]
        self.context.process.run(generate, env={**self.context.environment, "HAYLEN_UI_TESTS": str(tests)})

        family = UiTests.PLATFORMS[platform][1]
        with AppleSimulators(self.context).booted(family, device) as simulator:
            udid = simulator["udid"]
            project = (runner / "HaylenUITests.xcodeproj", platform, udid, runner / "derived")
            self.context.terminal.step("Building the UI test runner")
            self.context.process.run(UiTests.command("build-for-testing", *project))
            self.context.process.run(["xcrun", "simctl", "install", udid, product.path])
            self.context.terminal.step(f'Running the UI tests of `{Repository.shown(folder)}` on the simulator "{simulator["name"]}"')
            try:
                with UnifiedLog(self.context.terminal, info["CFBundleExecutable"], ["xcrun", "simctl", "spawn", udid]) as errors:
                    UiTestRun(self.context.terminal).run(UiTests.command("test-without-building", *project), UiTests.environment(self.context.environment, info["CFBundleIdentifier"], output))
            finally:
                for identifier in (info["CFBundleIdentifier"], UiTests.RUNNER):
                    subprocess.run(["xcrun", "simctl", "uninstall", udid, identifier], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)
                if any(output.iterdir()):
                    self.context.terminal.info(f"The screenshots of the UI tests are in `{Repository.shown(output)}`.")
        if errors:
            raise BuildFailed(f"The UI tests passed, but the engine logged {len(errors)} errors meanwhile, the first being: {errors[0]}")
        self.context.terminal.success(f'The UI tests passed on the simulator "{simulator["name"]}".')
        return {"app": str(folder), "platform": platform, "simulator": udid, "screenshots": str(output)}
