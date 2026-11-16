import argparse
from collections.abc import Mapping

from haylen.cli.arguments import Arguments
from haylen.cli.command import Command
from haylen.cli.command_session import CommandSession
from haylen.plugins.app_plugins import AppPlugins


class PluginUpdateCommand(Command):
    PATH = ("plugin", "update")
    HELP = 'Pin a plugin of an app that comes from a git repository to the commit that its branch or tag names now, in "haylen.lock", which every later build takes.'

    def configure(self, parser: argparse.ArgumentParser) -> None:
        parser.add_argument("id", help="The id of the plugin.")
        parser.add_argument("--app", default=".", help="The app folder, the current folder by default.")

    def run(self, arguments: argparse.Namespace, session: CommandSession) -> Mapping[str, object]:
        folder = Arguments.app_folder(arguments)
        commit = AppPlugins(session.context, folder).update(arguments.id)
        session.terminal.success(f'Pinned the plugin "{arguments.id}" to the commit "{commit}".')
        return {"id": arguments.id, "commit": commit}
