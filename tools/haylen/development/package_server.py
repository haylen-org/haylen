import contextlib
import functools
from pathlib import Path

from haylen.assets.app_shaders import AppShaders
from haylen.core.app_kind import AppKind
from haylen.core.app_manifest import AppManifest
from haylen.core.errors.development_failed import DevelopmentFailed
from haylen.core.repository import Repository
from haylen.core.tooling_context import ToolingContext
from haylen.development.development_address import DevelopmentAddress
from haylen.development.development_reach import DevelopmentReach
from haylen.development.development_server import DevelopmentServer
from haylen.development.local_server import LocalServer
from haylen.development.package_handler import PackageHandler
from haylen.development.run_session import RunSession
from haylen.platforms.product_folders import ProductFolders


class PackageServer:
    """Serves the zip of the package of a Lua app and its development server on one port of the local network, both behind the token of the run, for a player on another device that downloads the package and plays it in development with the option `developmentServer` of `haylen.launch`. The manifest of the development server holds the files of the zip, so a player that downloaded it catches up on what changed since."""

    def __init__(self, context: ToolingContext, folder: Path, host: str, port: int) -> None:
        self.context = context
        self.folder = folder
        self.host = host
        self.port = port

    def serve(self) -> dict[str, str]:
        """Packages the app, prints the address of its zip with the keys of the run, and serves until the key "q" or Ctrl+C stops it. It returns the address of the zip."""
        app = AppManifest.load(self.folder)
        if app.kind is AppKind.CPP:
            shown = Repository.shown(self.folder)
            raise DevelopmentFailed(f'The app `{shown}` is a C++ app, which has no package to serve. Run it with "haylen app run {shown}".')

        AppShaders(self.context).update(app.folder)
        layout = DevelopmentServer.read_layout(self.context, app.folder)
        archive = ProductFolders.app_folder(app.folder) / "served" / "app.zip"
        layout.zip(archive)
        development = DevelopmentServer(self.context, layout, DevelopmentServer.archive_manifest(archive))

        address = DevelopmentAddress.of(DevelopmentReach.NETWORK, self.host)
        handler = functools.partial(PackageHandler, development=development, archive=archive, development_address=address.websocket(self.port, development.token))
        try:
            server = LocalServer((address.listen, self.port), handler)
        except OSError as error:
            raise DevelopmentFailed(f'The server cannot listen on port {self.port} of "{address.listen}": {error.strerror}. Stop what uses the port, or pick another one with "--port".') from error

        served = f"http://{address.reached}:{self.port}/haylen/{development.token}/app.zip"
        rows = [("App", f'{app.name} ("{app.identifier}")'), ("Address", f"`{served}`"), ("Hot reload", "Every file saved in the app reloads in the players that play it")]
        # The session follows no process, so it only gives the terminal back when it ends.
        session = RunSession(self.context, development)
        try:
            development.start(address.listen, self.port, None)
            session.show(rows, server.shutdown)
            with contextlib.suppress(KeyboardInterrupt):
                server.serve_forever()
        finally:
            session.end(failed=False)
            development.close()
            server.server_close()
        return {"address": served}
