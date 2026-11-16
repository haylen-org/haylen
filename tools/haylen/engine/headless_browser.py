import contextlib
import os
import signal
import socket
import subprocess
import tempfile
import time
import urllib.request
from collections.abc import Sequence
from pathlib import Path

from haylen.core.errors.build_failed import BuildFailed
from haylen.core.folders import Folders
from haylen.core.host import Host
from haylen.core.tooling_context import ToolingContext
from haylen.toolchains.chrome import Chrome


class HeadlessBrowser:
    """A headless Chrome of its own with a profile that goes away with it, whose debugging protocol the drivers of the web checks and the GPU tests of the web talk to. The browser runs in a process group of its own, so the profile goes away only once the browser and every helper process it started have ended."""

    START_SECONDS = 30
    # The helpers of the browser write its profile until they exit, which they do within this time of the request to end.
    EXIT_SECONDS = 30
    # The flags that make WebGPU and WebGL2 draw through SwiftShader, so the pages start on a machine without a GPU for them.
    SOFTWARE_GPU = ("--enable-unsafe-webgpu", "--enable-unsafe-swiftshader", "--use-angle=swiftshader")

    def __init__(self, context: ToolingContext, arguments: Sequence[str], purpose: str) -> None:
        self.context = context
        self.arguments = list(arguments)
        self.purpose = purpose
        self.port = 0
        self.process: subprocess.Popen | None = None
        self.profile: Path | None = None

    @staticmethod
    def free_port() -> int:
        with socket.socket() as probe:
            probe.bind(("127.0.0.1", 0))
            return probe.getsockname()[1]

    def __enter__(self) -> int:
        """Starts the browser and returns the port of its debugging protocol once it answers."""
        self.port = HeadlessBrowser.free_port()
        self.profile = Path(tempfile.mkdtemp())
        command = [
            Chrome.require(self.context.environment),
            "--headless=new",
            f"--remote-debugging-port={self.port}",
            f"--user-data-dir={self.profile}",
            "--no-first-run",
            *self.arguments,
            "about:blank",
        ]
        self.process = subprocess.Popen(command, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, start_new_session=True)
        try:
            self.wait_until_it_answers()
        except BaseException:
            self.__exit__()
            raise
        return self.port

    def wait_until_it_answers(self) -> None:
        deadline = time.monotonic() + HeadlessBrowser.START_SECONDS
        while True:
            try:
                urllib.request.urlopen(f"http://127.0.0.1:{self.port}/json/version", timeout=1).close()
                return
            except OSError:
                if time.monotonic() > deadline:
                    raise BuildFailed(f"The headless browser of {self.purpose} did not start within {HeadlessBrowser.START_SECONDS} seconds.") from None
                time.sleep(0.2)

    def __exit__(self, *_: object) -> None:
        if self.process is not None:
            self.end_process_tree(self.process)
            self.process = None
        if self.profile is not None:
            Folders.remove(self.profile)
            self.profile = None

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
