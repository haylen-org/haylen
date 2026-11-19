import http.server
import json
import re
import urllib.parse
from pathlib import Path
from typing import BinaryIO

from haylen.development.development_server import DevelopmentServer
from haylen.development.file_range import FileRange


class WebHandler(http.server.SimpleHTTPRequestHandler):
    """Serves a folder with the headers WebAssembly pages need: explicit media types, no caching, precompressed files and byte ranges. The web runtime is single-threaded, so the page stays in one browsing context group with the popups it opens, such as sign-in and payment pages, unless a page asks for cross-origin isolation with the opener policy `same-origin`. With a development server, the handler also serves its development path and adds where it is to the `config.json` of the site."""

    CONFIG = "/config.json"

    MEDIA_TYPES = {
        ".wasm": "application/wasm",
        ".js": "text/javascript",
        ".mjs": "text/javascript",
        ".json": "application/json",
        ".zip": "application/zip",
        ".html": "text/html",
        ".css": "text/css",
        ".svg": "image/svg+xml",
        ".png": "image/png",
        ".data": "application/octet-stream",
    }
    ENCODINGS = ((".br", "br"), (".gz", "gzip"))
    RANGE = re.compile(r"bytes=(\d+)-(\d*)")

    def __init__(self, *arguments: object, directory: str, coep: str, coop: str, development: DevelopmentServer | None, **keywords: object) -> None:
        # The base class answers the request inside its constructor, so the headers and the server it needs are set first.
        self.coep = coep
        self.coop = coop
        self.development = development
        super().__init__(*arguments, directory=directory, **keywords)

    def do_GET(self) -> None:
        path = urllib.parse.urlsplit(self.path).path
        if self.development is not None and path == DevelopmentServer.PATH:
            self.development.accept(self)
            return
        if self.development is not None and path == WebHandler.CONFIG:
            self.send_development_config()
            return
        super().do_GET()

    def send_development_config(self) -> None:
        """Answers the `config.json` of the site with the path and the token of the development server, which the page connects to, so the site of the build stays the same in Debug and Release and nothing of the run stays in it."""
        config = json.loads(Path(self.translate_path(WebHandler.CONFIG)).read_text(encoding="utf-8"))
        config["development"] = {"path": DevelopmentServer.PATH, "token": self.development.token}
        body = (json.dumps(config, indent=4) + "\n").encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_POST(self) -> None:
        if self.development is None or urllib.parse.urlsplit(self.path).path != DevelopmentServer.GPU_CAPTURE_PATH:
            self.send_error(404, "The server takes no requests at this path.")
            return
        self.development.answer_gpu_capture(self)

    def end_headers(self) -> None:
        if self.coop != "off":
            self.send_header("Cross-Origin-Opener-Policy", self.coop)
        if self.coep != "off":
            self.send_header("Cross-Origin-Embedder-Policy", self.coep)
        self.send_header("Cross-Origin-Resource-Policy", "cross-origin")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Cache-Control", "no-store, no-cache, must-revalidate")
        super().end_headers()

    def guess_type(self, path: str) -> str:
        return WebHandler.MEDIA_TYPES.get(Path(path).suffix.lower()) or super().guess_type(path)

    def send_head(self) -> BinaryIO | FileRange | None:
        path = Path(self.translate_path(self.path))
        if self.path.endswith("/") and path.is_dir() and (path / "index.html").is_file():
            path = path / "index.html"

        # A request for a file that also exists as `.br` or `.gz` is answered with the compressed copy that the browser accepts.
        accepted = self.headers.get("Accept-Encoding", "")
        for suffix, encoding in WebHandler.ENCODINGS:
            compressed = path.with_name(path.name + suffix)
            if encoding in accepted and path.suffix and compressed.is_file():
                stream = compressed.open("rb")
                self.send_response(200)
                self.send_header("Content-Type", self.guess_type(str(path)))
                self.send_header("Content-Encoding", encoding)
                self.send_header("Content-Length", str(compressed.stat().st_size))
                self.send_header("Vary", "Accept-Encoding")
                self.end_headers()
                return stream

        if not path.is_file():
            return super().send_head()
        return self.send_file(path)

    def send_file(self, path: Path) -> FileRange | None:
        """Answers a file whole or by one byte range, with a strong validator, so the protected content of a release streams from the site the way a content delivery network serves it, and a range whose `If-Range` names another validator gets the whole file."""
        status = path.stat()
        size = status.st_size
        validator = f'"{size:x}-{status.st_mtime_ns:x}"'
        first = 0
        last = size - 1
        requested = WebHandler.RANGE.fullmatch(self.headers.get("Range", "").strip())
        partial = requested is not None and self.headers.get("If-Range", validator) == validator
        if partial:
            first = int(requested.group(1))
            last = min(int(requested.group(2)) if requested.group(2) else size - 1, size - 1)
            if first >= size:
                self.send_response(416)
                self.send_header("Content-Range", f"bytes */{size}")
                self.send_header("Content-Length", "0")
                self.end_headers()
                return None

        length = max(0, last - first + 1)
        self.send_response(206 if partial else 200)
        self.send_header("Content-Type", self.guess_type(str(path)))
        self.send_header("Content-Length", str(length))
        self.send_header("ETag", validator)
        self.send_header("Accept-Ranges", "bytes")
        self.send_header("Access-Control-Expose-Headers", "Content-Range, Content-Length, ETag")
        if partial:
            self.send_header("Content-Range", f"bytes {first}-{last}/{size}")
        self.end_headers()
        return FileRange(path, first, length)

    def log_message(self, format: str, *arguments: object) -> None:
        pass
