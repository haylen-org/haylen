import argparse
from collections.abc import Mapping
from pathlib import Path

from haylen.cli.arguments import Arguments
from haylen.cli.command import Command
from haylen.cli.command_session import CommandSession
from haylen.core.app_manifest import AppManifest
from haylen.core.configuration import Configuration
from haylen.core.package_layout import PackageLayout
from haylen.core.repository import Repository
from haylen.plugins.plugin_lock import PluginLock
from haylen.plugins.plugin_resolver import PluginResolver
from haylen.plugins.plugin_source import PluginSource


class AppZipCommand(Command):
    PATH = ("app", "zip")
    HELP = 'Zip the package of an app: "haylen.json", "content" and the manifest of every plugin wherever it comes from, with "source" and the Lua modules of every plugin for an app that runs Lua, which a player or the web runtime runs as it is.'

    def configure(self, parser: argparse.ArgumentParser) -> None:
        Arguments.add_app(parser)
        parser.add_argument("-o", "--output", default="app.zip", help='The zip file to write, "app.zip" in the current folder by default.')

    def run(self, arguments: argparse.Namespace, session: CommandSession) -> Mapping[str, object]:
        app = AppManifest.load(Arguments.app_folder(arguments))
        source = PluginSource(app.folder, PluginLock.load(app.folder), session.context.process, session.terminal)
        loaded = PluginResolver(app, source).load(Configuration.DEBUG)
        layout = PackageLayout.of_app(app, {manifest.id: manifest.folder for _, manifest in loaded})
        output = Path(arguments.output).expanduser().resolve()
        layout.zip(output)
        session.terminal.success(f"Zipped the package of `{Repository.shown(app.folder)}` into `{Repository.shown(output)}`.")
        return {"output": str(output), "files": len(layout.files())}
