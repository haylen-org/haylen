import io
import json
import os
import socket
import time
from collections.abc import Callable, Mapping
from pathlib import Path
from types import MappingProxyType

import pytest

from haylen.core.package_layout import PackageLayout
from haylen.core.repository import Repository
from haylen.core.tooling_context import ToolingContext
from haylen.development.development_server import DevelopmentServer
from haylen.platforms.build_product import BuildProduct
from haylen.platforms.launch_request import LaunchRequest
from haylen.platforms.preparation_state import PreparationState
from haylen.platforms.prepared_project import PreparedProject
from haylen.platforms.product_folders import ProductFolders
from haylen.process import Process
from haylen.terminal import Terminal
from tests.support.tooling_fixture import ToolingFixture


class DevelopmentFixture:
    """An app in a temporary folder with Lua modules and content, the context of the tooling with a terminal that prints into a text, the layout of its package and the hashes of its files as they ship. The build folder of the apps moves into the temporary folder, so the record of a run never reaches the build folder of the checkout."""

    HELLO_KEY = "dGhlIHNhbXBsZSBub25jZQ=="

    def __init__(self, folder: Path, monkeypatch: pytest.MonkeyPatch, environment: Mapping[str, str] | None = None) -> None:
        monkeypatch.setattr(Repository, "APPS", folder / "apps")
        self.tooling = ToolingFixture(folder)
        self.scratch = folder
        self.folder = self.tooling.app({}).resolve()
        self.write("source/level.lua", "return {}", later=False)
        self.write("content/data.json", "{}", later=False)
        self.output = io.StringIO()
        terminal = Terminal(self.output, self.output, environment or {}, folder / "logs" / "test.log")
        self.context = ToolingContext(terminal, Process(terminal), 1, dict(os.environ))
        self.layout = PackageLayout.of(self.folder, {}, True)
        self.manifest = DevelopmentServer.hash_files(self.layout.files())

    def write(self, path: str, text: str, later: bool = True) -> Path:
        """Writes a file of the app, by default with a time later than any scan saw, as a save of the developer has."""
        file = self.folder / path
        file.parent.mkdir(parents=True, exist_ok=True)
        file.write_text(text, encoding="utf-8")
        if later:
            DevelopmentFixture.later(file)
        return file

    @staticmethod
    def later(file: Path) -> None:
        """Moves the time of a file seconds ahead, so a scan sees it changed even within the resolution of the clock of the file system."""
        status = file.stat()
        os.utime(file, ns=(status.st_atime_ns, status.st_mtime_ns + 5_000_000_000))

    def server(self) -> DevelopmentServer:
        return DevelopmentServer(self.context, self.layout, self.manifest)

    def product(self, configuration: str = "Debug") -> BuildProduct:
        """A built product of the app for the fake backend, as `app run` hands it to a backend."""
        if not self.tooling.sdks:
            self.tooling.sdk()
        resolved = self.tooling.resolve(self.folder, configuration=configuration)
        state = PreparationState("", "fake", "", "", "", "", "", MappingProxyType({}))
        folders = ProductFolders(self.scratch / "product")
        prepared = PreparedProject(resolved, folders, folders.project, "fake", state, False)
        return BuildProduct(prepared, folders.products / "app", "app", (), ())

    @staticmethod
    def launch(development: bool, port: int = 0) -> LaunchRequest:
        return LaunchRequest(device=None, development=development, host="127.0.0.1", port=port, debugger_port=0, options={})

    @staticmethod
    def free_port() -> int:
        with socket.socket() as probe:
            probe.bind(("127.0.0.1", 0))
            return probe.getsockname()[1]

    @staticmethod
    def wait(condition: Callable[[], bool]) -> None:
        deadline = time.monotonic() + 5
        while not condition():
            assert time.monotonic() < deadline, "The condition did not hold in time."
            time.sleep(0.01)

    @staticmethod
    def masked(opcode: int, payload: bytes) -> bytes:
        """A frame as an app sends it, masked and shorter than 126 bytes."""
        mask = bytes([9, 8, 7, 6])
        return bytes([0x80 | opcode, 0x80 | len(payload)]) + mask + bytes(byte ^ mask[index % 4] for index, byte in enumerate(payload))

    @staticmethod
    def connect(port: int, token: str) -> tuple[socket.socket, io.BufferedReader, str]:
        """Opens the WebSocket of the development path as an app does, and returns the socket, its stream after the headers and the status line of the answer."""
        connection = socket.create_connection(("127.0.0.1", port), timeout=5)
        request = (
            f"GET {DevelopmentServer.PATH}?token={token} HTTP/1.1\r\nHost: localhost\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
            f"Sec-WebSocket-Key: {DevelopmentFixture.HELLO_KEY}\r\nSec-WebSocket-Version: 13\r\n\r\n"
        )
        connection.sendall(request.encode())
        stream = connection.makefile("rb")
        status = stream.readline().decode()
        while stream.readline() not in (b"\r\n", b""):
            pass
        return connection, stream, status

    @staticmethod
    def hello(session: str, revision: int) -> bytes:
        return json.dumps({"type": "hello", "session": session, "revision": revision}).encode()
