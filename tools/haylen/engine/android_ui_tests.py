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
    """Runs the UI tests of an app on an Android device or emulator: the app builds through the pipeline for the ABI of the device, and the runner of `engine/tests/ui/android` builds with the Kotlin tests of the folder `ui-tests/android` of the app into a test APK that instruments itself. Its instrumentation drives the installed app through UI Automator, the dialogs, the notifications, the home screen, the recent apps and the back button of the system included. A test that turns the device asks the run in a status of its report, and the run turns the emulator by its accelerometer through the console of the emulator, which an app that follows the sensor obeys, and gives the accelerometer back its reading after the run. The run fails on a failed test, copies back the screenshots of the tests, uninstalls the app and the runner, and shuts down an emulator that it started."""

    PROJECT = Repository.ENGINE / "tests" / "ui" / "android"
    # The test APK of the runner instruments itself, so the device holds only the app and that APK.
    RUNNER = "com.haylen.uitests.test"
    # The package of the Kotlin tests of every app, whose classes "--ui-test" names.
    PACKAGE = "com.haylen.uitests"
    INSTRUMENTATION = f"{RUNNER}/androidx.test.runner.AndroidJUnitRunner"
    # The folder of the screenshots in the files of the runner on the device.
    DEVICE_OUTPUT = "ui-tests"
    # The file of the log of the device after a failed run, next to the screenshots of the run.
    DEVICE_LOG = "logcat.txt"
    # The line of the output of "am instrument -r" that starts the status of a test, which the bound of the start of the run waits for.
    STARTED = re.compile(r"INSTRUMENTATION_STATUS: (test|class)=")
    FARM_ABI = "arm64-v8a"
    # The secure settings that the run sets while the tests run and gives back afterwards: that the person saw the hint of full screen apps, which covers an app the first time it hides the bars of the system, and that the keyboard of the system shows while a hardware keyboard is attached, as the setting "Use on-screen keyboard" of Android says, since an emulator reports the keyboard of the machine it runs on as one and the tests type as a person does on a phone.
    SETTINGS = {"immersive_mode_confirmations": "confirmed", "show_ime_with_hard_keyboard": "1"}
    # The result that "am instrument" reports when every test passed.
    PASSED = "INSTRUMENTATION_CODE: -1"
    FAILED = "INSTRUMENTATION_STATUS_CODE: -2"
    # The status of a test that asks the run to turn the device until the display takes a rotation, a quarter turn from 0 to 3 from the natural orientation of the device, which "turnDevice" of the tests sends.
    TURN = re.compile(r"^INSTRUMENTATION_STATUS: haylenTurn=([0-3])$")
    # The reading of the accelerometer that holds the device in each rotation of the display, the push that holds it up in meters per second squared along the axes of the device, so the side that faces down turns the display.
    GRAVITY = 9.80665
    ACCELERATIONS = {0: (0.0, GRAVITY, 0.0), 1: (GRAVITY, 0.0, 0.0), 2: (0.0, -GRAVITY, 0.0), 3: (-GRAVITY, 0.0, 0.0)}
    # The answer of the console of the emulator to "sensor get acceleration", such as "acceleration = 0:9.77631:0.812349".
    READING = re.compile(r"^acceleration = (\S+)$", re.MULTILINE)

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
            kept = {setting: self.setting(serial, setting) for setting in AndroidUiTests.SETTINGS}
            for setting, value in AndroidUiTests.SETTINGS.items():
                self.context.process.run(self.bridge.command(serial, "shell", "settings", "put", "secure", setting, value))
            acceleration = self.acceleration(serial) if AndroidUiTests.is_emulator(serial) else None
            try:
                self.instrument_on(serial, classes)
            except BuildFailed:
                self.keep_log(serial, output)
                raise
            finally:
                for setting, value in kept.items():
                    self.restore_setting(serial, setting, value)
                if acceleration is not None:
                    self.context.process.run(self.bridge.command(serial, "emu", "sensor", "set", "acceleration", acceleration))
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
        """Runs the instrumentation, prints its report as it comes, answers the tests that turn the device and fails unless it reports that every test passed."""
        command = self.bridge.command(serial, "shell", *AndroidUiTests.instrument(classes))
        lines = UiTestRun(self.context.terminal, AndroidUiTests.STARTED, lambda line: self.answer(serial, line)).run(command, self.context.environment)
        if not AndroidUiTests.passed(lines):
            raise BuildFailed("The UI tests failed, and the report above shows which ones and why.")

    @staticmethod
    def is_emulator(serial: str) -> bool:
        """Whether the device is an emulator, whose console takes the commands of "adb emu"."""
        return serial.startswith("emulator-")

    def acceleration(self, serial: str) -> str:
        """The reading of the accelerometer of the emulator, in the form that "sensor set acceleration" takes."""
        answer = self.context.process.capture(self.bridge.command(serial, "emu", "sensor", "get", "acceleration"))
        reading = AndroidUiTests.READING.search(answer)
        if reading is None:
            raise BuildFailed(f'The emulator "{serial}" does not report the reading of its accelerometer, so the run cannot turn it.')
        return reading[1]

    @staticmethod
    def turned(rotation: int) -> str:
        """The reading of the accelerometer that turns the display to the rotation, in the form that "sensor set acceleration" takes."""
        return ":".join(f"{axis:g}" for axis in AndroidUiTests.ACCELERATIONS[rotation])

    def answer(self, serial: str, line: str) -> None:
        """Turns the emulator when the line asks for it. A test asks only on an emulator, since a physical device turns only in the hands of a person, so the test fails there before it asks."""
        request = AndroidUiTests.TURN.match(line.strip())
        if request is not None:
            self.context.process.run(self.bridge.command(serial, "emu", "sensor", "set", "acceleration", AndroidUiTests.turned(int(request[1]))))

    @staticmethod
    def passed(lines: list[str]) -> bool:
        """Whether the report of "am instrument -r" says that the run ended and that no test failed, which a status code of -2 marks."""
        stripped = [line.strip() for line in lines]
        return AndroidUiTests.PASSED in stripped and AndroidUiTests.FAILED not in stripped

    def keep_log(self, serial: str, output: Path) -> None:
        """Keeps the log of the device after a failed run next to the screenshots, so a run on a machine that nobody watches, such as continuous integration, tells what the app and the system did meanwhile."""
        path = output / AndroidUiTests.DEVICE_LOG
        with path.open("w", encoding="utf-8") as log:
            subprocess.run([str(part) for part in self.bridge.command(serial, "logcat", "-d", "-v", "threadtime")], stdout=log, stderr=subprocess.STDOUT, check=False)
        self.context.terminal.info(f"The log of the device during the UI tests is in `{Repository.shown(path)}`.")

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
