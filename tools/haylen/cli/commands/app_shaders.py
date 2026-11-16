import argparse
from collections.abc import Mapping

from haylen.assets.app_shaders import AppShaders
from haylen.cli.arguments import Arguments
from haylen.cli.command import Command
from haylen.cli.command_session import CommandSession
from haylen.core.repository import Repository


class AppShadersCommand(Command):
    PATH = ("app", "shaders")
    HELP = 'Compile the shaders under "content/shaders" of an app into ".shader" packages for every shader language of the engine, the ones that are out of date or every one with "--force".'

    def configure(self, parser: argparse.ArgumentParser) -> None:
        Arguments.add_app(parser)
        parser.add_argument("--force", action="store_true", help="Compile every shader, including the ones that are up to date.")

    def run(self, arguments: argparse.Namespace, session: CommandSession) -> Mapping[str, object]:
        folder = Arguments.app_folder(arguments)
        sources = AppShaders.sources(folder)
        compiled = AppShaders(session.context).update(folder, arguments.force)
        if not sources:
            session.terminal.info(f"The app `{Repository.shown(folder)}` has no shaders under `{Repository.shown(folder / 'content' / 'shaders')}`.")
        elif not compiled:
            session.terminal.success(f"The {len(sources)} shaders of `{Repository.shown(folder)}` are up to date.")
        return {"sources": [str(path) for path in sources], "compiled": [str(path) for path in compiled]}
