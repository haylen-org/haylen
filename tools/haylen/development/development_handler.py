import http.server
import urllib.parse

from haylen.development.development_server import DevelopmentServer


class DevelopmentHandler(http.server.BaseHTTPRequestHandler):
    """Serves the development path alone, for apps that run outside a browser, and the GPU captures that `haylen app capture` asks for."""

    def __init__(self, *arguments: object, development: DevelopmentServer, **keywords: object) -> None:
        # The base class answers the request inside its constructor, so the server it needs is set first.
        self.development = development
        super().__init__(*arguments, **keywords)

    def do_GET(self) -> None:
        if urllib.parse.urlsplit(self.path).path != DevelopmentServer.PATH:
            self.send_error(404, "The development server only serves its development path.")
            return
        self.development.accept(self)

    def do_POST(self) -> None:
        if urllib.parse.urlsplit(self.path).path != DevelopmentServer.GPU_CAPTURE_PATH:
            self.send_error(404, "The development server only serves its development path.")
            return
        self.development.answer_gpu_capture(self)

    def log_message(self, format: str, *arguments: object) -> None:
        pass
