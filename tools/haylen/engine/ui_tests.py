import math
import plistlib
import re
import subprocess
import time
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
from haylen.engine.ui_test_run import UiTestRun
from haylen.engine.unified_log import UnifiedLog
from haylen.platforms.apple.apple_devices import AppleDevices
from haylen.platforms.build_product import BuildProduct
from haylen.platforms.product_folders import ProductFolders
from haylen.toolchains.apple_simulators import AppleSimulators
from haylen.toolchains.xcodegen import XcodeGen


class UiTests:
    """Runs the UI tests of an app on an iOS, tvOS or visionOS simulator or device, or on Android through `AndroidUiTests`: the app builds through the pipeline for the target, and the XCUITest runner of `engine/tests/ui/apple/project.yml` builds with the Swift tests of the folder `ui-tests` of the app and drives the installed app, taps, the remote and the system UI included. The run fails on a failed test, and on a simulator also on an error that the engine logged meanwhile. It uninstalls the app and the runner, and shuts down a simulator that it booted."""

    PROJECT = Repository.ENGINE / "tests" / "ui" / "apple"
    FOLDER = "ui-tests"
    # The folder of the tests of each runner inside the folder of the UI tests, with their language and their files.
    RUNNERS = {"apple": ("Swift", "*.swift"), "android": ("Kotlin", "*.kt")}
    RUNNER = "com.haylen.uitests.xctrunner"
    # Each simulator platform of the UI tests with the scheme of the runner and the family of its simulators.
    PLATFORMS = {"ios-simulator": ("iOS", "iOS"), "tvos-simulator": ("tvOS", "tvOS"), "visionos-simulator": ("visionOS", "xrOS")}
    # Each device platform of the UI tests with the scheme of the runner.
    DEVICES = {"ios": "iOS", "tvos": "tvOS", "visionos": "visionOS"}
    # The longest that one UI test runs before xcodebuild stops it, so a gesture that the system never finishes fails the test instead of the run waiting forever.
    TEST_SECONDS = 600
    # The folder of the screenshots in the container of the runner on a device, which the run copies back.
    DEVICE_OUTPUT = "Library/Caches/ui-tests"
    # The development team in the signature of an app, as "codesign" describes it.
    TEAM = re.compile(r"^TeamIdentifier=(\w+)$", re.MULTILINE)
    # The file of the unified log of a simulator during a failed run, next to the result bundles of the run.
    SIMULATOR_LOG = "simulator.log"
    # The preference of the keyboard of an iOS simulator that the run turns off while the tests run and gives back afterwards: that the keyboard of the system shrinks to its bar while a hardware keyboard is attached, since a simulator reports the keyboard of the Mac it runs on as one and the tests type as a person does on a phone.
    KEYBOARD_DOMAIN = "com.apple.keyboard.preferences"
    KEYBOARD_MINIMIZATION = "AutomaticMinimizationEnabled"

    def __init__(self, context: ToolingContext, pipeline: AppPipeline) -> None:
        self.context = context
        self.pipeline = pipeline

    @staticmethod
    def command(action: str, project: Path, scheme: str, destination: str, derived: Path, classes: Sequence[str] = ()) -> list[object]:
        """The command that builds the runner of a scheme with "build-for-testing", or runs its tests on the simulator or device of that identifier with "test-without-building", whose output holds only the tests, the classes that it names alone or every class. The target of the runner of each platform takes the name of its scheme."""
        command: list[object] = ["xcodebuild", action, "-project", project, "-scheme", scheme, "-destination", f"id={destination}", "-derivedDataPath", derived]
        if action == "test-without-building":
            command += ["-test-timeouts-enabled", "YES", "-maximum-test-execution-time-allowance", str(UiTests.TEST_SECONDS), "-collect-test-diagnostics", "never"]
            command += [f"-only-testing:{scheme}/{name}" for name in classes]
        return command

    @staticmethod
    def signing(team: str) -> list[str]:
        """The settings that sign the runner of a device for the development team of the app, whose profiles Xcode makes as it builds."""
        return ["-allowProvisioningUpdates", f"DEVELOPMENT_TEAM={team}", "CODE_SIGN_STYLE=Automatic", "CODE_SIGN_IDENTITY=Apple Development"]

    @staticmethod
    def generation(environment: Mapping[str, str], tests: Path, runner: Path, identifier: str, output: str) -> dict[str, str]:
        """The environment of XcodeGen, which writes the tests of the app into the project of the runner, and the identifier of the app and the folder of the screenshots into the Info.plist of the runner."""
        return {**environment, "HAYLEN_UI_TESTS": str(tests), "HAYLEN_UI_RUNNER": str(runner), "HAYLEN_UI_APP": identifier, "HAYLEN_UI_OUTPUT": output}

    @staticmethod
    def tests(app: str, runner: str) -> tuple[Path, Path]:
        """The folder of the app and the folder of its UI tests for a runner, `apple` or `android`, which must hold at least one test in the language of the runner."""
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
        if platform not in UiTests.PLATFORMS and platform not in UiTests.DEVICES:
            names = ", ".join(f'"{name}"' for name in (*UiTests.PLATFORMS, *UiTests.DEVICES, "android"))
            raise UnsupportedTarget(f'The UI tests run on {names}. Pick one with "--target".')
        if Host.system() != "macos":
            raise UnsupportedTarget(f'The UI tests of Apple platforms run with Xcode, which needs macOS, and this machine runs "{Host.system()}".')
        folder, tests = UiTests.tests(app, "apple")

        request = ProductRequest(folder, platform, Configuration(configuration), Configuration.RELEASE, None)
        product = self.pipeline.build(self.pipeline.prepare(self.pipeline.resolve(request, build_sdk=True)))
        ReportPrinter.verification(self.pipeline.verify(product), self.context.terminal, f"`{Repository.shown(product.path)}`")
        output = ProductFolders.app_folder(folder) / "ui-tests" / platform
        Folders.remove(output)
        output.mkdir(parents=True)
        runner = Repository.ENGINE_BUILDS / f"{platform}-ui-tests"
        runner.mkdir(parents=True, exist_ok=True)
        self.context.terminal.step(f"Generating the UI test runner in `{Repository.shown(runner)}`")
        identifier = plistlib.loads((product.path / "Info.plist").read_bytes())["CFBundleIdentifier"]
        # A runner on a simulator writes its screenshots into the folder of the app on this Mac, and a runner on a device into its own container.
        screenshots = UiTests.DEVICE_OUTPUT if platform in UiTests.DEVICES else str(output)
        generate = [XcodeGen.path(self.context), "generate", "--quiet", "--spec", UiTests.PROJECT / "project.yml", "--project", runner]
        self.context.process.run(generate, env=UiTests.generation(self.context.environment, tests, runner, identifier, screenshots))

        if farm:
            return self.run_on_farm(product, runner, farm, ProductFolders.app_folder(folder) / "ui-tests" / "device-farm-ios")
        run = self.run_on_device if platform in UiTests.DEVICES else self.run_on_simulator
        name = run(product, platform, device, runner, output, classes)
        if any(output.iterdir()):
            self.context.terminal.info(f"The screenshots of the UI tests are in `{Repository.shown(output)}`.")
        self.context.terminal.success(f'The UI tests passed on "{name}".')
        return {"app": str(folder), "target": platform, "device": name, "screenshots": str(output)}

    def run_on_simulator(self, product: BuildProduct, platform: str, device: str | None, runner: Path, output: Path, classes: Sequence[str]) -> str:
        info = plistlib.loads((product.path / "Info.plist").read_bytes())
        scheme, family = UiTests.PLATFORMS[platform]
        with AppleSimulators(self.context).booted(family, device) as simulator:
            udid = simulator["udid"]
            project = (runner / "HaylenUITests.xcodeproj", scheme, udid, runner / "derived")
            self.context.terminal.step("Building the UI test runner")
            self.context.process.run(UiTests.command("build-for-testing", *project))
            self.context.process.run(["xcrun", "simctl", "install", udid, product.path])
            self.context.terminal.step(f'Running the UI tests on the simulator "{simulator["name"]}"')
            phone = family == "iOS"
            minimization = self.keyboard_minimization(udid) if phone else None
            if phone:
                self.set_keyboard_minimization(udid, False)
            started = time.monotonic()
            try:
                with UnifiedLog(self.context.terminal, info["CFBundleExecutable"], ["xcrun", "simctl", "spawn", udid]) as errors:
                    UiTestRun(self.context.terminal).run(UiTests.command("test-without-building", *project, classes), self.context.environment)
            except BuildFailed:
                self.keep_simulator_log(udid, time.monotonic() - started, runner / "derived" / "Logs" / "Test")
                raise
            finally:
                if phone:
                    self.set_keyboard_minimization(udid, minimization)
                for identifier in (info["CFBundleIdentifier"], UiTests.RUNNER):
                    subprocess.run(["xcrun", "simctl", "uninstall", udid, identifier], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)
        if errors:
            raise BuildFailed(f"The UI tests passed, but the engine logged {len(errors)} errors meanwhile, the first being: {errors[0]}")
        return simulator["name"]

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

    @staticmethod
    def log_command(udid: str, seconds: float) -> list[str]:
        """The command that writes the unified log of a simulator over the last whole minutes that cover a run of that many seconds."""
        minutes = math.ceil(seconds / 60) + 1
        return ["xcrun", "simctl", "spawn", udid, "log", "show", "--last", f"{minutes}m", "--style", "compact"]

    def keep_simulator_log(self, udid: str, seconds: float, folder: Path) -> None:
        """Keeps the unified log of the simulator during a failed run next to the result bundles of the run, so a run on a machine that nobody watches, such as continuous integration, tells what the system did meanwhile, such as an answer of a daemon that never came."""
        folder.mkdir(parents=True, exist_ok=True)
        path = folder / UiTests.SIMULATOR_LOG
        with path.open("w", encoding="utf-8") as output:
            subprocess.run(UiTests.log_command(udid, seconds), stdout=output, stderr=subprocess.STDOUT, check=False)
        self.context.terminal.info(f"The log of the simulator during the UI tests is in `{Repository.shown(path)}`.")

    def run_on_device(self, product: BuildProduct, platform: str, device: str | None, runner: Path, output: Path, classes: Sequence[str]) -> str:
        """Runs the tests on a device, which gives the Mac no stream of its unified log, so the checks of the tests are the results. The tests write their screenshots into the container of the runner on the device, which the run copies back."""
        info = plistlib.loads((product.path / "Info.plist").read_bytes())
        target = AppleDevices(self.context).device(device, platform)
        # The command "codesign" describes a signature on its error output.
        signature = subprocess.run(["codesign", "--display", "--verbose=2", str(product.path)], capture_output=True, text=True, check=True).stderr
        project = (runner / "HaylenUITests.xcodeproj", UiTests.DEVICES[platform], target.identifier, runner / "derived")
        self.context.terminal.step(f'Building the UI test runner for "{target.name}"')
        self.context.process.run([*UiTests.command("build-for-testing", *project), *UiTests.signing(UiTests.team(signature))])
        devicectl = ["xcrun", "devicectl", "device"]
        self.context.terminal.step(f'Installing the app on "{target.name}"')
        self.context.process.run([*devicectl, "install", "app", "--device", target.identifier, product.path])
        self.context.terminal.step(f'Running the UI tests on "{target.name}"')
        try:
            UiTestRun(self.context.terminal).run(UiTests.command("test-without-building", *project, classes), self.context.environment)
        finally:
            copy = [*devicectl, "copy", "from", "--device", target.identifier, "--domain-type", "appDataContainer", "--domain-identifier", UiTests.RUNNER]
            subprocess.run([*copy, "--source", UiTests.DEVICE_OUTPUT, "--destination", str(output)], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)
            for identifier in (info["CFBundleIdentifier"], UiTests.RUNNER):
                subprocess.run([*devicectl, "uninstall", "app", "--device", target.identifier, identifier], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)
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
