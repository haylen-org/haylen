import argparse
from collections.abc import Mapping
from pathlib import Path

from haylen.cli.command import Command
from haylen.cli.command_session import CommandSession
from haylen.core.repository import Repository
from haylen.scaffold.plugin_scaffold import PluginScaffold


class PluginNewCommand(Command):
    PATH = ("plugin", "new")
    HELP = "Create a plugin from templates/plugin, with a native part for every platform that answers its method and sends its event from the first build."

    def configure(self, parser: argparse.ArgumentParser) -> None:
        parser.add_argument("folder", help="The folder of the new plugin, named after its id, which must not exist or be empty.")
        parser.add_argument("--id", help='The id of the plugin in "dash-case", the folder name by default.')

    def run(self, arguments: argparse.Namespace, session: CommandSession) -> Mapping[str, object]:
        folder = Path(arguments.folder).expanduser().resolve()
        manifest = PluginScaffold.create(folder, arguments.id or folder.name)
        session.terminal.success(f'Created the plugin "{manifest.id}" in `{Repository.shown(folder)}`.')
        session.terminal.info(f'Add it to an app with "haylen plugin add {Repository.shown(folder)} --app <app folder>".')
        return {"id": manifest.id, "folder": str(folder)}
