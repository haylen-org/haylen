import json
from pathlib import Path

from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.core.host import Host
from haylen.core.repository import Repository
from haylen.core.tooling_context import ToolingContext
from haylen.engine.engine_tree import EngineTree
from haylen.toolchains.build_tools import BuildTools
from haylen.toolchains.path_tool import PathTool


class GccCheck:
    """Compiles every source of every target of the engine with the GCC of this Linux machine and warnings as errors, so the warnings that only GCC raises fail before continuous integration does. It links nothing, since a warning comes from a compile while the links of the programs would take most of the time, keeps its tree, so a later check compiles only what changed, and runs no test. Code is checked on a machine of its own platform, so the check runs on Linux alone."""

    GCC = PathTool("gcc", 'Install GCC, such as with "apt install gcc g++".')
    GXX = PathTool("g++", 'Install GCC, such as with "apt install gcc g++".')
    SUFFIX = "gcc"
    OBJECTS = "objects.ninja"
    OBJECTS_TARGET = "haylen-objects"

    def __init__(self, context: ToolingContext) -> None:
        self.context = context

    def run(self, configuration: str) -> dict[str, object]:
        if Host.system() != "linux":
            raise UnsupportedTarget(f'The option "--compiler gcc" checks the engine with the GCC of a Linux machine, and this machine runs "{Host.system()}". Run it on a Linux machine.')

        tree = EngineTree(self.context, EngineTree.host_name(configuration, suffix=GccCheck.SUFFIX))
        # The objects keep the file and line of each instruction alone, as the Debug trees of continuous integration do.
        options = [
            *EngineTree.host_options(),
            f"-DCMAKE_C_COMPILER={GccCheck.GCC.require().as_posix()}",
            f"-DCMAKE_CXX_COMPILER={GccCheck.GXX.require().as_posix()}",
            "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
            "-DHAYLEN_WARNINGS_AS_ERRORS=ON",
            "-DHAYLEN_DEBUG_INFO=LINES",
        ]
        tree.ensure(tree.command(configuration, *options))

        # Ninja generates again only the file of a build that it was given, so CMake runs first in a tree whose CMake files changed, and the compile commands hold the sources of the current tree.
        ninja = BuildTools.NINJA.require()
        self.context.process.run([ninja, "-C", tree.folder, "build.ninja"])
        (tree.folder / GccCheck.OBJECTS).write_text(GccCheck.objects(tree.folder), encoding="utf-8")

        self.context.terminal.step(f"Compiling every source of the engine with GCC in `{Repository.shown(tree.folder)}`")
        # The build goes on after a failure, so one check reports every warning of the change.
        self.context.process.run([ninja, "-C", tree.folder, "-f", GccCheck.OBJECTS, "-j", str(self.context.jobs), "-k", "0", GccCheck.OBJECTS_TARGET])
        self.context.terminal.success("GCC compiled every source of the engine without a warning.")
        return {"tree": str(tree.folder), "compiler": "gcc", "configuration": configuration}

    @staticmethod
    def objects(folder: Path) -> str:
        """The Ninja file that builds the object of every compile command of a tree, which reads the build of the tree for their rules."""
        database = json.loads((folder / "compile_commands.json").read_text(encoding="utf-8"))
        outputs = sorted({GccCheck.path(GccCheck.output(folder, entry)) for entry in database})
        lines = ["# Written by the tooling for the check of every source with GCC.", "include build.ninja", f"build {GccCheck.OBJECTS_TARGET}: phony {' '.join(outputs)}", ""]
        return "\n".join(lines)

    @staticmethod
    def output(folder: Path, entry: dict[str, str]) -> Path:
        """The object of a compile command, relative to its tree, as the build of the tree names it."""
        output = Path(entry["output"])
        return output.relative_to(folder) if output.is_absolute() else output

    @staticmethod
    def path(path: Path) -> str:
        """A path of a Ninja file, whose spaces and colons Ninja reads as separators."""
        return str(path).replace("$", "$$").replace(" ", "$ ").replace(":", "$:")
