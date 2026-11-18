import contextlib
import hashlib
import http.server
import json
import os
import threading
import time
import urllib.parse
import zipfile
from collections.abc import Iterable, Mapping
from pathlib import Path
from typing import BinaryIO

from haylen.assets.app_shaders import AppShaders
from haylen.core.app_manifest import AppManifest
from haylen.core.configuration import Configuration
from haylen.core.errors.development_failed import DevelopmentFailed
from haylen.core.errors.haylen_error import HaylenError
from haylen.core.package_layout import PackageLayout
from haylen.core.repository import Repository
from haylen.core.tooling_context import ToolingContext
from haylen.development.debug_relay import DebugRelay
from haylen.development.development_client import DevelopmentClient
from haylen.development.development_record import DevelopmentRecord
from haylen.development.websocket import WebSocket
from haylen.plugins.plugin_lock import PluginLock
from haylen.plugins.plugin_resolver import PluginResolver
from haylen.plugins.plugin_source import PluginSource
from haylen.sdk.selected_sdk import SelectedSdk


class DevelopmentServer:
    """Watches the package of an app in development and pushes every file the developer saves, in the app folder and in the folders of its plugins, to every connected app, as a JSON message of the batch followed by the bytes of each file. A connection needs the token of the run, the shaders compile again as their sources change, and an app that connects late catches up: a fresh app gets the files that changed since the package it loaded was made, an app of an earlier run of the server gets every file, and an app that only lost its connection gets what changed since the package was made again. The server carries the developer console, the GPU captures and the debugger between the terminal, the editor and the apps."""

    PATH = "/haylen/development"
    # The path that asks the connected apps for a GPU capture, which `haylen app capture` reaches from another terminal through the record of the run.
    GPU_CAPTURE_PATH = "/haylen/development/gpu-capture"
    # How long the apps take to answer a line of the developer console, and to end a GPU capture once they started it, since a capture tool takes seconds to write a capture of many frames.
    CONSOLE_ANSWER_SECONDS = 10
    GPU_CAPTURE_SECONDS = 120
    # The largest file the server pushes, which is the largest message the WebSocket of the engine accepts by default.
    MAX_FILE = 16 * 1024 * 1024
    SETTLE_SECONDS = 0.05
    WATCH_SECONDS = 0.5
    BACKUP_SUFFIXES = (".swp", ".swo", ".swx", ".tmp", ".bak", ".orig")

    def __init__(self, context: ToolingContext, layout: PackageLayout, manifest: Mapping[str, str], sdk: SelectedSdk | None = None) -> None:
        self.context = context
        # The SDK of the target of the app, whose shader tool of a port the shaders that the server compiles take, or none for a player that downloads the package.
        self.sdk = sdk
        self.terminal = context.terminal
        self.folder = layout.folder
        self.layout = layout
        self.layout_stamp = (self.folder / PackageLayout.MANIFEST).stat().st_mtime_ns
        self.layout_problem = ""
        self.manifest = dict(manifest)
        self.session = os.urandom(8).hex()
        # A token of hexadecimal digits, which a person also types on the remote of a TV.
        self.token = os.urandom(6).hex()
        self.revision = 0
        self.clients: list[DevelopmentClient] = []
        self.lock = threading.Lock()
        # The watcher and a reload that the developer asks for scan the package in turn, since a scan replaces the snapshot it compares with.
        self.scanning = threading.Lock()
        self.files = self.snapshot()
        self.lines = 0
        # The commands of the developer console of the apps, which the console of the terminal completes from and lists.
        self.commands: list[dict] = []
        self.debugger: DebugRelay | None = None
        # The answers that a GPU capture waits for: the results of its console line by the id of the line, and the capture reports by app while a capture waits for them.
        self.answers = threading.Condition()
        self.awaited: dict[int, list[tuple[DevelopmentClient, dict]]] = {}
        self.gpu_reports: dict[DevelopmentClient, dict] | None = None
        self.capturing = threading.Lock()
        self.record: Path | None = None
        self.stopped = threading.Event()

    @staticmethod
    def read_layout(context: ToolingContext, folder: Path) -> PackageLayout:
        """Reads the layout of the package of an app from its `haylen.json`, with the folder of every plugin it lists, wherever the plugin comes from."""
        app = AppManifest.load(folder)
        source = PluginSource(app.folder, PluginLock.load(app.folder), context.process, context.terminal)
        loaded = PluginResolver(app, source).load(Configuration.DEBUG)
        return PackageLayout.of(app.folder, {manifest.id: manifest.folder for _, manifest in loaded})

    @staticmethod
    def hash_files(files: Mapping[str, Path]) -> dict[str, str]:
        """Hashes files by their package paths."""
        return {path: hashlib.sha256(file.read_bytes()).hexdigest() for path, file in files.items()}

    @staticmethod
    def folder_manifest(folder: Path) -> dict[str, str]:
        """Hashes the files of a package folder as a build ships it."""
        files = {path.relative_to(folder).as_posix(): path for path in sorted(folder.rglob("*")) if path.is_file()}
        return DevelopmentServer.hash_files(files)

    @staticmethod
    def archive_manifest(archive: Path) -> dict[str, str]:
        """Hashes the files of the zip of a package."""
        with zipfile.ZipFile(archive) as opened:
            return {name: hashlib.sha256(opened.read(name)).hexdigest() for name in opened.namelist()}

    @staticmethod
    def watched(path: str) -> bool:
        """Tells whether a package path is one that hot reload applies, by the same rule as the engine: `haylen.json`, a file under `content`, a Lua module under `source`, and the manifest and the Lua modules of a plugin, none of them hidden or a backup."""
        parts = path.split("/")
        if any(part.startswith((".", "#")) or part.endswith("~") for part in parts) or path.endswith(DevelopmentServer.BACKUP_SUFFIXES):
            return False
        if path == PackageLayout.MANIFEST or (parts[0] == "content" and len(parts) > 1):
            return True
        if parts[0] == "source":
            return path.endswith(".lua")
        if len(parts) < 3 or parts[0] != "plugins":
            return False
        return parts[2:] == [PackageLayout.PLUGIN_MANIFEST] or (parts[2] == "source" and path.endswith(".lua"))

    @staticmethod
    def quoted(names: Iterable[str]) -> str:
        return ", ".join(f'"{name}"' for name in names)

    def start(self, host: str, port: int, debugger_port: int | None) -> None:
        """Starts what the server needs besides its connections: the debugger relay on its port, when the run offers one, the record of the run and the watcher of the package. A debugger port that something else uses stops the start before anything else runs."""
        if debugger_port is not None:
            self.debugger = DebugRelay(self.layout, debugger_port, self.terminal)
            self.debugger.start()
        self.record = DevelopmentRecord.write(self.folder, host, port, self.token)
        threading.Thread(target=self.watch, daemon=True).start()

    def close(self) -> None:
        self.stopped.set()
        if self.debugger is not None:
            self.debugger.close()
        if self.record is not None:
            self.record.unlink(missing_ok=True)

    def refresh_layout(self) -> None:
        """Reads the layout of the package again once `haylen.json` changed, so the plugins that the app lists while it runs are watched too, wherever they come from. While the plugins of `haylen.json` have a problem, which the terminal shows once, the server keeps watching the layout before it, as it does while an editor that saves `haylen.json` holds no file there."""
        try:
            stamp = (self.folder / PackageLayout.MANIFEST).stat().st_mtime_ns
        except FileNotFoundError:
            return
        if stamp == self.layout_stamp:
            return

        self.layout_stamp = stamp
        try:
            layout = DevelopmentServer.read_layout(self.context, self.folder)
        except HaylenError as error:
            if error.message != self.layout_problem:
                self.terminal.error(error.message, error.details)
            self.layout_problem = error.message
            return

        self.layout_problem = ""
        self.layout = layout
        if self.debugger is not None:
            self.debugger.layout = layout

    def snapshot(self) -> dict[str, tuple[int, int]]:
        """Records the time and size of every watched file of the package. A file that goes away while the package is listed, such as the scratch file of an editor, is no file of the snapshot."""
        found: dict[str, tuple[int, int]] = {}
        for relative, path in self.layout.files().items():
            if not DevelopmentServer.watched(relative):
                continue
            with contextlib.suppress(FileNotFoundError):
                status = path.stat()
                found[relative] = (status.st_mtime_ns, status.st_size)
        return found

    def scan(self) -> tuple[list[str], list[str]]:
        """Returns the files that changed and the files that went away since the last scan, leaving the files that are still being written for the next scan."""
        self.refresh_layout()
        current = self.snapshot()
        changed = sorted(path for path, stamp in current.items() if self.files.get(path) != stamp)
        removed = sorted(path for path in self.files if path not in current)

        if changed:
            time.sleep(DevelopmentServer.SETTLE_SECONDS)
            settled = self.snapshot()
            for path in [path for path in changed if settled.get(path) != current[path]]:
                changed.remove(path)
                if path in self.files:
                    current[path] = self.files[path]
                else:
                    del current[path]

        self.files = current
        return changed, removed

    def batch(self, changed: list[str], removed: list[str]) -> tuple[dict, list[bytes]]:
        """Reads the files of a batch, leaving out the files that are larger than the engine accepts, which the developer then ships with the build, and the files that went away since the scan, which the next scan names as removed."""
        files: list[dict] = []
        contents: list[bytes] = []
        for path in changed:
            disk = self.layout.to_disk(path)
            try:
                content = disk.read_bytes()
            except FileNotFoundError:
                continue
            if len(content) > DevelopmentServer.MAX_FILE:
                self.terminal.error(
                    f"The file `{Repository.shown(disk)}` is larger than the {DevelopmentServer.MAX_FILE} bytes that the development server pushes, so the running app keeps its old version until the app runs again."
                )
                continue
            files.append({"path": path, "size": len(content)})
            contents.append(content)
        return {"type": "files", "session": self.session, "revision": self.revision, "files": files, "removed": removed}, contents

    def catch_up(self, hello: Mapping[str, object]) -> tuple[dict, list[bytes]] | None:
        """Returns the batch that brings an app that just connected up to date, or nothing when it is up to date."""
        if hello.get("session") == self.session and hello.get("revision") == self.revision:
            return None
        current = DevelopmentServer.hash_files({path: self.layout.to_disk(path) for path in self.files})
        if hello.get("session") not in ("", self.session):
            return self.batch(sorted(current), [])

        changed = sorted(path for path, digest in current.items() if self.manifest.get(path) != digest)
        removed = sorted(path for path in self.manifest if path not in current and DevelopmentServer.watched(path))
        return self.batch(changed, removed)

    def broadcast(self, changed: list[str], removed: list[str]) -> None:
        with self.lock:
            self.revision += 1
            message, contents = self.batch(changed, removed)
            clients = list(self.clients)

        for client in clients:
            if not client.send_batch(message, contents):
                self.drop(client)
        apps = "app" if len(clients) == 1 else "apps"
        self.terminal.info(f"Sent {DevelopmentServer.quoted([*changed, *removed])} to {len(clients)} connected {apps}.")

    def drop(self, client: DevelopmentClient) -> None:
        with self.lock:
            if client in self.clients:
                self.clients.remove(client)
        if self.debugger is not None:
            self.debugger.app_left(client)

    def watch(self) -> None:
        """Scans the package twice a second until the server closes, compiles the shaders whose sources changed, which the scan then pushes as compiled files, and reports their errors to the apps too."""
        shaders = self.folder / "content" / "shaders"
        seen = AppShaders.newest_input(self.folder) if shaders.is_dir() else 0.0
        while not self.stopped.wait(DevelopmentServer.WATCH_SECONDS):
            stamp = AppShaders.newest_input(self.folder) if shaders.is_dir() else 0.0
            if stamp != seen:
                seen = stamp
                self.compile_shaders()
            with self.scanning:
                changed, removed = self.scan()
                if changed or removed:
                    self.broadcast(changed, removed)

    def compile_shaders(self) -> None:
        try:
            AppShaders(self.context, self.sdk).update(self.folder)
        except HaylenError as error:
            self.terminal.error(error.message, error.details)
            message = {"type": "shaderError", "path": "content/shaders", "message": f"{error.message} {error.details}".strip()}
            self.send_all(json.dumps(message).encode())

    def reload(self) -> None:
        """Scans the package at once and sends what changed, for a save that the watcher has not seen yet."""
        with self.scanning:
            changed, removed = self.scan()
            if changed or removed:
                self.broadcast(changed, removed)
                return
        self.terminal.info("Nothing changed since the last files the apps received.")

    def restart(self) -> None:
        """Asks every connected app to start again from its package with a fresh Lua state."""
        if self.send_all(json.dumps({"type": "restart"}).encode()) == 0:
            self.terminal.warning("No app is connected to the development server, so no app restarted.")
            return
        self.terminal.info("Restarting the app.")

    def report(self, client: DevelopmentClient, message: Mapping[str, object]) -> None:
        """Prints what a batch did in an app, or the GPU capture that it ended, which a capture that waits for the app receives too."""
        if "gpuCapture" in message:
            capture = message["gpuCapture"] if isinstance(message["gpuCapture"], dict) else {}
            if capture.get("ok"):
                self.terminal.success(DevelopmentServer.capture_written(capture))
            else:
                self.terminal.error(f"The GPU capture of the app failed: {capture.get('error', '')}")
            with self.answers:
                if self.gpu_reports is not None:
                    self.gpu_reports[client] = capture
                    self.answers.notify_all()
            return

        if message.get("restarted"):
            self.terminal.info(f"The app restarts: {message.get('reason', '')}")
        elif message.get("modules"):
            resumed = " and resumed" if message.get("resumed") else ""
            self.terminal.success(f"The app reloaded {DevelopmentServer.quoted(message['modules'])}{resumed} in {round(message.get('milliseconds', 0))} ms.")
        elif message.get("restored"):
            resumed = ", and the app resumed" if message.get("resumed") else ""
            self.terminal.success(f"The files {DevelopmentServer.quoted(message['restored'])} hold the code that runs again{resumed}.")
        elif message.get("assets"):
            self.terminal.success(f"The app reloaded the assets {DevelopmentServer.quoted(message['assets'])}.")

    @staticmethod
    def capture_written(capture: Mapping[str, object]) -> str:
        frames = capture.get("frames", 1)
        noun = "frame" if frames == 1 else "frames"
        return f'The app wrote the GPU capture of {frames} {noun} with "{capture.get("tool", "")}" to `{capture.get("path", "")}`.'

    def accept(self, handler: http.server.BaseHTTPRequestHandler) -> None:
        """Turns a request of the development path into a WebSocket and serves it until the app leaves. A request without the token of the run is refused."""
        query = urllib.parse.parse_qs(urllib.parse.urlsplit(handler.path).query)
        key = handler.headers.get("Sec-WebSocket-Key")
        if query.get("token") != [self.token] or key is None or handler.headers.get("Upgrade", "").lower() != "websocket":
            handler.send_error(403, "The development server needs the token of this run.")
            return

        # Browsers accept the upgrade only in an HTTP/1.1 status line, whatever version the server answers other requests with.
        upgrade = f"HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: {WebSocket.accept(key)}\r\n\r\n"
        handler.wfile.write(upgrade.encode())
        handler.wfile.flush()
        handler.close_connection = True
        client = DevelopmentClient(handler.wfile)
        with contextlib.suppress(OSError):
            self.serve(client, handler.rfile)
        self.drop(client)
        self.terminal.info("An app left the development server.")

    def serve(self, client: DevelopmentClient, stream: BinaryIO) -> None:
        """Answers the frames of an app until it closes the connection or the connection ends, such as when the app stops or restarts."""
        message = b""
        while (frame := WebSocket.read_frame(stream)) is not None:
            final, opcode, payload = frame
            if opcode == WebSocket.CLOSE:
                client.send(WebSocket.CLOSE, payload[:2])
                break
            if opcode == WebSocket.PING:
                client.send(WebSocket.PONG, payload)
                continue

            message += payload
            if not final:
                continue
            text, message = message, b""
            try:
                received = json.loads(text)
            except ValueError:
                self.terminal.warning("An app sent the development server a message that is not JSON, which the server skips.")
                continue
            self.receive(client, received)

    def send_all(self, message: bytes) -> int:
        """Sends a text message to every connected app and returns how many it reached, dropping the apps whose connection ended."""
        with self.lock:
            clients = list(self.clients)
        reached = 0
        for client in clients:
            if client.send(WebSocket.TEXT, message):
                reached += 1
            else:
                self.drop(client)
        return reached

    def send_line(self, line: str) -> int:
        """Sends a line of the developer console to every connected app, which runs it and answers with its result, and returns how many apps it reached."""
        with self.lock:
            self.lines += 1
            message = json.dumps({"type": "console", "id": self.lines, "line": line}).encode()
        reached = self.send_all(message)
        if reached == 0:
            self.terminal.warning("No app is connected to the development server, so the line ran nowhere.")
        return reached

    def request_commands(self) -> None:
        """Asks every connected app for the commands of its developer console, which it answers with a message of their names, how to type them, their help and the kinds of their arguments."""
        self.send_all(json.dumps({"type": "consoleCommands"}).encode())

    def capture_gpu(self, frames: int) -> list[dict]:
        """Asks every connected app for a GPU capture of its next frames through its developer console, and returns what each app answered: `{"ok": True, "path", "tool", "frames"}` once its capture ended, or `{"ok": False, "error"}`. Each wait has its bound, and an app that does not answer within it counts as failed."""
        if not self.capturing.acquire(blocking=False):
            raise DevelopmentFailed("Another GPU capture of the apps is running. Capture again once it ends.")
        try:
            with self.lock:
                self.lines += 1
                number = self.lines
            with self.answers:
                self.awaited[number] = []
                self.gpu_reports = {}

            line = json.dumps({"type": "console", "id": number, "line": f"gpuCapture {frames}"}).encode()
            reached = self.send_all(line)
            with self.answers:
                self.answers.wait_for(lambda: len(self.awaited[number]) >= reached, timeout=DevelopmentServer.CONSOLE_ANSWER_SECONDS)
                answers = self.awaited.pop(number)
                started = [client for client, answer in answers if answer.get("ok")]
                self.answers.wait_for(lambda: all(client in self.gpu_reports for client in started), timeout=DevelopmentServer.GPU_CAPTURE_SECONDS)
                reports, self.gpu_reports = self.gpu_reports, None
        finally:
            self.capturing.release()

        results = [{"ok": False, "error": str(answer.get("output", ""))} for _, answer in answers if not answer.get("ok")]
        silent = {"ok": False, "error": f"The app did not answer the developer console within {DevelopmentServer.CONSOLE_ANSWER_SECONDS} seconds."}
        results += [silent] * (reached - len(answers))
        unfinished = {"ok": False, "error": f"The app did not end its GPU capture within {DevelopmentServer.GPU_CAPTURE_SECONDS} seconds."}
        results += [reports.get(client, unfinished) for client in started]
        return results

    def answer_gpu_capture(self, handler: http.server.BaseHTTPRequestHandler) -> None:
        """Answers the request of `haylen app capture` with what every connected app captured, as JSON. A request without the token of the run is refused."""
        query = urllib.parse.parse_qs(urllib.parse.urlsplit(handler.path).query)
        if query.get("token") != [self.token]:
            DevelopmentServer.answer_json(handler, 403, {"error": "The development server needs the token of this run."})
            return

        try:
            request = json.loads(handler.rfile.read(int(handler.headers.get("Content-Length", 0))) or b"{}")
        except ValueError:
            request = None
        frames = request.get("frames", 1) if isinstance(request, dict) else None
        if not isinstance(frames, int) or isinstance(frames, bool) or frames < 1:
            DevelopmentServer.answer_json(handler, 400, {"error": "A GPU capture records a whole number of frames, at least 1."})
            return

        try:
            results = self.capture_gpu(frames)
        except DevelopmentFailed as error:
            DevelopmentServer.answer_json(handler, 409, {"error": error.message})
            return
        DevelopmentServer.answer_json(handler, 200, {"results": results})

    @staticmethod
    def answer_json(handler: http.server.BaseHTTPRequestHandler, status: int, value: Mapping[str, object]) -> None:
        body = json.dumps(value).encode()
        handler.send_response(status)
        handler.send_header("Content-Type", "application/json")
        handler.send_header("Content-Length", str(len(body)))
        handler.end_headers()
        handler.wfile.write(body)

    def receive(self, client: DevelopmentClient, message: Mapping[str, object]) -> None:
        kind = message.get("type")
        if kind == "debug":
            if self.debugger is not None:
                self.debugger.from_app(client, message.get("message"))
            return
        if kind == "report":
            self.report(client, message)
            return
        if kind == "consoleResult":
            self.console_result(client, message)
            return
        if kind == "consoleCommands":
            commands = message.get("commands")
            if isinstance(commands, list):
                self.commands = [command for command in commands if DevelopmentServer.is_command(command)]
            return
        if kind == "hello":
            self.welcome(client, message)

    @staticmethod
    def is_command(command: object) -> bool:
        """Tells whether a command that an app sent has its name, how to type it, its help and the kinds of its arguments."""
        if not isinstance(command, dict):
            return False
        return all(isinstance(command.get(key), str) for key in ("name", "usage", "help")) and isinstance(command.get("arguments"), list)

    def console_result(self, client: DevelopmentClient, message: Mapping[str, object]) -> None:
        output = str(message.get("output", ""))
        if not message.get("ok"):
            self.terminal.error(self.terminal.fit(output or "The line failed.", len("Error: ")))
        elif output:
            self.terminal.info(self.terminal.fit(output))
        with self.answers:
            if message.get("id") in self.awaited:
                self.awaited[message["id"]].append((client, dict(message)))
                self.answers.notify_all()

    def welcome(self, client: DevelopmentClient, hello: Mapping[str, object]) -> None:
        """Takes an app that said hello into the connected apps, brings it up to date and lets the editor debug it."""
        with self.lock:
            update = self.catch_up(hello)
            self.clients.append(client)
        self.terminal.info("An app connected to the development server.")

        if update is not None and (update[0]["files"] or update[0]["removed"]):
            client.send_batch(*update)
        if self.debugger is not None:
            self.debugger.app_connected(client)
