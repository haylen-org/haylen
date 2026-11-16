import argparse
from collections.abc import Mapping

from haylen.cli.arguments import Arguments
from haylen.cli.command import Command
from haylen.cli.command_session import CommandSession
from haylen.core.repository import Repository
from haylen.plugins.app_plugins import AppPlugins


class PluginAddCommand(Command):
    PATH = ("plugin", "add")
    HELP = 'Copy a plugin folder or a folder of a git repository into "plugins" of an app, or reference it, and list it in "haylen.json". A referenced repository is pinned to its current commit in "haylen.lock".'

    def configure(self, parser: argparse.ArgumentParser) -> None:
        parser.add_argument("plugin", help='A plugin folder, or a git repository that holds a plugin at its root or in the folder that "--path" names, such as https://example.com/plugins.git.')
        parser.add_argument("--ref", help="The branch, tag or commit of the repository, its default branch otherwise.")
        parser.add_argument("--path", help="The plugin folder inside the repository, relative to its root, such as camera.")
        parser.add_argument("--reference", action="store_true", help='List the folder or the repository in "haylen.json" as where the plugin comes from, instead of copying it into the app.')
        parser.add_argument("--app", default=".", help="The app folder, the current folder by default.")

    def run(self, arguments: argparse.Namespace, session: CommandSession) -> Mapping[str, object]:
        folder = Arguments.app_folder(arguments)
        manifest = AppPlugins(session.context, folder).add(arguments.plugin, arguments.ref, arguments.path, arguments.reference)
        session.terminal.success(f'Added the plugin "{manifest.id}" {manifest.version.text} to `{Repository.shown(folder)}`.')
        missing = [name for name, parameter in manifest.parameters.items() if parameter.required]
        if missing:
            names = ", ".join(f'"{name}"' for name in missing)
            session.terminal.warning(f'Fill in {names} under "plugins.{manifest.id}.parameters" of `{Repository.shown(folder / "haylen.json")}` before the app builds.')
        for dependency in manifest.dependencies:
            session.terminal.warning(f'The plugin "{manifest.id}" depends on the plugin "{dependency}". Add it with "haylen plugin add" if the app does not list it yet.')
        return {"id": manifest.id, "version": manifest.version.text}
