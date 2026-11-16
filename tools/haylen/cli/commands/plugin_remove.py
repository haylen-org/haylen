import argparse
from collections.abc import Mapping

from haylen.cli.arguments import Arguments
from haylen.cli.command import Command
from haylen.cli.command_session import CommandSession
from haylen.core.repository import Repository
from haylen.plugins.app_plugins import AppPlugins


class PluginRemoveCommand(Command):
    PATH = ("plugin", "remove")
    HELP = 'Remove a plugin from "haylen.json" and "haylen.lock" of an app, with its copy in "plugins", while a folder or a repository that the app references stays as it is.'

    def configure(self, parser: argparse.ArgumentParser) -> None:
        parser.add_argument("id", help="The id of the plugin.")
        parser.add_argument("--app", default=".", help="The app folder, the current folder by default.")

    def run(self, arguments: argparse.Namespace, session: CommandSession) -> Mapping[str, object]:
        folder = Arguments.app_folder(arguments)
        AppPlugins(session.context, folder).remove(arguments.id)
        session.terminal.success(f'Removed the plugin "{arguments.id}" from `{Repository.shown(folder)}`.')
        return {"id": arguments.id}
