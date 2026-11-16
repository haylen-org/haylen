import argparse
import json
import os
import sys

from haylen.cli.command import Command
from haylen.cli.command_session import CommandSession
from haylen.cli.command_tree import CommandTree
from haylen.cli.terminated import Terminated
from haylen.core.errors.haylen_error import HaylenError
from haylen.core.host import Host
from haylen.core.repository import Repository
from haylen.core.tooling_context import ToolingContext
from haylen.process import Process
from haylen.terminal import Terminal


class Application:
    """The command line `haylen`: it parses the command, prints its steps in the terminal or its result as JSON with `--json`, and ends with the sentence of a failure instead of a traceback. The launchers `haylen` and `haylen.cmd` only run it through uv."""

    @staticmethod
    def main() -> None:
        # A console of Windows takes any text, while a pipe or a file there takes the code page of the system, so a character outside it prints as a replacement instead of stopping the command.
        for stream in (sys.stdout, sys.stderr):
            stream.reconfigure(errors="replace")
        Terminated.install()
        parser, commands = Application.parser()
        arguments = parser.parse_args()
        command = commands[arguments.command_path]

        terminal = Terminal(sys.stdout, sys.stderr, os.environ, Repository.LOGS / f"{'-'.join(command.PATH)}.log", structured=arguments.json)
        terminal.verbose = arguments.verbose
        terminal.log.unlink(missing_ok=True)
        # The OpenSSL build that Varn adds runs as many jobs as `CMAKE_BUILD_PARALLEL_LEVEL` named when its tree was configured, so every build keeps to its `--jobs` without it.
        os.environ.pop("CMAKE_BUILD_PARALLEL_LEVEL", None)
        session = CommandSession(ToolingContext(terminal, Process(terminal), arguments.jobs, os.environ), arguments.json)

        try:
            result = command.run(arguments, session)
        except HaylenError as error:
            Application.fail(error, terminal, arguments.json)
            raise SystemExit(1) from None
        except KeyboardInterrupt:
            raise SystemExit(130) from None
        except Terminated as ended:
            raise SystemExit(128 + ended.number) from None
        if arguments.json:
            print(json.dumps({"ok": True, "result": result or {}}, indent=4, default=str))

    @staticmethod
    def fail(error: HaylenError, terminal: Terminal, structured: bool) -> None:
        if structured:
            print(json.dumps({"ok": False, "error": error.describe()}, indent=4))
            return
        terminal.error(error.message, error.details)

    @staticmethod
    def parser() -> tuple[argparse.ArgumentParser, dict[tuple[str, ...], Command]]:
        """Builds the parser of every command of the tree, with the global options that every command also takes after its words."""
        parser = argparse.ArgumentParser(
            prog="haylen",
            description="Creates, prepares, builds, verifies and runs Haylen apps through the backends of their targets, builds the engine SDKs and develops the engine.",
        )
        parser.add_argument("--version", action="version", version=f"Haylen {Repository.engine_version()}", help="Show the version of the engine and exit.")
        Application.global_options(parser, argparse.SUPPRESS)
        parser.set_defaults(verbose=False, json=False, jobs=Host.default_jobs())
        common = argparse.ArgumentParser(add_help=False)
        Application.global_options(common, argparse.SUPPRESS)

        commands: dict[tuple[str, ...], Command] = {}
        groups = {(): parser.add_subparsers(dest="command", required=True, metavar="command")}
        for command_class in CommandTree.COMMANDS:
            command = command_class()
            for depth in range(1, len(command.PATH)):
                prefix = command.PATH[:depth]
                if prefix not in groups:
                    group = groups[prefix[:-1]].add_parser(prefix[-1], parents=[common], help=CommandTree.GROUPS[prefix])
                    groups[prefix] = group.add_subparsers(dest=f"action_{depth}", required=True, metavar="action")
            sub = groups[command.PATH[:-1]].add_parser(command.PATH[-1], parents=[common], help=command.HELP, description=command.HELP)
            command.configure(sub)
            sub.set_defaults(command_path=command.PATH)
            commands[command.PATH] = command
        return parser, commands

    @staticmethod
    def global_options(parser: argparse.ArgumentParser, default: object) -> None:
        parser.add_argument(
            "--verbose",
            action="store_true",
            default=default,
            help='Show the whole output of every tool in the terminal, which a command otherwise keeps in its log in "build/logs".',
        )
        parser.add_argument(
            "--json",
            action="store_true",
            default=default,
            help="Print the result of the command as one JSON document, and its steps and the output of its tools on the error output, for continuous integration and editors.",
        )
        parser.add_argument("--jobs", type=int, default=default, help="Parallel jobs of the builds, one less than the processors of this machine by default.")
