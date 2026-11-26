import subprocess
import threading
from collections import deque
from collections.abc import Callable
from pathlib import Path

from haylen.cli.report_printer import ReportPrinter
from haylen.clock import Clock
from haylen.core.app_manifest import AppManifest
from haylen.core.app_pipeline import AppPipeline
from haylen.core.configuration import Configuration
from haylen.core.errors.build_failed import BuildFailed
from haylen.core.errors.invalid_manifest import InvalidManifest
from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.core.host import Host
from haylen.core.product_request import ProductRequest
from haylen.core.repository import Repository
from haylen.core.tooling_context import ToolingContext
from haylen.platforms.apple.app_bundle import AppBundle


class MacAppCheck:
    """Checks that an app quits on this Mac in every way a person quits it, as a macOS or a Mac Catalyst app. The app builds for the processor of this Mac in the configuration it is asked for and starts once for each way. Once its window shows and the app ran `SETTLE_SECONDS` more, the check closes the window with its close button, or quits the app with the quit event that the Dock and logging out send. The process must then end with the status 0 within `QUIT_SECONDS`. The check fails when the process ends before it is asked to, ends with another status or a crash, or keeps running, and it stops a process that it left running. It reaches the window through the accessibility of macOS, which the app that runs the check, such as the terminal, needs in Privacy & Security. Its waits take their time from `clock`, and `start` starts the process of the app as a `Run`."""

    TARGETS = ("macos", "catalyst")
    # Each way of quitting, with how the steps and the failures of the check name it.
    WAYS = {"closeButton": "closing its window with the close button", "quitEvent": "quitting it as the Dock does"}
    START_SECONDS = 120
    SETTLE_SECONDS = 3
    QUIT_SECONDS = 10
    POLL_SECONDS = 0.5
    LOG_LINES = 40
    # The scripts take the process identifier of the app as their argument. System Events reaches the windows of any app through its accessibility, and `NSRunningApplication` sends the quit event of the Dock, which needs no scripting of the app.
    # System Events knows the process only once it registered with the window server, and until then the app counts no window.
    COUNT_WINDOWS = (
        "on run argv",
        'tell application "System Events"',
        "set found to processes whose unix id is (item 1 of argv as integer)",
        "if found is {} then return 0",
        "return count windows of (item 1 of found)",
        "end tell",
        "end run",
    )
    CLOSE_WINDOW = (
        "on run argv",
        'tell application "System Events" to click (first button of window 1 of (first process whose unix id is (item 1 of argv as integer)) whose subrole is "AXCloseButton")',
        "end run",
    )
    QUIT_EVENT = 'ObjC.import("AppKit"); function run(argv) { return $.NSRunningApplication.runningApplicationWithProcessIdentifier(Number(argv[0])).terminate; }'

    def __init__(self, context: ToolingContext, pipeline: AppPipeline, clock: Clock, start: Callable[[Path], MacAppCheck.Run]) -> None:
        self.context = context
        self.pipeline = pipeline
        self.clock = clock
        self.start = start

    def run(self, app: str, target: str, configuration: str) -> dict[str, object]:
        folder = Path(app).expanduser().resolve()
        if not AppManifest.is_app(folder):
            raise InvalidManifest(f'The folder `{Repository.shown(folder)}` holds no "{AppManifest.FILE}". Name the folder of the app that the check quits.')
        if Host.system() != "macos":
            raise UnsupportedTarget(f'The quit check of a Mac app runs on a Mac, and this machine runs "{Host.system()}".')

        request = ProductRequest(folder, target, Configuration.parse(configuration), Configuration.RELEASE, (Host.architecture(),))
        product = self.pipeline.build(self.pipeline.prepare(self.pipeline.resolve(request, build_sdk=True)))
        ReportPrinter.verification(self.pipeline.verify(product), self.context.terminal, f"`{Repository.shown(product.path)}`")
        bundle = AppBundle(self.context, product.path, True)
        name = product.prepared.product.app.name

        for way, description in MacAppCheck.WAYS.items():
            self.context.terminal.step(f'Starting "{name}" and {description}')
            self.check(bundle.executable, way)
        self.context.terminal.success(f'The app "{name}" ended with the status 0 on "{target}" after each way of quitting it.')
        return {"app": str(folder), "target": target, "configuration": configuration, "ways": list(MacAppCheck.WAYS)}

    class Run:
        """The process of one start of the app with the last lines of its output, which a failure shows. The status of `poll` and `wait` is `None` while the process runs, and negative for the signal that stopped it."""

        # How long the output of an app that ended takes to reach the check at most.
        DRAIN_SECONDS = 1.0

        def __init__(self, executable: Path) -> None:
            self.output: deque[str] = deque(maxlen=MacAppCheck.LOG_LINES)
            self.process = subprocess.Popen([str(executable)], stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="replace")
            self.pid = self.process.pid
            self.reader = threading.Thread(target=self.keep, daemon=True)
            self.reader.start()

        def keep(self) -> None:
            for line in self.process.stdout:
                self.output.append(line.rstrip("\n"))

        def poll(self) -> int | None:
            return self.process.poll()

        def wait(self, seconds: float) -> int | None:
            """Waits for the process to end for `seconds` at most."""
            try:
                return self.process.wait(seconds)
            except subprocess.TimeoutExpired:
                return None

        def failed(self, reason: str) -> BuildFailed:
            """The failure with the reason and the last lines of the log of the app, all of them once the app ended."""
            if self.process.poll() is not None:
                self.reader.join(MacAppCheck.Run.DRAIN_SECONDS)
            return BuildFailed(f"{reason} The last lines of its log follow.", details="\n".join(self.output))

        def stop(self) -> None:
            if self.process.poll() is None:
                self.process.kill()
                self.process.wait()

    def check(self, executable: Path, way: str) -> None:
        """Starts the app, quits it in the way that `way` names once its window showed and it settled, and fails unless its process ends with the status 0. A process that is left running ends with the check."""
        run = self.start(executable)
        try:
            self.quit(run, way)
        finally:
            run.stop()

    def quit(self, run: Run, way: str) -> None:
        description = MacAppCheck.WAYS[way]
        self.wait_for_window(run)
        self.clock.sleep(MacAppCheck.SETTLE_SECONDS)
        if run.poll() is not None:
            raise run.failed("The app stopped before the check quit it.")

        pid = str(run.pid)
        if way == "closeButton":
            self.ask(run, ["osascript", *MacAppCheck.lines(MacAppCheck.CLOSE_WINDOW), pid])
        else:
            self.ask(run, ["osascript", "-l", "JavaScript", "-e", MacAppCheck.QUIT_EVENT, pid])

        status = run.wait(MacAppCheck.QUIT_SECONDS)
        if status is None:
            raise run.failed(f"The process of the app kept running {MacAppCheck.QUIT_SECONDS} seconds after {description}.")
        if status < 0:
            raise run.failed(f"The app was stopped by the signal {-status} after {description}.")
        if status != 0:
            raise run.failed(f"The app ended with the status {status} after {description}.")

    def wait_for_window(self, run: Run) -> None:
        deadline = self.clock.monotonic() + MacAppCheck.START_SECONDS
        while True:
            if run.poll() is not None:
                raise run.failed("The app stopped while it started.")
            windows = self.ask(run, ["osascript", *MacAppCheck.lines(MacAppCheck.COUNT_WINDOWS), str(run.pid)])
            if int(windows or "0") > 0:
                return
            if self.clock.monotonic() > deadline:
                raise run.failed(f"The app showed no window within {MacAppCheck.START_SECONDS} seconds.")
            self.clock.sleep(MacAppCheck.POLL_SECONDS)

    def ask(self, run: Run, command: list[str]) -> str:
        """Runs a script about the app and returns what it printed. A script that fails while the app still runs shows that macOS does not let the check reach the windows of the app."""
        status, printed = self.context.process.attempt(command)
        if status == 0:
            return printed.decode("utf-8", errors="replace").strip()
        if run.poll() is not None:
            raise run.failed("The app stopped before the check quit it.")
        raise BuildFailed(
            'The check cannot reach the windows of the app, because macOS does not allow it. Allow the app that runs the check, such as the terminal, in "Accessibility" of "Privacy & Security" in System Settings.'
        )

    @staticmethod
    def lines(script: tuple[str, ...]) -> list[str]:
        """The options of `osascript` that hand it a script one line after the other."""
        return [part for line in script for part in ("-e", line)]
