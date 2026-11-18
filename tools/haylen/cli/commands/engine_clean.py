import argparse
from collections.abc import Mapping

from haylen.cli.command import Command
from haylen.cli.command_session import CommandSession
from haylen.core.folders import Folders
from haylen.core.repository import Repository


class EngineCleanCommand(Command):
    PATH = ("engine", "clean")
    HELP = 'Remove the build folder of the checkout: every build tree of the engine, every SDK, the build folders of apps and the logs. The tools in ".tools" stay.'

    def configure(self, parser: argparse.ArgumentParser) -> None:
        return

    def run(self, arguments: argparse.Namespace, session: CommandSession) -> Mapping[str, object]:
        if not Repository.BUILD.exists():
            session.terminal.success("The build folder is already clean.")
            return {"removed": False}
        Folders.remove(Repository.BUILD)
        session.terminal.success("Cleaned the build folder.")
        return {"removed": True}
