import shutil
from pathlib import Path

from haylen.core.errors.build_failed import BuildFailed
from haylen.core.errors.invalid_manifest import InvalidManifest
from haylen.core.folders import Folders
from haylen.core.host import Host
from haylen.core.repository import Repository
from haylen.core.resolved_project import ResolvedProject
from haylen.core.tooling_context import ToolingContext
from haylen.native.cmake_tree import CMakeTree
from haylen.native.native_library import NativeLibrary
from haylen.native.xc_framework import XcFramework
from haylen.toolchains.apple_sdk import AppleSdk


class HostNativeLibraries:
    """Places the native libraries of the app and its plugins that ship to the desktop of this machine in one folder, where the desktop player and the executables of desktop apps load them with `native.load`: a prebuilt file as it is, the macOS library of a prebuilt xcframework, or the shared library that the CMake project of a library builds against the native API of the SDK, whose target `haylen::native` gives it `HaylenNative.h` and nlohmann/json. Desktops load every library dynamically."""

    SHARED_LIBRARIES = {"macos": "lib{name}.dylib", "windows": "{name}.dll", "linux": "lib{name}.so"}

    def __init__(self, context: ToolingContext) -> None:
        self.context = context

    def place(self, project: ResolvedProject, package: Path, builds: Path, folder: Path) -> tuple[Path, ...]:
        """Builds or copies every library of a project into `folder`, which it empties first, with the CMake package of the SDK in `package`, and the build trees of the libraries under `builds`, and returns the placed libraries."""
        Folders.remove(folder)
        folder.mkdir(parents=True)
        placed = []
        for library in project.native_libraries:
            if library.cmake is None:
                placed.append(HostNativeLibraries.copy_into(HostNativeLibraries.prebuilt(library, project.platform), folder))
                continue
            built = self.build(library, project.platform, package, builds / library.name)
            placed.append(HostNativeLibraries.copy_into(built, folder))
        return tuple(placed)

    def build(self, library: NativeLibrary, platform: str, package: Path, folder: Path) -> Path:
        """Configures and builds the target of a library, which takes the name of the library, as a shared library, and returns the file it built."""
        output = folder / "out"
        command: list[object] = [
            "cmake",
            "-S",
            library.cmake,
            "-B",
            folder / "build",
            *CMakeTree.generator(),
            "-DCMAKE_BUILD_TYPE=Release",
            "-DBUILD_SHARED_LIBS=ON",
            f"-Dhaylen_DIR={package.as_posix()}",
        ]
        command += [f"-DCMAKE_{kind}_OUTPUT_DIRECTORY{suffix}={output.as_posix()}" for kind in ("LIBRARY", "ARCHIVE", "RUNTIME") for suffix in ("", "_RELEASE")]
        if Host.system() == "macos":
            command.append(f"-DCMAKE_OSX_DEPLOYMENT_TARGET={AppleSdk.MINIMUM_VERSIONS['macOS']}")
        tree = CMakeTree(self.context, folder / "build")
        self.context.terminal.step(f'Building the native library "{library.name}" in `{Repository.shown(folder)}`')
        tree.configure(command)
        tree.build([library.name], "Release")

        name = HostNativeLibraries.SHARED_LIBRARIES[platform].format(name=library.name)
        built = output / name
        if not built.is_file():
            raise BuildFailed(
                f'The CMake target "{library.name}" of `{Repository.shown(library.cmake)}` built no "{name}" into `{Repository.shown(output)}`. Make the target a library named after the native library.'
            )
        return built

    @staticmethod
    def prebuilt(library: NativeLibrary, platform: str) -> Path:
        """Returns the prebuilt file of a library for a platform, which an xcframework holds among its slices on macOS."""
        path = library.files[platform]
        if path.suffix != ".xcframework":
            return path
        library_path = XcFramework.library(path, "macos", None)
        if library_path is not None:
            return library_path
        raise InvalidManifest(
            f'The xcframework `{Repository.shown(path)}` of the native library "{library.name}" has no slice of macOS. Add that slice, or leave "macos" out of the platforms of the library.'
        )

    @staticmethod
    def copy_into(source: Path, folder: Path) -> Path:
        """Copies a library file or bundle into a folder, keeping the links inside bundles."""
        destination = folder / source.name
        if source.is_dir():
            shutil.copytree(source, destination, symlinks=True)
        else:
            shutil.copy2(source, destination)
        return destination
