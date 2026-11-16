import argparse
from collections.abc import Mapping

from haylen.cli.arguments import Arguments
from haylen.cli.command import Command
from haylen.cli.command_session import CommandSession
from haylen.core.repository import Repository
from haylen.plugins.app_plugins import AppPlugins


class PluginListCommand(Command):
    PATH = ("plugin", "list")
    HELP = "List the plugins of an app with their versions, the platforms they support, where they come from and what keeps them from building."

    def configure(self, parser: argparse.ArgumentParser) -> None:
        parser.add_argument("--app", default=".", help="The app folder, the current folder by default.")

    def run(self, arguments: argparse.Namespace, session: CommandSession) -> Mapping[str, object]:
        folder = Arguments.app_folder(arguments)
        statuses = AppPlugins(session.context, folder).statuses()
        if session.structured:
            return {"plugins": [status.describe() for status in statuses]}

        if not statuses:
            session.terminal.info(f"The app `{Repository.shown(folder)}` has no plugins.")
        for status in statuses:
            state = f"{len(status.problems)} problems" if status.problems else "Ready"
            if not status.listed:
                state = 'Not in "haylen.json", so no build carries it'
            session.terminal.rows([(status.id, f"{status.version or '?'}, {', '.join(status.implementations)}, from `{status.source}`, {state}")], keys=True)
            for problem in status.problems:
                session.terminal.note(f"    {problem}")
        return {"plugins": [status.describe() for status in statuses]}
