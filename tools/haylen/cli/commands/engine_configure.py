import argparse
from collections.abc import Mapping

from haylen.cli.command import Command
from haylen.cli.command_session import CommandSession
from haylen.core.repository import Repository
from haylen.engine.engine_tree import EngineTree


class EngineConfigureCommand(Command):
    PATH = ("engine", "configure")
    HELP = 'Configure the build tree of the engine of this machine under "build/engine", with its tests, benchmarks, tools and player, for developing the engine. Apps never build against it, since "haylen sdk build" makes their SDKs.'

    @staticmethod
    def add_tree(parser: argparse.ArgumentParser) -> None:
        parser.add_argument("--config", default="Debug", choices=EngineTree.CONFIGURATIONS, help='The configuration of the tree, "Debug" by default.')
        parser.add_argument("--sanitizers", choices=EngineTree.SANITIZERS, help="A tree of its own with AddressSanitizer and UndefinedBehaviorSanitizer, or with ThreadSanitizer.")
        parser.add_argument("--xcode", action="store_true", help="A tree of its own with the Xcode generator on macOS, for debugging the engine in Xcode.")

    @staticmethod
    def tree(arguments: argparse.Namespace, session: CommandSession) -> tuple[EngineTree, list[object]]:
        tree = EngineTree(session.context, EngineTree.host_name(arguments.config, arguments.sanitizers, "xcode" if arguments.xcode else ""))
        return tree, tree.command(arguments.config, *EngineTree.host_options(arguments.sanitizers, arguments.xcode))

    def configure(self, parser: argparse.ArgumentParser) -> None:
        EngineConfigureCommand.add_tree(parser)

    def run(self, arguments: argparse.Namespace, session: CommandSession) -> Mapping[str, object]:
        tree, command = EngineConfigureCommand.tree(arguments, session)
        tree.configure(command)
        session.terminal.success(f"Configured the engine in `{Repository.shown(tree.folder)}`.")
        return {"tree": str(tree.folder)}
