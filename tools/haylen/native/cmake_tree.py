import os
import shutil
from collections.abc import Mapping, Sequence
from pathlib import Path

from haylen.core.repository import Repository
from haylen.core.tooling_context import ToolingContext


class CMakeTree:
    """A CMake build tree that a command configures, builds and installs. It records that CMake configured and generated it only once a configure succeeded, so a tree whose configure failed configures again instead of building."""

    CONFIGURED = "haylen-configured.txt"
    # Apps and plugins use no C++ modules, like the engine, so their trees compile without the scan for modules, which CMake would pair with a module `std` read from the standard library that it finds, the one of the host even for a cross compiler such as the one of the Android NDK.
    WITHOUT_MODULE_SCAN = "-DCMAKE_CXX_SCAN_FOR_MODULES=OFF"

    def __init__(self, context: ToolingContext, folder: Path) -> None:
        self.context = context
        self.folder = folder

    @staticmethod
    def cpm_cache(environment: Mapping[str, str]) -> str:
        """The folder where CPM keeps the sources of the dependencies, which every tree shares."""
        return environment.get("CPM_SOURCE_CACHE", str(Repository.ROOT / ".cache" / "cpm"))

    @staticmethod
    def generator() -> list[str]:
        """Ninja, or the default generator on Windows without Ninja."""
        return ["-G", "Ninja"] if os.name != "nt" or shutil.which("ninja") else []

    @property
    def configured(self) -> bool:
        return (self.folder / CMakeTree.CONFIGURED).is_file()

    def configure(self, command: Sequence[object], env: Mapping[str, str] | None = None) -> None:
        self.folder.mkdir(parents=True, exist_ok=True)
        (self.folder / CMakeTree.CONFIGURED).unlink(missing_ok=True)
        self.context.process.run(command, env=env)
        (self.folder / CMakeTree.CONFIGURED).write_text("", encoding="utf-8")

    def build(self, targets: Sequence[str], configuration: str) -> None:
        self.context.process.run(["cmake", "--build", self.folder, "--config", configuration, "--target", *targets, "--parallel", str(self.context.jobs)])

    def install(self, component: str, prefix: Path, configuration: str) -> None:
        self.context.process.run(["cmake", "--install", self.folder, "--config", configuration, "--component", component, "--prefix", prefix])

    def cache_value(self, name: str) -> str | None:
        """Returns a value of the cache of the tree, or nothing when the tree has no cache or the cache lacks it."""
        cache = self.folder / "CMakeCache.txt"
        if not cache.is_file():
            return None
        for line in cache.read_text(encoding="utf-8").splitlines():
            key, _, value = line.partition("=")
            if key.split(":")[0] == name:
                return value
        return None
