import argparse
from collections.abc import Mapping

from haylen.cli.arguments import Arguments
from haylen.cli.command import Command
from haylen.cli.command_session import CommandSession
from haylen.development.package_server import PackageServer
from haylen.development.server_options import ServerOptions


class AppServeCommand(Command):
    PATH = ("app", "serve")
    HELP = 'Serve the package of a Lua app and its development server on the local network, for a player on another device that downloads the package and plays it with hot reload, until the key "q" or Ctrl+C stops it.'

    def configure(self, parser: argparse.ArgumentParser) -> None:
        Arguments.add_app(parser)
        ServerOptions.add_address(
            parser,
            "Address that the server listens on, every address of this machine by default, while the address of the zip names the address of this machine on the local network.",
            "Port of the server, which serves the zip of the package and the development server,",
        )

    def run(self, arguments: argparse.Namespace, session: CommandSession) -> Mapping[str, object]:
        folder = Arguments.app_folder(arguments)
        return PackageServer(session.context, folder, arguments.host, arguments.port).serve()
