from collections.abc import Mapping, Sequence
from pathlib import Path

from haylen.core.host import Host
from haylen.core.repository import Repository
from haylen.core.tooling_context import ToolingContext
from haylen.native.cmake_tree import CMakeTree
from haylen.toolchains.apple_sdk import AppleSdk
from haylen.toolchains.build_tools import BuildTools
from haylen.toolchains.shader_compiler import ShaderCompiler


class EngineTree:
    """A build tree of the root project under `build/engine/`, which builds the engine with its tests, benchmarks, fuzzers or the test port, for developing the engine itself. Apps never build against it, since they take the engine from an SDK. A tree that the shader compiler of another pin or another uv configured configures again, so its shaders and its tests never use an old tool."""

    CONFIGURATIONS = ("Debug", "Release", "RelWithDebInfo")
    SANITIZERS = ("address", "thread")
    # The options that leave out what a tree for one purpose does not build.
    ONLY_TESTS = ("-DHAYLEN_BUILD_TESTS=ON", "-DHAYLEN_BUILD_PLAYER=OFF", "-DHAYLEN_BUILD_SDK=OFF", "-DHAYLEN_BUILD_TOOLS=OFF", "-DHAYLEN_BUILD_BENCHMARKS=OFF")

    def __init__(self, context: ToolingContext, name: str) -> None:
        self.context = context
        self.tree = CMakeTree(context, Repository.ENGINE_BUILDS / name)

    @staticmethod
    def host_name(configuration: str, sanitizers: str | None = None, suffix: str = "") -> str:
        """The name of the tree of this machine for a configuration, with a tree of its own for each sanitizer and each other generator, so switching never rebuilds the plain tree."""
        parts = [Host.system(), configuration.lower(), *([sanitizers] if sanitizers else []), *([suffix] if suffix else [])]
        return "-".join(parts)

    @property
    def folder(self) -> Path:
        return self.tree.folder

    def uv(self) -> Path:
        return BuildTools.UV.require()

    def command(self, configuration: str, *options: object) -> list[object]:
        """The configure command of the root project into this tree, with the shader compiler of the pin and the uv that the engine tests run the tooling with."""
        return [
            "cmake",
            "-S",
            Repository.ROOT,
            "-B",
            self.folder,
            f"-DCMAKE_BUILD_TYPE={configuration}",
            f"-DHAYLEN_SHADERC={ShaderCompiler.path(self.context).as_posix()}",
            f"-DHAYLEN_UV={self.uv().as_posix()}",
            *options,
        ]

    @staticmethod
    def host_options(sanitizers: str | None = None, xcode: bool = False) -> list[str]:
        """The generator and the options of a tree that runs on this machine."""
        options = ["-G", "Xcode"] if xcode else CMakeTree.generator()
        options.append(f"-DHAYLEN_SANITIZERS={(sanitizers or 'off').upper()}")
        if Host.system() == "macos":
            options.append(f"-DCMAKE_OSX_DEPLOYMENT_TARGET={AppleSdk.MINIMUM_VERSIONS['macOS']}")
        return options

    def is_current(self) -> bool:
        """Tells whether CMake configured and generated the tree with the shader compiler of the current pin and the uv of this machine."""
        if not self.tree.configured:
            return False
        compiler = self.tree.cache_value("HAYLEN_SHADERC")
        uv = self.tree.cache_value("HAYLEN_UV")
        if compiler is None or uv is None:
            return False
        return Path(compiler) == ShaderCompiler.path(self.context) and Path(uv) == self.uv()

    def configure(self, command: Sequence[object], env: Mapping[str, str] | None = None) -> None:
        self.context.terminal.step(f"Configuring the engine in `{Repository.shown(self.folder)}`")
        self.tree.configure(command, env)

    def ensure(self, command: Sequence[object], env: Mapping[str, str] | None = None) -> None:
        """Configures the tree when it was never configured, its configure failed or another tool configured it."""
        if not self.is_current():
            self.configure(command, env)

    def build(self, targets: Sequence[str], configuration: str, *arguments: str) -> None:
        names = ", ".join(f'"{target}"' for target in targets) if targets else "the engine"
        self.context.terminal.step(f"Building {names} in `{Repository.shown(self.folder)}`")
        command: list[object] = ["cmake", "--build", self.folder, "--config", configuration, "--parallel", str(self.context.jobs)]
        if targets:
            command += ["--target", *targets]
        if arguments:
            command += ["--", *arguments]
        self.context.process.run(command)

    def binary(self, name: str) -> Path:
        return self.folder / "bin" / Host.executable(name)
