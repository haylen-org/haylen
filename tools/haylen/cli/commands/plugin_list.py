import argparse
from collections.abc import Mapping
from pathlib import Path

from haylen.cli.arguments import Arguments
from haylen.cli.command import Command
from haylen.cli.command_session import CommandSession
from haylen.core.repository import Repository
from haylen.plugins.app_plugins import AppPlugins
from haylen.terminal import Terminal


class PluginListCommand(Command):
    PATH = ("plugin", "list")
    HELP = "List the plugins of an app with their versions, the platforms they support, where they come from and what keeps them from building."

    COLUMNS = (
        Terminal.Column("Plugin", style=Terminal.EXPRESSION),
        Terminal.Column("Version"),
        Terminal.Column("Platforms", shrink=2),
        Terminal.Column("Source", shrink=1, cut=True),
        Terminal.Column("State"),
    )

    def configure(self, parser: argparse.ArgumentParser) -> None:
        parser.add_argument("--app", default=".", help="The app folder, the current folder by default.")

    def run(self, arguments: argparse.Namespace, session: CommandSession) -> Mapping[str, object]:
        folder = Arguments.app_folder(arguments)
        statuses = AppPlugins(session.context, folder).statuses()
        if not session.structured:
            PluginListCommand.show(session.terminal, folder, statuses)
        return {"plugins": [status.describe() for status in statuses]}

    @staticmethod
    def show(terminal: Terminal, folder: Path, statuses: list[AppPlugins.Status]) -> None:
        """Prints a table of the plugins with their state, where a plugin folder that "haylen.json" does not list, and so no build carries, is not listed, and then the problems of each plugin under its name."""
        if not statuses:
            terminal.info(f"The app `{Repository.shown(folder)}` has no plugins.")
            return
        terminal.group("Plugins", len(statuses))
        terminal.table(PluginListCommand.COLUMNS, [PluginListCommand.row(status) for status in statuses])
        for status in statuses:
            if not status.problems:
                continue
            terminal.group(f'Problems of "{status.id}"', len(status.problems))
            for problem in status.problems:
                terminal.item(Terminal.Sign.FAILED, problem)

    @staticmethod
    def row(status: AppPlugins.Status) -> tuple[str | Terminal.Cell, ...]:
        if not status.listed:
            state = Terminal.Cell("Not listed", Terminal.Sign.IDLE)
        elif status.problems:
            state = Terminal.Cell("Problems", Terminal.Sign.FAILED)
        else:
            state = Terminal.Cell("Ready", Terminal.Sign.READY)
        return (status.id, status.version or "", ", ".join(status.implementations), status.source, state)
