import argparse
from collections.abc import Mapping

from haylen.cli.command import Command
from haylen.cli.command_session import CommandSession
from haylen.core.repository import Repository
from haylen.toolchains.emscripten import Emscripten
from haylen.toolchains.shader_compiler import ShaderCompiler


class EngineToolsCommand(Command):
    PATH = ("engine", "tools")
    HELP = 'Install the pinned build tools of the engine into ".tools" now instead of at the first build that needs them: the Haylen shader compiler, and Emscripten with "--emscripten".'

    def configure(self, parser: argparse.ArgumentParser) -> None:
        parser.add_argument("--emscripten", action="store_true", help=f'Install Emscripten {Emscripten.VERSION} into ".tools/emsdk" too.')

    def run(self, arguments: argparse.Namespace, session: CommandSession) -> Mapping[str, object]:
        tools = {"haylen-shaderc": ShaderCompiler.path(session.context)}
        if arguments.emscripten:
            tools["Emscripten"] = Emscripten.install(session.context)
        for name, path in tools.items():
            session.terminal.success(f'The tool "{name}" is ready at `{Repository.shown(path)}`.')
        return {name: str(path) for name, path in tools.items()}
