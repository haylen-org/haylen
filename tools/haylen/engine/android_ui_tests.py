import io
import re
import subprocess
import tarfile
from collections.abc import Sequence
from pathlib import Path

from haylen.cli.report_printer import ReportPrinter
from haylen.core.app_pipeline import AppPipeline
from haylen.core.configuration import Configuration
from haylen.core.errors.build_failed import BuildFailed
from haylen.core.folders import Folders
from haylen.core.product_request import ProductRequest
from haylen.core.repository import Repository
from haylen.core.tooling_context import ToolingContext
from haylen.engine.android_test_device import AndroidTestDevice
from haylen.engine.device_farm_ui_tests import DeviceFarmUiTests
from haylen.engine.ui_test_run import UiTestRun
from haylen.platforms.product_folders import ProductFolders
from haylen.toolchains.android_debug_bridge import AndroidDebugBridge
from haylen.toolchains.android_sdk import AndroidSdk
from haylen.toolchains.gradle import Gradle


class AndroidUiTests:
    """Runs the UI tests of an app on an Android device or emulator: the app builds through the pipeline for the ABI of the device, and the runner of `engine/tests/ui/android` builds with the Kotlin tests of the folder `ui-tests/android` of the app into a test APK that instruments itself. Its instrumentation drives the installed app through UI Automator, the dialogs, the notifications, the home screen, the recent apps and the back button of the system included. The run fails on a failed test, copies back the screenshots of the tests, uninstalls the app and the runner, and shuts down an emulator that it started."""

    PROJECT = Repository.ENGINE / "tests" / "ui" / "android"
    # The test APK of the runner instruments itself, so the device holds only the app and that APK.
    RUNNER = "com.haylen.uitests.test"
    # The package of the Kotlin tests of every app, whose classes "--ui-test" names.
    PACKAGE = "com.haylen.uitests"
    INSTRUMENTATION = f"{RUNNER}/androidx.test.runner.AndroidJUnitRunner"
    # The folder of the screenshots in the files of the runner on the device.
    DEVICE_OUTPUT = "ui-tests"
    # The line of the output of "am instrument -r" that starts the status of a test, which the bound of the start of the run waits for.
    STARTED = re.compile(r"INSTRUMENTATION_STATUS: (test|class)=")
    FARM_ABI = "arm64-v8a"
    # The secure setting that tells Android that the person saw the hint of full screen apps, which covers an app the first time it hides the bars of the system. The run sets it while the tests run and gives it back afterwards.
    IMMERSIVE_CONFIRMATIONS = "immersive_mode_confirmations"
    # The result that "am instrument" reports when every test passed.
    PASSED = "INSTRUMENTATION_CODE: -1"
    FAILED = "INSTRUMENTATION_STATUS_CODE: -2"

    def __init__(self, context: ToolingContext, pipeline: AppPipeline) -> None:
        self.context = context
        self.pipeline = pipeline
        self.sdk = AndroidSdk(context.environment)
        self.bridge = AndroidDebugBridge(context, self.sdk)

    @staticmethod
    def instrument(classes: Sequence[str] = ()) -> list[str]:
        """The arguments of `adb shell` that run the tests of the runner, the classes that it names alone or every class, wait for them and report each test in its raw form."""
        command = ["am", "instrument", "-w", "-r"]
        if classes:
            command += ["-e", "class", ",".join(f"{AndroidUiTests.PACKAGE}.{name}" for name in classes)]
        return [*command, AndroidUiTests.INSTRUMENTATION]

    def gradle(self, tests: Path, identifier: str, builds: Path) -> list[object]:
        """The command that builds the test APK of the runner with the Kotlin tests of the app, which drives the app of the identifier, into the build folder of the engine."""
        tasks = [":runner:assembleDebug"]
        command: list[object] = [Gradle.path(self.context), "-p", AndroidUiTests.PROJECT, *tasks, f"--max-workers={self.context.jobs}", "--project-cache-dir", builds / "gradle-cache"]
        return [*command, f"-PhaylenUiTests={tests}", f"-PhaylenUiApp={identifier}", f"-PhaylenBuildDirectory={builds}"]

    def build(self, folder: Path, tests: Path, configuration: str, abi: str) -> tuple[Path, Path, str]:
        """Builds the APK of the app for the ABI and the test APK of the runner, and returns both with the identifier of the app."""
        request = ProductRequest(folder, "android", Configuration(configuration), Configuration.RELEASE, (abi,))
        product = self.pipeline.build(self.pipeline.prepare(self.pipeline.resolve(request, build_sdk=True)))
        ReportPrinter.verification(self.pipeline.verify(product), self.context.terminal, f"`{Repository.shown(product.path)}`")
        identifier = product.prepared.product.app.identifier

        builds = Repository.ENGINE_BUILDS / "android-ui-tests"
        self.context.terminal.step(f"Building the UI test runner in `{Repository.shown(builds)}`")
        self.context.process.run(self.gradle(tests, identifier, builds), env=self.context.environment)
        return product.path, builds / "runner" / "outputs" / "apk" / "debug" / "runner-debug.apk", identifier

    def run_on_farm(self, folder: Path, tests: Path, configuration: str, devices: Sequence[str]) -> dict[str, object]:
        """Runs the tests on the phones of Device Farm, which run the 64-bit ARM ABI."""
        app, runner, _ = self.build(folder, tests, configuration, AndroidUiTests.FARM_ABI)
        return DeviceFarmUiTests(self.context).run("android", app, runner, devices, ProductFolders.app_folder(folder) / "ui-tests" / "device-farm-android")

    def run(self, folder: Path, tests: Path, configuration: str, requested: str | None, classes: Sequence[str] = ()) -> dict[str, object]:
        device = AndroidTestDevice(self.context, self.sdk, requested)
        app, runner, identifier = self.build(folder, tests, configuration, device.abi())
        output = ProductFolders.app_folder(folder) / "ui-tests" / "android"
        Folders.remove(output)
        output.mkdir(parents=True)
        with device as serial:
            name = self.bridge.name(serial)
            self.context.terminal.step(f'Installing the app and the UI test runner on "{name}"')
            for apk in (app, runner):
                self.context.process.run(self.bridge.command(serial, "install", "-r", apk))
            self.context.terminal.step(f'Running the UI tests on "{name}"')
            confirmations = self.setting(serial, AndroidUiTests.IMMERSIVE_CONFIRMATIONS)
            self.context.process.run(self.bridge.command(serial, "shell", "settings", "put", "secure", AndroidUiTests.IMMERSIVE_CONFIRMATIONS, "confirmed"))
            try:
                self.instrument_on(serial, classes)
            finally:
                self.restore_setting(serial, AndroidUiTests.IMMERSIVE_CONFIRMATIONS, confirmations)
                self.copy_screenshots(serial, output)
                for package in (identifier, AndroidUiTests.RUNNER):
                    subprocess.run([str(part) for part in self.bridge.command(serial, "uninstall", package)], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)
        if any(output.iterdir()):
            self.context.terminal.info(f"The screenshots of the UI tests are in `{Repository.shown(output)}`.")
        self.context.terminal.success(f'The UI tests passed on "{name}".')
        return {"app": str(folder), "target": "android", "device": name, "screenshots": str(output)}

    def setting(self, serial: str, name: str) -> str | None:
        """The value of a secure setting of the device, or None when the device has none."""
        value = self.context.process.capture(self.bridge.command(serial, "shell", "settings", "get", "secure", name)).strip()
        return None if value == "null" else value

    def restore_setting(self, serial: str, name: str, value: str | None) -> None:
        """Gives a secure setting back the value it had before the run, which deletes one that the device did not have. The command "adb shell" joins its arguments into one command line for the shell of the device, which drops an empty word, so an empty value travels quoted."""
        action = ["delete", "secure", name] if value is None else ["put", "secure", name, value or "''"]
        self.context.process.run(self.bridge.command(serial, "shell", "settings", *action))

    def instrument_on(self, serial: str, classes: Sequence[str]) -> None:
        """Runs the instrumentation, prints its report as it comes and fails unless it reports that every test passed."""
        command = self.bridge.command(serial, "shell", *AndroidUiTests.instrument(classes))
        lines = UiTestRun(self.context.terminal, AndroidUiTests.STARTED).run(command, self.context.environment)
        if not AndroidUiTests.passed(lines):
            raise BuildFailed("The UI tests failed, and the report above shows which ones and why.")

    @staticmethod
    def passed(lines: list[str]) -> bool:
        """Whether the report of "am instrument -r" says that the run ended and that no test failed, which a status code of -2 marks."""
        stripped = [line.strip() for line in lines]
        return AndroidUiTests.PASSED in stripped and AndroidUiTests.FAILED not in stripped

    def copy_screenshots(self, serial: str, output: Path) -> None:
        """Copies the screenshots that the tests saved in the files of the runner, which a debuggable app lets adb read. A run whose tests saved none, such as one that failed before its first screenshot, leaves no folder to copy."""
        folder = f"files/{AndroidUiTests.DEVICE_OUTPUT}"
        exists = subprocess.run([str(part) for part in self.bridge.command(serial, "shell", "run-as", AndroidUiTests.RUNNER, "test", "-d", folder)], capture_output=True, check=False)
        if exists.returncode != 0:
            return
        command = [str(part) for part in self.bridge.command(serial, "exec-out", "run-as", AndroidUiTests.RUNNER, "tar", "-c", "-C", "files", AndroidUiTests.DEVICE_OUTPUT)]
        archived = subprocess.run(command, capture_output=True, check=True)
        with tarfile.open(fileobj=io.BytesIO(archived.stdout)) as archive:
            for member in archive.getmembers():
                if member.isfile() and member.name.endswith(".png"):
                    (output / Path(member.name).name).write_bytes(archive.extractfile(member).read())
