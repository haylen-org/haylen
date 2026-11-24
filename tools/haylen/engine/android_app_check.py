import re
import subprocess
import time
from pathlib import Path

from haylen.cli.report_printer import ReportPrinter
from haylen.core.app_manifest import AppManifest
from haylen.core.app_pipeline import AppPipeline
from haylen.core.configuration import Configuration
from haylen.core.errors.build_failed import BuildFailed
from haylen.core.errors.invalid_manifest import InvalidManifest
from haylen.core.product_request import ProductRequest
from haylen.core.repository import Repository
from haylen.core.tooling_context import ToolingContext
from haylen.engine.android_test_device import AndroidTestDevice
from haylen.engine.web_start_report import WebStartReport
from haylen.platforms.android.android_launcher import AndroidLauncher
from haylen.toolchains.android_debug_bridge import AndroidDebugBridge
from haylen.toolchains.android_sdk import AndroidSdk


class AndroidAppCheck:
    """Checks that an app starts on Android: the app builds for the ABI of the device that `--device` names, the only one connected or an emulator that the check starts without a window, in the configuration it is asked for, installs and starts from its launcher intent, while the check reads the log of the engine and the crashes of Java. The check fails when the process of the app ends, Java reports a crash, such as a native method that the library of the app lacks, the engine logs an error, such as the report of the error screen, or the app does not log the graphics API it draws through within `START_SECONDS`, and it passes once the app has run `SETTLE_SECONDS` after that line without any of those. It uninstalls the app, and shuts down an emulator that it started."""

    TARGET = "android"
    START_SECONDS = 180
    SETTLE_SECONDS = 10
    POLL_SECONDS = 1.0
    # A line of logcat in its "tag" format, with its level, its tag, which logcat pads, and its message.
    LINE = re.compile(r"([VDIWEF])/(\S+)\s*: (.*)")
    # The line that the engine logs once its GPU device started, with the name of its graphics API.
    DRAWS_THROUGH = re.compile(r'The app draws through "([^"]+)"\.')
    # The levels of logcat that fail the check, for the lines of the engine and of the runtime of Java, whose crashes it logs at the level of errors.
    FAILURES = {AndroidLauncher.LOG_TAG: ("E", "F"), AndroidLauncher.CRASH_TAG: ("E", "F")}
    FILTERS = (f"{AndroidLauncher.LOG_TAG}:V", f"{AndroidLauncher.CRASH_TAG}:E", "*:S")

    def __init__(self, context: ToolingContext, pipeline: AppPipeline) -> None:
        self.context = context
        self.pipeline = pipeline
        self.sdk = AndroidSdk(context.environment)
        self.bridge = AndroidDebugBridge(context, self.sdk)
        self.launcher = AndroidLauncher(context)

    def run(self, app: str, configuration: str, requested: str | None) -> dict[str, object]:
        folder = Path(app).expanduser().resolve()
        if not AppManifest.is_app(folder):
            raise InvalidManifest(f'The folder `{Repository.shown(folder)}` holds no "{AppManifest.FILE}". Name the folder of the app that the check starts.')
        device = AndroidTestDevice(self.context, self.sdk, requested)
        request = ProductRequest(folder, AndroidAppCheck.TARGET, Configuration.parse(configuration), Configuration.RELEASE, (device.abi(),))
        product = self.pipeline.build(self.pipeline.prepare(self.pipeline.resolve(request, build_sdk=True)))
        ReportPrinter.verification(self.pipeline.verify(product), self.context.terminal, f"`{Repository.shown(product.path)}`")
        package = product.prepared.product.app.identifier

        with device as serial:
            name = self.bridge.name(serial)
            self.context.terminal.step(f'Installing the app on "{name}"')
            self.context.process.run(self.bridge.command(serial, "install", "-r", product.path))
            try:
                self.context.process.run(self.bridge.command(serial, "logcat", "-b", "all", "-c"))
                self.context.terminal.step(f'Starting "{package}" in the "{configuration}" configuration on "{name}"')
                self.launcher.start(serial, package, None)
                api = self.follow(serial, package)
            finally:
                subprocess.run([str(part) for part in self.bridge.command(serial, "uninstall", package)], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)
        self.context.terminal.success(f'The app "{package}" started on "{name}" and drew through "{api}" for {AndroidAppCheck.SETTLE_SECONDS} seconds without an error.')
        return {"app": str(folder), "target": AndroidAppCheck.TARGET, "configuration": configuration, "device": name, "graphicsApi": api}

    def follow(self, serial: str, package: str) -> str:
        """Reads the log of the app until it settles after it logged its graphics API, and returns that API, or fails with the first reason of a start that went wrong and the last lines of the log."""
        deadline = time.monotonic() + AndroidAppCheck.START_SECONDS
        settled: float | None = None
        while True:
            lines = self.context.process.capture(self.bridge.command(serial, "logcat", "-d", "-b", "main", "-b", "crash", "-v", "tag", *AndroidAppCheck.FILTERS)).splitlines()
            details = "\n".join(lines[-WebStartReport.LOG_LINES :])
            entries = [entry.groups() for entry in map(AndroidAppCheck.LINE.fullmatch, lines) if entry is not None]
            failure = next((message for level, tag, message in entries if level in AndroidAppCheck.FAILURES.get(tag, ())), None)
            if failure is not None:
                raise BuildFailed(f'The app "{package}" logged an error while it started: {failure} The last lines of its log follow.', details=details)
            if not self.launcher.pid(serial, package):
                raise BuildFailed(f'The app "{package}" stopped while it started. The last lines of its log follow.', details=details)

            drawn = next((found[1] for _, tag, message in entries if tag == AndroidLauncher.LOG_TAG and (found := AndroidAppCheck.DRAWS_THROUGH.search(message))), None)
            now = time.monotonic()
            if drawn is not None and settled is None:
                settled = now + AndroidAppCheck.SETTLE_SECONDS
            if settled is not None and now >= settled:
                return drawn
            if settled is None and now > deadline:
                raise BuildFailed(
                    f'The app "{package}" did not log the graphics API it draws through within {AndroidAppCheck.START_SECONDS} seconds. The last lines of its log follow.', details=details
                )
            time.sleep(AndroidAppCheck.POLL_SECONDS)
