import argparse
from collections.abc import Mapping

from haylen.cli.command import Command
from haylen.cli.command_session import CommandSession
from haylen.cli.commands.engine_configure import EngineConfigureCommand
from haylen.core.repository import Repository


class EngineBuildCommand(Command):
    PATH = ("engine", "build")
    HELP = 'Configure the build tree of the engine of this machine when it needs it and build it, or one CMake target of it with "--cmake-target".'

    def configure(self, parser: argparse.ArgumentParser) -> None:
        EngineConfigureCommand.add_tree(parser)
        parser.add_argument("--cmake-target", action="append", dest="targets", help='A CMake target to build, such as "haylen-tests", which repeats. Every target by default.')

    def run(self, arguments: argparse.Namespace, session: CommandSession) -> Mapping[str, object]:
        tree, command = EngineConfigureCommand.tree(arguments, session)
        tree.ensure(command)
        targets = arguments.targets or []
        tree.build(targets, arguments.config)
        session.terminal.success(f"Built the engine in `{Repository.shown(tree.folder)}`.")
        return {"tree": str(tree.folder), "targets": targets}
