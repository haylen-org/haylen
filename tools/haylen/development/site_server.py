import contextlib
import functools
from pathlib import Path

from haylen.core.errors.development_failed import DevelopmentFailed
from haylen.development.development_server import DevelopmentServer
from haylen.development.local_server import LocalServer
from haylen.development.web_handler import WebHandler


class SiteServer:
    """Serves a folder of this machine as a web site with the headers that WebAssembly pages need, through `WebHandler`, and with a development server on the same port when a run plays the site in development. It listens as soon as it is made, so a port that something else uses fails before anything else starts."""

    def __init__(self, folder: Path, host: str, port: int, coep: str = "off", coop: str = "off", development: DevelopmentServer | None = None) -> None:
        self.folder = folder
        self.host = host
        handler = functools.partial(WebHandler, directory=str(folder), coep=coep, coop=coop, development=development)
        try:
            self.server = LocalServer((host, port), handler)
        except OSError as error:
            raise DevelopmentFailed(f'The web server cannot listen on port {port} of "{host}": {error.strerror}. Stop what uses the port, or pick another one with "--port".') from error

    def __enter__(self) -> SiteServer:
        return self

    def __exit__(self, *_: object) -> None:
        self.close()

    @property
    def port(self) -> int:
        return self.server.server_address[1]

    @property
    def url(self) -> str:
        return f"http://{self.host}:{self.port}/"

    def serve(self) -> None:
        """Serves until Ctrl+C or a call of `shutdown` from another thread stops the server."""
        with contextlib.suppress(KeyboardInterrupt):
            self.server.serve_forever()

    def shutdown(self) -> None:
        self.server.shutdown()

    def close(self) -> None:
        self.server.server_close()
