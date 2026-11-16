import contextlib
import json
import socket
import threading
from pathlib import Path

from haylen.core.errors.development_failed import DevelopmentFailed
from haylen.core.package_layout import PackageLayout
from haylen.development.debug_message import DebugMessage
from haylen.development.development_client import DevelopmentClient
from haylen.development.websocket import WebSocket
from haylen.terminal import Terminal


class DebugRelay:
    """Carries the messages of the Debug Adapter Protocol between one editor, which attaches to a TCP port of this machine, and the app that connected last to the development server, whose engine runs the debugger. The paths of the editor become package paths, such as `source/main.lua`, and back. The relay keeps the requests that configure the debugger and sends them again to an app that connects later, such as one that restarted, answering none of them twice, and holds the requests of the editor while no app is connected."""

    # The requests that configure the debugger, which the relay sends again to an app that connects after them.
    CONFIGURATION = ("initialize", "attach", "setBreakpoints", "setExceptionBreakpoints", "configurationDone")
    # The sequence numbers of the requests that the relay sends again start here, so the answers of the app to them never reach the editor.
    REPLAY_SEQUENCE = 1 << 40

    def __init__(self, layout: PackageLayout, port: int, terminal: Terminal) -> None:
        self.layout = layout
        self.terminal = terminal
        try:
            self.listener = socket.create_server(("127.0.0.1", port))
        except OSError as error:
            raise DevelopmentFailed(f'The debugger cannot listen on port {port}: {error.strerror}. Stop what uses the port, or pick another one with "--debugger-port".') from error
        self.port = self.listener.getsockname()[1]
        self.lock = threading.Lock()
        self.editor: socket.socket | None = None
        self.app: DevelopmentClient | None = None
        self.configuration: dict[tuple[str, str | None], dict] = {}
        self.pending: list[dict] = []
        self.replayed = DebugRelay.REPLAY_SEQUENCE
        self.silenced = 0

    def start(self) -> None:
        threading.Thread(target=self.accept, daemon=True).start()

    def close(self) -> None:
        self.listener.close()
        with self.lock:
            editor, self.editor = self.editor, None
        if editor is not None:
            editor.close()

    def accept(self) -> None:
        """Takes one editor at a time, and turns away another one while an editor is attached."""
        while True:
            try:
                connection, _ = self.listener.accept()
            except OSError:
                return
            with self.lock:
                busy = self.editor is not None
                if not busy:
                    self.editor = connection

            if busy:
                connection.close()
                self.terminal.warning(f"An editor tried to attach to port {self.port} while another one is attached, so it was turned away.")
                continue
            self.terminal.info(f"An editor attached to the debugger on port {self.port}.")
            threading.Thread(target=self.read, args=(connection,), daemon=True).start()

    def read(self, connection: socket.socket) -> None:
        """Hands every message of the editor to the app until the editor leaves, and then lets the app forget it, as a request "disconnect" does."""
        stream = connection.makefile("rb")
        with contextlib.suppress(OSError, ValueError):
            while (message := DebugMessage.read(stream)) is not None:
                self.from_editor(message)

        with self.lock:
            if self.editor is connection:
                self.editor = None
            self.configuration.clear()
            self.pending.clear()
            app = self.app
        if app is not None:
            self.send_app(app, self.replay_request("disconnect"))
        connection.close()
        self.terminal.info("The editor left the debugger.")

    def to_package(self, path: str) -> str:
        return self.layout.to_package(Path(path)) or path

    def to_editor(self, path: str) -> str:
        return path if Path(path).is_absolute() else str(self.layout.to_disk(path))

    def replay_request(self, command: str, arguments: dict | None = None) -> dict:
        self.replayed += 1
        return {"seq": self.replayed, "type": "request", "command": command, "arguments": arguments or {}}

    def from_editor(self, message: dict) -> None:
        converted = DebugMessage.source_paths(message, self.to_package)
        with self.lock:
            app = self.app
            if app is None:
                self.pending.append(converted)
                return
            self.remember(converted)
        self.send_app(app, converted)

    def remember(self, message: dict) -> None:
        """Keeps a request that configures the debugger, the last one of its kind, and of the breakpoints the last one of each file."""
        command = message.get("command")
        if message.get("type") != "request" or command not in DebugRelay.CONFIGURATION:
            return
        path = message.get("arguments", {}).get("source", {}).get("path") if command == "setBreakpoints" else None
        self.configuration[(command, path)] = message

    def send_app(self, app: DevelopmentClient, message: dict) -> None:
        app.send(WebSocket.TEXT, json.dumps({"type": "debug", "message": message}).encode())

    def from_app(self, client: DevelopmentClient, message: object) -> None:
        """Hands a message of the app to the editor, except the answers to the requests the relay sent again and the event that their initialization sends."""
        if not isinstance(message, dict):
            return
        with self.lock:
            if client is not self.app or self.editor is None:
                return
            if message.get("type") == "response" and message.get("request_seq", 0) > DebugRelay.REPLAY_SEQUENCE:
                return
            if message.get("type") == "event" and message.get("event") == "initialized" and self.silenced > 0:
                self.silenced -= 1
                return
            editor = self.editor

        converted = DebugMessage.source_paths(message, self.to_editor)
        with contextlib.suppress(OSError):
            editor.sendall(DebugMessage.frame(converted))

    def app_connected(self, client: DevelopmentClient) -> None:
        """Makes the app that connected last the one the editor debugs, sends it the configuration of the editor again and then what the editor sent while no app was connected."""
        with self.lock:
            self.app = client
            if self.editor is None:
                return
            replay = [self.replay_request(request["command"], request.get("arguments")) for request in self.configuration.values()]
            self.silenced += sum(1 for request in replay if request["command"] == "initialize")
            pending, self.pending = self.pending, []
            for message in pending:
                self.remember(message)

        for message in [*replay, *pending]:
            self.send_app(client, message)

    def app_left(self, client: DevelopmentClient) -> None:
        with self.lock:
            if self.app is client:
                self.app = None
