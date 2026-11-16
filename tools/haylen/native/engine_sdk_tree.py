from pathlib import Path

from haylen.core.errors.build_failed import BuildFailed
from haylen.core.repository import Repository
from haylen.core.tooling_context import ToolingContext
from haylen.native.cmake_tree import CMakeTree
from haylen.toolchains.shader_compiler import ShaderCompiler


class EngineSdkTree:
    """A build tree of the engine project alone under `build/engine/` from which an engine port installs an SDK, with the player and without the tests and benchmarks. A tree that the shader compiler of another pin configured configures again, so the shaders of the SDK never come from an old compiler."""

    def __init__(self, context: ToolingContext, name: str) -> None:
        self.context = context
        self.tree = CMakeTree(context, Repository.ENGINE_BUILDS / name)

    @property
    def folder(self) -> Path:
        return self.tree.folder

    def is_current(self) -> bool:
        recorded = self.tree.cache_value("HAYLEN_SHADERC")
        return self.tree.configured and recorded is not None and Path(recorded) == ShaderCompiler.path(self.context)

    def command(self, configuration: str, *options: object) -> list[object]:
        """The configure command of the engine into this tree for an SDK with its player, with the shader compiler of the pin and the shared sources of the dependencies."""
        return [
            "cmake",
            "-S",
            Repository.ENGINE,
            "-B",
            self.folder,
            f"-DHAYLEN_SHADERC={ShaderCompiler.path(self.context).as_posix()}",
            f"-DCPM_SOURCE_CACHE={CMakeTree.cpm_cache(self.context.environment)}",
            f"-DCMAKE_BUILD_TYPE={configuration}",
            "-DHAYLEN_BUILD_SDK=ON",
            "-DHAYLEN_BUILD_PLAYER=ON",
            "-DHAYLEN_BUILD_TESTS=OFF",
            "-DHAYLEN_BUILD_BENCHMARKS=OFF",
            *options,
        ]

    def graphics_apis(self) -> tuple[str, ...]:
        """The graphics APIs whose devices the tree compiled, which its cache records."""
        recorded = self.tree.cache_value("HAYLEN_GRAPHICS_APIS")
        if not recorded:
            raise BuildFailed(f'The engine tree `{Repository.shown(self.folder)}` records no "HAYLEN_GRAPHICS_APIS", so its configure did not finish. Build the SDK again.')
        return tuple(recorded.split(";"))

    @staticmethod
    def package_folder(installed: Path) -> Path:
        """The folder of the CMake package of an installed SDK, whose library folder `GNUInstallDirs` names after the system."""
        configs = sorted(installed.rglob("haylen-config.cmake"))
        if not configs:
            raise BuildFailed(f'The SDK installed into `{Repository.shown(installed)}` holds no "haylen-config.cmake", so its install did not finish. Build the SDK again.')
        return configs[0].parent
