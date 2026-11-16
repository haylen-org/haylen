import threading
from pathlib import Path

from haylen.development.site_server import SiteServer


class ServedSite:
    """Serves a site on a free port of this machine from a thread of its own while the block runs, for the pages that the web checks and the GPU tests of the web open."""

    def __init__(self, folder: Path) -> None:
        self.server = SiteServer(folder, "127.0.0.1", 0)
        self.thread = threading.Thread(target=self.server.serve, daemon=True)

    def __enter__(self) -> ServedSite:
        self.thread.start()
        return self

    def __exit__(self, *_: object) -> None:
        self.server.shutdown()
        self.thread.join()
        self.server.close()

    def page(self, query: str = "", path: str = "") -> str:
        """The address of a page of the site, such as `http://127.0.0.1:<port>/gpu-tests.html?api=webGpu`."""
        return f"http://127.0.0.1:{self.server.port}/{path}{query}"
