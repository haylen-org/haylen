import json
import webbrowser
from pathlib import Path

from haylen.core.errors.development_failed import DevelopmentFailed
from haylen.core.repository import Repository
from haylen.core.tooling_context import ToolingContext
from haylen.development.development_server import DevelopmentServer
from haylen.development.run_session import RunSession
from haylen.development.server_options import ServerOptions
from haylen.development.site_server import SiteServer
from haylen.platforms.build_product import BuildProduct
from haylen.platforms.launch_request import LaunchRequest


class SiteRun:
    """The run of a built web site that the web backend serves from `PlatformBackend.run`. In development, the development server answers on the same port as the site, and the page connects to it, since entering the run writes the path and the token of the server into the `config.json` of the site. Its manifest holds the files of the `app.zip` of the site, so a page that loads late gets only what changed since. The headers of the site come from the options `coep` and `coop` of the launch, and the option `open` opens the page in the default browser.

    ```python
    with SiteRun(self.context, product, launch, product.path) as run:
        run.serve()
    ```
    """

    CONFIG = "config.json"
    ARCHIVE = "app.zip"

    def __init__(self, context: ToolingContext, product: BuildProduct, launch: LaunchRequest, site: Path) -> None:
        self.context = context
        self.product = product
        self.launch = launch
        self.site = site
        self.development: DevelopmentServer | None = None
        self.server: SiteServer | None = None
        self.session = RunSession(context)

    def __enter__(self) -> SiteRun:
        options = self.launch.options
        coep = str(options.get(ServerOptions.COEP, ServerOptions.OFF))
        coop = str(options.get(ServerOptions.COOP, ServerOptions.OFF))
        if not self.launch.development:
            self.server = SiteServer(self.site, self.launch.host, self.launch.port, coep, coop)
            return self

        manifest = DevelopmentServer.archive_manifest(self.site / SiteRun.ARCHIVE)
        development = DevelopmentServer(self.context, self.product.prepared.product.package, manifest, self.product.prepared.product.primary.sdk)
        server = SiteServer(self.site, self.launch.host, self.launch.port, coep, coop, development)
        try:
            development.start(self.launch.host, server.port, self.launch.debugger_port)
        except DevelopmentFailed:
            server.close()
            raise

        config_path = self.site / SiteRun.CONFIG
        config = json.loads(config_path.read_text(encoding="utf-8"))
        config["development"] = {"path": DevelopmentServer.PATH, "token": development.token}
        config_path.write_text(json.dumps(config, indent=4) + "\n", encoding="utf-8")
        self.development = development
        self.server = server
        self.session = RunSession(self.context, development)
        return self

    def __exit__(self, failure: type[BaseException] | None, *_: object) -> None:
        self.session.end(failure is not None)
        if self.development is not None:
            self.development.close()
        if self.server is not None:
            self.server.close()

    def rows(self) -> list[tuple[str, str]]:
        resolved = self.product.prepared.product
        rows = [("App", f'{resolved.app.name} ("{resolved.app.identifier}")'), ("Page", f"`{self.server.url}`"), ("Site", f"`{Repository.shown(self.site)}`")]
        if self.development is not None:
            rows.append(("Hot reload", "Every file saved in the app reloads in the open pages"))
            rows.append(("Debugger", f"Attach an editor to port {self.development.debugger.port} of this machine, for log points, since a page cannot halt"))
        return rows

    def serve(self) -> None:
        """Prints the summary of the site and the keys, opens the page when the launch asks for it, and serves until the key "q" or Ctrl+C stops the server."""
        self.session.show(self.rows(), self.server.shutdown)
        if self.launch.options.get(ServerOptions.OPEN):
            webbrowser.open(self.server.url)
        self.server.serve()
