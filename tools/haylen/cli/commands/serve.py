import argparse
import webbrowser
from collections.abc import Mapping
from pathlib import Path

from haylen.cli.command import Command
from haylen.cli.command_session import CommandSession
from haylen.core.errors.development_failed import DevelopmentFailed
from haylen.core.repository import Repository
from haylen.development.server_options import ServerOptions
from haylen.development.site_server import SiteServer


class ServeCommand(Command):
    PATH = ("serve",)
    HELP = "Serve a folder with the headers that WebAssembly pages need, until Ctrl+C stops the server."

    def configure(self, parser: argparse.ArgumentParser) -> None:
        parser.add_argument("folder", help="The folder of the web page.")
        ServerOptions.add_address(
            parser,
            'Address that the web server listens on. The address of this machine on the local network lets phones and other computers open the page, which then runs without sound, because browsers offer "AudioWorklet" only to pages served over https or from "localhost".',
            "Port of the web server,",
        )
        ServerOptions.add_page(parser)

    def run(self, arguments: argparse.Namespace, session: CommandSession) -> Mapping[str, object]:
        folder = Path(arguments.folder).expanduser().resolve()
        if not folder.is_dir():
            raise DevelopmentFailed(f"The folder `{Repository.shown(folder)}` does not exist. Name the folder of a web page.")

        with SiteServer(folder, arguments.host, arguments.port, arguments.coep, arguments.coop) as server:
            session.terminal.success(f"Serving `{Repository.shown(folder)}` at `{server.url}`")
            session.terminal.info("Press Ctrl+C to stop the server.")
            if arguments.open:
                webbrowser.open(server.url)
            server.serve()
        return {"folder": str(folder), "url": server.url}
