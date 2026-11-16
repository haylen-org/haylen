import http.server
import re
import shutil
import urllib.parse
from pathlib import Path

from haylen.development.development_server import DevelopmentServer


class PackageHandler(http.server.BaseHTTPRequestHandler):
    """Serves the zip of the package of an app, with the address of the development server in its header, and the development path, both only with the token of the run."""

    # The zip of the package, with the token of the run in its path, so its address holds only letters, digits and the marks that every keyboard of a phone or a TV types on its first pages.
    ARCHIVE_PATH = re.compile(r"/haylen/([0-9a-f]+)/app\.zip")
    # The header of the zip that names the development server with its token, so a player plays the app with hot reload from the address of the zip alone.
    DEVELOPMENT_HEADER = "Haylen-Development-Server"

    def __init__(self, *arguments: object, development: DevelopmentServer, archive: Path, development_address: str, **keywords: object) -> None:
        # The base class answers the request inside its constructor, so what it serves is set first.
        self.development = development
        self.archive = archive
        self.development_address = development_address
        super().__init__(*arguments, **keywords)

    def do_GET(self) -> None:
        parts = urllib.parse.urlsplit(self.path)
        if parts.path == DevelopmentServer.PATH:
            self.development.accept(self)
            return
        requested = PackageHandler.ARCHIVE_PATH.fullmatch(parts.path)
        if requested is None:
            self.send_error(404, "The server only serves the zip of the app and its development path.")
            return
        if requested[1] != self.development.token:
            self.send_error(403, "The server needs the token of this run.")
            return

        self.send_response(200)
        self.send_header("Content-Type", "application/zip")
        self.send_header("Content-Length", str(self.archive.stat().st_size))
        self.send_header("Cache-Control", "no-store")
        self.send_header(PackageHandler.DEVELOPMENT_HEADER, self.development_address)
        # A player in a web page downloads from another origin, and reads the header only when the server exposes it.
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Expose-Headers", PackageHandler.DEVELOPMENT_HEADER)
        self.end_headers()
        with self.archive.open("rb") as stream:
            shutil.copyfileobj(stream, self.wfile)
        self.development.terminal.info("A player downloaded the app.")

    def do_POST(self) -> None:
        if urllib.parse.urlsplit(self.path).path != DevelopmentServer.GPU_CAPTURE_PATH:
            self.send_error(404, "The server only serves the zip of the app and its development path.")
            return
        self.development.answer_gpu_capture(self)

    def log_message(self, format: str, *arguments: object) -> None:
        pass
