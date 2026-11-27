import datetime
import functools
import json
import plistlib
import re
import subprocess
import zipfile
from collections.abc import Mapping, Sequence
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
from haylen.engine.android_ui_tests import AndroidUiTests
from haylen.engine.device_farm_ui_tests import DeviceFarmUiTests
from haylen.engine.mac_keyboard import MacKeyboard
from haylen.engine.ui_test_results import UiTestResults
from haylen.engine.ui_test_run import UiTestRun
from haylen.engine.unified_log import UnifiedLog
from haylen.engine.windows_ui_tests import WindowsUiTests
from haylen.platforms.apple.apple_devices import AppleDevices
from haylen.platforms.apple.ios_devices import IosDevices
from haylen.platforms.build_product import BuildProduct
from haylen.platforms.product_folders import ProductFolders
from haylen.toolchains.apple_simulators import AppleSimulators
from haylen.toolchains.xcodegen import XcodeGen


class UiTests:
    """Runs the UI tests of an app on an iOS, tvOS or visionOS simulator or device, on this Mac as a macOS or a Mac Catalyst app, on Android through `AndroidUiTests`, or on this Windows machine through `WindowsUiTests`: the app builds through the pipeline for the target, and the XCUITest runner of `engine/tests/ui/apple/project.yml` builds with the Swift tests of the folder `ui-tests` of the app and drives the app, taps, clicks, keys, the remote and the system UI included. The runner of the Mac launches the app from the bundle that the run built, since a Mac installs no app for the tests. The run fails on a failed test, and on a simulator also on an error that the engine logged meanwhile, and a run that fails keeps its results through `UiTestResults`, with the diagnostics of a simulator and a sample of the app at each failure there. It uninstalls the app and the runner, and shuts down a simulator that it booted."""

    PROJECT = Repository.ENGINE / "tests" / "ui" / "apple"
    FOLDER = "ui-tests"
    # The folder of the tests of each runner inside the folder of the UI tests, with their language and their files.
    RUNNERS = {"apple": ("Swift", "*.swift"), "android": ("Kotlin", "*.kt"), "windows": ("Python", "test_*.py")}
    RUNNER = "com.haylen.uitests.xctrunner"
    # Each simulator platform of the UI tests with the scheme of the runner and the family of its simulators.
    PLATFORMS = {"ios-simulator": ("iOS", "iOS"), "tvos-simulator": ("tvOS", "tvOS"), "visionos-simulator": ("visionOS", "xrOS")}
    # Each device platform of the UI tests with the scheme of the runner.
    DEVICES = {"ios": "iOS", "tvos": "tvOS", "visionos": "visionOS"}
    # The targets of the apps of this Mac, which the runner of the Mac drives, since a Mac Catalyst app is an app of the Mac too.
    MACS = ("macos", "catalyst")
    MAC_SCHEME = "macOS"
    MAC_NAME = "this Mac"
    INPUT_SOURCE_FILE = "input-source"
    # The longest that one UI test runs before xcodebuild stops it, so a gesture that the system never finishes fails the test instead of the run waiting forever.
    TEST_SECONDS = 600
    # The folder of the screenshots in the container of the runner on a device, which the run copies back.
    DEVICE_OUTPUT = "Library/Caches/ui-tests"
    # The name that Xcode suggests for a screenshot that a test attached to a result bundle as it exports it: the name of the attachment, its place and a unique identifier, and the extension of a PNG image. XCTest attaches its own screenshots as HEIC images and its descriptions of elements as text.
    ATTACHMENT_NAME = re.compile(r"^(?P<name>.+)_\d+_[0-9A-F-]{36}(?P<extension>\.png)$")
    # The development team in the signature of an app, as "codesign" describes it.
    TEAM = re.compile(r"^TeamIdentifier=(\w+)$", re.MULTILINE)
    # The label of the job of an app inside a simulator, which its identifier follows.
    APP_JOB = "UIKitApplication:"
    # The seconds that the run samples the app for as a test fails.
    SAMPLE_SECONDS = 3
    # The preference of the keyboard of an iOS simulator that the run turns off while the tests run and gives back afterwards: that the keyboard of the system shrinks to its bar while a hardware keyboard is attached, since a simulator reports the keyboard of the Mac it runs on as one and the tests type as a person does on a phone.
    KEYBOARD_DOMAIN = "com.apple.keyboard.preferences"
    KEYBOARD_MINIMIZATION = "AutomaticMinimizationEnabled"

    def __init__(self, context: ToolingContext, pipeline: AppPipeline) -> None:
        self.context = context
        self.pipeline = pipeline

    @staticmethod
    def command(action: str, project: Path, scheme: str, destination: str, derived: Path, classes: Sequence[str] = (), results: Path | None = None) -> list[object]:
        """The command that builds the runner of a scheme with "build-for-testing", or runs its tests on the destination with "test-without-building", a simulator or a device by its identifier or this Mac by its processor, whose output holds only the tests, the classes that it names alone or every class, into the result bundle that `results` names or one of its own. The target of the runner of each platform takes the name of its scheme."""
        command: list[object] = ["xcodebuild", action, "-project", project, "-scheme", scheme, "-destination", destination, "-derivedDataPath", derived]
        if action == "test-without-building":
            command += ["-test-timeouts-enabled", "YES", "-maximum-test-execution-time-allowance", str(UiTests.TEST_SECONDS), "-collect-test-diagnostics", "never"]
            command += [f"-only-testing:{scheme}/{name}" for name in classes]
            if results is not None:
                command += ["-resultBundlePath", results]
        return command

    @staticmethod
    def signing(team: str) -> list[str]:
        """The settings that sign the runner of a device for the development team of the app, whose profiles Xcode makes as it builds."""
        return ["-allowProvisioningUpdates", f"DEVELOPMENT_TEAM={team}", "CODE_SIGN_STYLE=Automatic", "CODE_SIGN_IDENTITY=Apple Development"]

    @staticmethod
    def generation(environment: Mapping[str, str], tests: Path, runner: Path, identifier: str, output: str, bundle: Path | None = None) -> dict[str, str]:
        """The environment of XcodeGen, which writes the tests of the app into the project of the runner, and the identifier of the app, the bundle that the runner of the Mac launches and the folder of the screenshots into the Info.plist of the runner."""
        app = "" if bundle is None else str(bundle)
        input_source = "" if bundle is None else str(UiTests.input_source_file(runner))
        return {
            **environment,
            "HAYLEN_UI_TESTS": str(tests),
            "HAYLEN_UI_RUNNER": str(runner),
            "HAYLEN_UI_APP": identifier,
            "HAYLEN_UI_APP_PATH": app,
            "HAYLEN_UI_OUTPUT": output,
            "HAYLEN_UI_INPUT_SOURCE": input_source,
        }

    @staticmethod
    def input_source_file(runner: Path) -> Path:
        """The file where the run of the Mac writes the input source that it selected for a test, which the runner reads in its sandbox to know that the selection happened."""
        return runner / UiTests.INPUT_SOURCE_FILE

    @staticmethod
    def mac_destination() -> str:
        """The destination of the runner of the Mac: this Mac, with the processor that the app builds for."""
        return f"platform=macOS,arch={Host.architecture()}"

    @staticmethod
    def tests(app: str, runner: str) -> tuple[Path, Path]:
        """The folder of the app and the folder of its UI tests for a runner, `apple`, `android` or `windows`, which must hold at least one test in the language of the runner."""
        folder = Path(app).expanduser().resolve()
        if not AppManifest.is_app(folder):
            raise InvalidManifest(f'The folder `{Repository.shown(folder)}` holds no "{AppManifest.FILE}". Name the folder of an app with UI tests.')
        language, pattern = UiTests.RUNNERS[runner]
        tests = folder / UiTests.FOLDER / runner
        if not any(tests.glob(pattern)):
            raise InvalidManifest(
                f'The app `{Repository.shown(folder)}` has no UI tests for "{runner}". Write them in {language} in `{Repository.shown(tests)}`, as the guide of testing describes for "haylen test --ui".'
            )
        return folder, tests

    @staticmethod
    def team(description: str) -> str:
        """The development team that signed an app, from what "codesign --display --verbose=2" writes about it."""
        match = UiTests.TEAM.search(description)
        if match is None or match[1] == "not":
            raise BuildFailed("The app of the device carries no signature of a development team, so its UI test runner cannot be signed for it.")
        return match[1]

    def run(self, app: str, platform: str, configuration: str, device: str | None, farm: Sequence[str] = (), classes: Sequence[str] = ()) -> dict[str, object]:
        """Runs the UI tests on the simulator, device or emulator of the platform, or with `farm` on the devices of Device Farm that it names, the classes that `classes` names alone or every class."""
        if farm and platform not in DeviceFarmUiTests.TYPES:
            raise UnsupportedTarget('Device Farm runs the UI tests of "android" and "ios". Pick one with "--target".')
        if farm and classes:
            raise UnsupportedTarget('Device Farm runs every class of the UI tests, so "--device-farm" takes no "--ui-test".')
        if platform == "android":
            folder, tests = UiTests.tests(app, "android")
            android = AndroidUiTests(self.context, self.pipeline)
            return android.run_on_farm(folder, tests, configuration, farm) if farm else android.run(folder, tests, configuration, device, classes)
        if platform == "windows":
            folder, tests = UiTests.tests(app, "windows")
            return WindowsUiTests(self.context, self.pipeline).run(folder, tests, configuration, device, classes)
        if platform not in UiTests.PLATFORMS and platform not in UiTests.DEVICES and platform not in UiTests.MACS:
            names = ", ".join(f'"{name}"' for name in (*UiTests.PLATFORMS, *UiTests.DEVICES, *UiTests.MACS, "android", "windows"))
            raise UnsupportedTarget(f'The UI tests run on {names}. Pick one with "--target".')
        if Host.system() != "macos":
            raise UnsupportedTarget(f'The UI tests of Apple platforms run with Xcode, which needs macOS, and this machine runs "{Host.system()}".')
        folder, tests = UiTests.tests(app, "apple")

        # A Mac runs the app that the run built, so the app builds for the processor of this Mac alone.
        architectures = (Host.architecture(),) if platform in UiTests.MACS else None
        request = ProductRequest(folder, platform, Configuration(configuration), Configuration.RELEASE, architectures)
        product = self.pipeline.build(self.pipeline.prepare(self.pipeline.resolve(request, build_sdk=True)))
        ReportPrinter.verification(self.pipeline.verify(product), self.context.terminal, f"`{Repository.shown(product.path)}`")
        output = ProductFolders.app_folder(folder) / "ui-tests" / platform
        Folders.remove(output)
        output.mkdir(parents=True)
        runner = Repository.ENGINE_BUILDS / f"{platform}-ui-tests"
        runner.mkdir(parents=True, exist_ok=True)
        self.context.terminal.step(f"Generating the UI test runner in `{Repository.shown(runner)}`")
        # The Info.plist of an app of the Mac sits in the folder "Contents" of its bundle.
        info = product.path / "Contents" / "Info.plist" if platform in UiTests.MACS else product.path / "Info.plist"
        identifier = plistlib.loads(info.read_bytes())["CFBundleIdentifier"]
        # A runner on a simulator writes its screenshots into the folder of the app on this Mac, a runner on a device into its own container, and the runner of the Mac into its result bundle.
        screenshots = UiTests.DEVICE_OUTPUT if platform in UiTests.DEVICES else str(output)
        bundle = product.path if platform in UiTests.MACS else None
        generate = [XcodeGen.path(self.context), "generate", "--quiet", "--spec", UiTests.PROJECT / "project.yml", "--project", runner]
        self.context.process.run(generate, env=UiTests.generation(self.context.environment, tests, runner, identifier, screenshots, bundle))

        if farm:
            return self.run_on_farm(product, runner, farm, ProductFolders.app_folder(folder) / "ui-tests" / "device-farm-ios")
        runs = {**dict.fromkeys(UiTests.DEVICES, self.run_on_device), **dict.fromkeys(UiTests.MACS, self.run_on_mac)}
        run = runs.get(platform, self.run_on_simulator)
        name = run(product, platform, device, runner, output, classes)
        if any(output.iterdir()):
            self.context.terminal.info(f"The screenshots of the UI tests are in `{Repository.shown(output)}`.")
        self.context.terminal.success(f'The UI tests passed on "{name}".')
        return {"app": str(folder), "target": platform, "device": name, "screenshots": str(output)}

    def run_on_simulator(self, product: BuildProduct, platform: str, device: str | None, runner: Path, output: Path, classes: Sequence[str]) -> str:
        """Runs the tests on a simulator, which keeps the results of a failed run with the diagnostics of the simulator and a sample of the app at each failure."""
        info = plistlib.loads((product.path / "Info.plist").read_bytes())
        scheme, family = UiTests.PLATFORMS[platform]
        simulators = AppleSimulators(self.context)
        simulator = simulators.find(family, device, IosDevices.of(product.prepared.product.app))
        results = UiTestResults(runner, platform, simulator["name"], classes, datetime.datetime.now())
        try:
            with simulators.use(simulator, family, results.folder):
                self.run_tests_on_simulator(product, info, simulator, scheme, runner, results, classes)
        except BuildFailed as error:
            results.keep(error, self.context.terminal)
            raise
        results.discard()
        return simulator["name"]

    def run_tests_on_simulator(self, product: BuildProduct, info: dict, simulator: dict, scheme: str, runner: Path, results: UiTestResults, classes: Sequence[str]) -> None:
        udid = simulator["udid"]
        project = (runner / "HaylenUITests.xcodeproj", scheme, f"id={udid}", runner / "derived")
        self.context.terminal.step("Building the UI test runner")
        self.context.process.run(UiTests.command("build-for-testing", *project))
        self.context.process.run(["xcrun", "simctl", "install", udid, product.path])
        self.context.terminal.step(f'Running the UI tests on the simulator "{simulator["name"]}"')
        phone = scheme == "iOS"
        minimization = self.keyboard_minimization(udid) if phone else None
        if phone:
            self.set_keyboard_minimization(udid, False)

        answer = functools.partial(self.answer_on_simulator, results, udid, info["CFBundleIdentifier"])
        try:
            with UnifiedLog(self.context.terminal, info["CFBundleExecutable"], ["xcrun", "simctl", "spawn", udid]) as errors:
                UiTestRun(self.context.terminal, answer=answer).run(UiTests.command("test-without-building", *project, classes, results.bundle), self.context.environment)
        finally:
            if phone:
                self.set_keyboard_minimization(udid, minimization)
            for identifier in (info["CFBundleIdentifier"], UiTests.RUNNER):
                subprocess.run(["xcrun", "simctl", "uninstall", udid, identifier], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)
        if errors:
            raise BuildFailed(f"The UI tests passed, but the engine logged {len(errors)} errors meanwhile, the first being: {errors[0]}")

    def answer_on_simulator(self, results: UiTestResults, udid: str, identifier: str, line: str) -> None:
        """Keeps the test that a line of the run ends as failed, and samples the app when a line reports a failure as it happens."""
        results.record(line)
        if UiTestResults.FAILURE.search(line):
            self.sample_app(udid, identifier, results.folder)

    @staticmethod
    def answer_on_mac(results: UiTestResults, keyboard: MacKeyboard, line: str) -> None:
        """Keeps the test that a line of the run ends as failed, and answers the requests of the tests for the keyboard of the Mac."""
        results.record(line)
        keyboard.answer(line)

    @staticmethod
    def app_process(listing: str, identifier: str) -> int | None:
        """The process of an app in what "launchctl list" writes inside a simulator, where the job of an app takes a label that starts with its identifier, or None when the app does not run."""
        for line in listing.splitlines():
            fields = line.split("\t")
            if len(fields) == 3 and fields[0].isdigit() and fields[2].startswith(f"{UiTests.APP_JOB}{identifier}["):
                return int(fields[0])
        return None

    def sample_app(self, udid: str, identifier: str, folder: Path) -> None:
        """Samples the threads of the app on a simulator as a test fails, while the app is still in the state that failed it, so a frame thread that stopped shows where it waits. The processes of a simulator run on the Mac, which samples them."""
        _, listing = self.context.process.attempt(["xcrun", "simctl", "spawn", udid, "launchctl", "list"])
        pid = UiTests.app_process(listing.decode("utf-8", errors="replace"), identifier)
        if pid is None:
            self.context.terminal.info("The app was not running when the test failed, so the run took no sample of it.")
            return
        folder.mkdir(parents=True, exist_ok=True)
        path = folder / f"sample-{len(list(folder.glob('sample-*.txt'))) + 1}.txt"
        code, _ = self.context.process.attempt(["sample", str(pid), str(UiTests.SAMPLE_SECONDS), "-file", str(path)])
        if code != 0:
            self.context.terminal.warning(f'The app could not be sampled as the test failed, since "sample" failed with exit code {code}.')
            return
        self.context.terminal.info(f"The sample of the app as the test failed is in `{Repository.shown(path)}`.")

    def keyboard_minimization(self, udid: str) -> bool | None:
        """The value of the preference that shrinks the keyboard of an iOS simulator while a hardware keyboard is attached, or None when the simulator has none."""
        command = ["xcrun", "simctl", "spawn", udid, "defaults", "read", UiTests.KEYBOARD_DOMAIN, UiTests.KEYBOARD_MINIMIZATION]
        read = subprocess.run(command, capture_output=True, text=True, check=False)
        return read.stdout.strip() == "1" if read.returncode == 0 else None

    def set_keyboard_minimization(self, udid: str, value: bool | None) -> None:
        """Gives the preference that shrinks the keyboard of an iOS simulator a value, or deletes it for None."""
        command = ["xcrun", "simctl", "spawn", udid, "defaults"]
        if value is None:
            command += ["delete", UiTests.KEYBOARD_DOMAIN, UiTests.KEYBOARD_MINIMIZATION]
        else:
            command += ["write", UiTests.KEYBOARD_DOMAIN, UiTests.KEYBOARD_MINIMIZATION, "-bool", "YES" if value else "NO"]
        self.context.process.run(command)

    def run_on_mac(self, product: BuildProduct, platform: str, device: str | None, runner: Path, output: Path, classes: Sequence[str]) -> str:
        """Runs the tests on this Mac. The runner launches the app from the bundle that the run built and ends it after each test, so nothing installs or stays. Its clicks and keys reach the app in front, so the Mac runs nothing else meanwhile, such as on a runner of continuous integration, and the run holds the keys and selects the input sources that a test asks for through `MacKeyboard`, and gives the Mac back its input sources as the run ends. The runner of the Mac runs in a sandbox that writes nowhere outside its container, so its screenshots are attachments of the result bundle of the run, which the run exports into the folder of the screenshots, also when the tests failed."""
        if device is not None:
            raise UnsupportedTarget(f'The UI tests of "{platform}" run on this Mac, so they take no "--device".')
        project = (runner / "HaylenUITests.xcodeproj", UiTests.MAC_SCHEME, UiTests.mac_destination(), runner / "derived")
        self.context.terminal.step("Building the UI test runner of the Mac")
        self.context.process.run(UiTests.command("build-for-testing", *project))
        self.context.terminal.step(f'Running the UI tests of "{product.prepared.product.app.name}" on this Mac')
        results = UiTestResults(runner, platform, UiTests.MAC_NAME, classes, datetime.datetime.now())
        attachments = runner / "attachments"
        keyboard = MacKeyboard(UiTests.input_source_file(runner))

        answer = functools.partial(UiTests.answer_on_mac, results, keyboard)
        try:
            UiTestRun(self.context.terminal, answer=answer).run(UiTests.command("test-without-building", *project, classes, results.bundle), self.context.environment)
        except BaseException as error:
            self.clean_up_failed_mac_run(keyboard, results.bundle, attachments, output)
            if isinstance(error, BuildFailed):
                results.keep(error, self.context.terminal)
            raise
        keyboard.restore()
        self.export_screenshots(results.bundle, attachments, output)
        results.discard()
        return UiTests.MAC_NAME

    def clean_up_failed_mac_run(self, keyboard: MacKeyboard, results: Path, attachments: Path, output: Path) -> None:
        """Gives the Mac back its input sources and exports the screenshots of a run that failed or stopped, the screenshots only when the runner wrote a result bundle. What fails here only warns, so the failure of the run stays its error, and the screenshots stay in the result bundle."""
        try:
            keyboard.restore()
        except BuildFailed as error:
            self.context.terminal.warning(str(error))
        if not results.exists():
            return
        try:
            self.export_screenshots(results, attachments, output)
        except BuildFailed as error:
            self.context.terminal.warning(f"The screenshots of the UI tests stay in `{Repository.shown(results)}`, since they could not be exported: {error}")

    def export_screenshots(self, results: Path, attachments: Path, output: Path) -> None:
        """Exports the attachments of a result bundle and moves the screenshots that the tests added into the folder of the screenshots under the names the tests gave them."""
        Folders.remove(attachments)
        self.context.process.run(["xcrun", "xcresulttool", "export", "attachments", "--path", results, "--output-path", attachments])
        manifest = json.loads((attachments / "manifest.json").read_text(encoding="utf-8"))
        for test in manifest:
            for attachment in test["attachments"]:
                name = UiTests.attachment_name(attachment["suggestedHumanReadableName"])
                if name is not None and not attachment["isAssociatedWithFailure"]:
                    (attachments / attachment["exportedFileName"]).replace(output / name)
        Folders.remove(attachments)

    @staticmethod
    def attachment_name(suggested: str) -> str | None:
        """The file name of a screenshot that a test attached from the name that Xcode suggests for it, or None for another attachment."""
        match = UiTests.ATTACHMENT_NAME.match(suggested)
        return None if match is None else match["name"] + match["extension"]

    def run_on_device(self, product: BuildProduct, platform: str, device: str | None, runner: Path, output: Path, classes: Sequence[str]) -> str:
        """Runs the tests on a device, which gives the Mac no stream of its unified log, so the checks of the tests are the results. The tests write their screenshots into the container of the runner on the device, which the run copies back."""
        info = plistlib.loads((product.path / "Info.plist").read_bytes())
        target = AppleDevices(self.context).device(device, platform)
        # The command "codesign" describes a signature on its error output.
        signature = subprocess.run(["codesign", "--display", "--verbose=2", str(product.path)], capture_output=True, text=True, check=True).stderr
        project = (runner / "HaylenUITests.xcodeproj", UiTests.DEVICES[platform], f"id={target.identifier}", runner / "derived")
        self.context.terminal.step(f'Building the UI test runner for "{target.name}"')
        self.context.process.run([*UiTests.command("build-for-testing", *project), *UiTests.signing(UiTests.team(signature))])
        devicectl = ["xcrun", "devicectl", "device"]
        self.context.terminal.step(f'Installing the app on "{target.name}"')
        self.context.process.run([*devicectl, "install", "app", "--device", target.identifier, product.path])
        self.context.terminal.step(f'Running the UI tests on "{target.name}"')
        results = UiTestResults(runner, platform, target.name, classes, datetime.datetime.now())
        try:
            UiTestRun(self.context.terminal, answer=results.record).run(UiTests.command("test-without-building", *project, classes, results.bundle), self.context.environment)
        except BuildFailed as error:
            results.keep(error, self.context.terminal)
            raise
        finally:
            copy = [*devicectl, "copy", "from", "--device", target.identifier, "--domain-type", "appDataContainer", "--domain-identifier", UiTests.RUNNER]
            subprocess.run([*copy, "--source", UiTests.DEVICE_OUTPUT, "--destination", str(output)], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)
            for identifier in (info["CFBundleIdentifier"], UiTests.RUNNER):
                subprocess.run([*devicectl, "uninstall", "app", "--device", target.identifier, identifier], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)
        results.discard()
        return target.name

    def run_on_farm(self, product: BuildProduct, runner: Path, devices: Sequence[str], output: Path) -> dict[str, object]:
        """Runs the tests on the iPhones and iPads of Device Farm: the runner builds for any iOS device with the team of the app, and the app and the runner go to Device Farm as IPA files."""
        signature = subprocess.run(["codesign", "--display", "--verbose=2", str(product.path)], capture_output=True, text=True, check=True).stderr
        derived = runner / "derived"
        build = ["xcodebuild", "build-for-testing", "-project", runner / "HaylenUITests.xcodeproj", "-scheme", "iOS", "-destination", "generic/platform=iOS", "-derivedDataPath", derived]
        self.context.terminal.step("Building the UI test runner for any iOS device")
        self.context.process.run([*build, *UiTests.signing(UiTests.team(signature))])
        packages = runner / "device-farm"
        Folders.remove(packages)
        packages.mkdir(parents=True)
        app = UiTests.package(product.path, packages / "app.ipa")
        tests = UiTests.package(next((derived / "Build" / "Products").glob("*-iphoneos/*-Runner.app")), packages / "runner.ipa")
        return DeviceFarmUiTests(self.context).run("ios", app, tests, devices, output)

    @staticmethod
    def package(bundle: Path, ipa: Path) -> Path:
        """Packs an app into an IPA file, the app under the folder `Payload` of a zip file, as Device Farm takes it."""
        with zipfile.ZipFile(ipa, "w", compression=zipfile.ZIP_DEFLATED) as archive:
            for path in sorted(bundle.rglob("*")):
                archive.write(path, Path("Payload") / bundle.name / path.relative_to(bundle))
        return ipa
