import argparse
from collections.abc import Mapping
from pathlib import Path

from haylen.cli.command import Command
from haylen.cli.command_session import CommandSession
from haylen.core.app_category import AppCategory
from haylen.core.app_kind import AppKind
from haylen.core.repository import Repository
from haylen.scaffold.app_scaffold import AppScaffold


class NewCommand(Command):
    PATH = ("new",)
    HELP = 'Create an app with its "haylen.json", its starter code and the bridge handlers of every platform. The app holds no platform project: every build generates it.'

    def configure(self, parser: argparse.ArgumentParser) -> None:
        parser.add_argument("folder", help="The folder of the new app, which must not exist or be empty.")
        parser.add_argument(
            "--kind",
            default=AppKind.LUA.value,
            choices=[kind.value for kind in AppKind],
            help='The kind of the app: "lua", whose code is Lua, or "cpp", whose C++ code compiles against the engine SDK. "lua" by default.',
        )
        parser.add_argument(
            "--category",
            choices=[category.value for category in AppCategory],
            help="The category that the stores and the app menus show the app in. An app without one leaves it out of every generated project.",
        )
        parser.add_argument("--name", help="The display name, from the folder name by default.")
        parser.add_argument("--identifier", help='The reverse domain identifier, "com.example.<folder>" by default.')
        parser.add_argument("--orientation", default="landscape", choices=["landscape", "portrait", "any"], help='The orientation of the app, "landscape" by default.')

    def run(self, arguments: argparse.Namespace, session: CommandSession) -> Mapping[str, object]:
        folder = Path(arguments.folder).expanduser().resolve()
        kind = AppKind(arguments.kind)
        manifest = AppScaffold.create(folder, kind, AppCategory(arguments.category) if arguments.category else None, arguments.name, arguments.identifier, arguments.orientation)
        session.terminal.success(f'Created the {AppScaffold.KIND_WORDS[kind]} app "{manifest.name}" with the identifier "{manifest.identifier}" in `{Repository.shown(folder)}`.')
        session.terminal.info(f'Run it with "haylen app run {Repository.shown(folder)}".')
        return {"folder": str(folder), "name": manifest.name, "identifier": manifest.identifier, "kind": kind.value, "category": manifest.category.value if manifest.category is not None else None}
