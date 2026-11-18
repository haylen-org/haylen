import contextlib
import os
import signal
import subprocess
import tempfile
import time
from collections.abc import Sequence
from pathlib import Path
from typing import IO

from haylen.core.errors.build_failed import BuildFailed
from haylen.core.folders import Folders
from haylen.core.host import Host
from haylen.core.tooling_context import ToolingContext
from haylen.toolchains.chrome import Chrome


class HeadlessBrowser:
    """A headless Chrome of its own with a profile that goes away with it, whose debugging protocol the drivers of the web checks and the GPU tests of the web talk to. The browser picks the port of the protocol itself and names it in the file `DevToolsActivePort` of its profile, so no other program takes the port between its choice and the start. The browser runs in a process group of its own, so the profile goes away only once the browser and every helper process it started have ended."""

    # A browser whose files are in memory starts within a second. Its first start on a machine reads the browser from the disk page by page, which took from under a second to more than 30 seconds on the runners of continuous integration, so the jobs there read the browser in one pass before their first page, as the action `.github/actions/browser` describes.
    START_SECONDS = 30
    # The helpers of the browser write its profile until they exit, which they do within this time of the request to end.
    EXIT_SECONDS = 30
    # The browser writes the port of its debugging protocol and the path of its target into this file of its profile once it listens on the port.
    ACTIVE_PORT = "DevToolsActivePort"
    # The lines of the output of a browser that did not start, which its error shows.
    OUTPUT_LINES = 20
    # The flags that make WebGPU and WebGL2 draw through SwiftShader, so the pages start on a machine without a GPU for them.
    SOFTWARE_GPU = ("--enable-unsafe-webgpu", "--enable-unsafe-swiftshader", "--use-angle=swiftshader")

    def __init__(self, context: ToolingContext, arguments: Sequence[str], purpose: str) -> None:
        self.context = context
        self.arguments = list(arguments)
        self.purpose = purpose
        self.process: subprocess.Popen | None = None
        self.profile: Path | None = None
        self.output: IO[bytes] | None = None

    def __enter__(self) -> int:
        """Starts the browser and returns the port of its debugging protocol once it listens on it."""
        self.profile = Path(tempfile.mkdtemp())
        self.output = tempfile.TemporaryFile()
        command = [
            Chrome.require(self.context.environment),
            "--headless=new",
            "--remote-debugging-port=0",
            f"--user-data-dir={self.profile}",
            "--no-first-run",
            *self.arguments,
            "about:blank",
        ]
        self.process = subprocess.Popen(command, stdout=self.output, stderr=subprocess.STDOUT, start_new_session=True)
        try:
            return self.wait_until_it_listens(self.process, self.profile / HeadlessBrowser.ACTIVE_PORT)
        except BaseException:
            self.__exit__()
            raise

    def wait_until_it_listens(self, process: subprocess.Popen, active_port: Path) -> int:
        """Returns the port that the browser listens on, and fails at once when the browser ends before it listens."""
        deadline = time.monotonic() + HeadlessBrowser.START_SECONDS
        while True:
            port = HeadlessBrowser.read_port(active_port)
            if port is not None:
                return port

            code = process.poll()
            if code is not None:
                raise self.failure(f"The headless browser of {self.purpose} ended with the code {code} before it listened for its driver.")
            if time.monotonic() > deadline:
                raise self.failure(f"The headless browser of {self.purpose} did not start within {HeadlessBrowser.START_SECONDS} seconds.")
            time.sleep(0.05)

    @staticmethod
    def read_port(active_port: Path) -> int | None:
        """The port that the first line of the file names, once the browser wrote the file with its second line, the path of its target."""
        try:
            lines = active_port.read_text(encoding="utf-8").splitlines()
        except FileNotFoundError:
            return None
        if len(lines) < 2:
            return None
        return int(lines[0])

    def failure(self, sentence: str) -> BuildFailed:
        """The error of a start that failed, with the last lines of the output of the browser when it wrote any."""
        self.output.seek(0)
        lines = self.output.read().decode("utf-8", errors="replace").splitlines()
        if not lines:
            return BuildFailed(f"{sentence} It wrote no output.")
        return BuildFailed(f"{sentence} The last lines of its output follow.", details="\n".join(lines[-HeadlessBrowser.OUTPUT_LINES :]))

    def __exit__(self, *_: object) -> None:
        if self.process is not None:
            self.end_process_tree(self.process)
            self.process = None
        if self.profile is not None:
            Folders.remove(self.profile)
            self.profile = None
        if self.output is not None:
            self.output.close()
            self.output = None

    def end_process_tree(self, process: subprocess.Popen) -> None:
        """Ends the browser and every process it started, and returns once none of them is left. On Windows the processes are the tree of the browser, which "taskkill" ends as a whole."""
        if Host.system() == "windows":
            if process.poll() is None:
                self.context.process.run(["taskkill", "/PID", str(process.pid), "/T", "/F"])
            process.wait()
            return

        with contextlib.suppress(ProcessLookupError):
            os.killpg(process.pid, signal.SIGTERM)
        process.wait()

        deadline = time.monotonic() + HeadlessBrowser.EXIT_SECONDS
        while True:
            try:
                os.killpg(process.pid, 0)
            except ProcessLookupError:
                return
            except PermissionError:
                # The kernel of macOS answers a group whose processes all ended and wait for their parent to collect them with EPERM instead of ESRCH.
                if Host.system() != "macos":
                    raise
                return
            if time.monotonic() > deadline:
                raise BuildFailed(
                    f"The processes of the headless browser of {self.purpose} were still running {HeadlessBrowser.EXIT_SECONDS} seconds after the request to end. End them, and try again."
                )
            time.sleep(0.1)
