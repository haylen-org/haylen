from dataclasses import dataclass
from pathlib import Path

from haylen.content.protected_releases import ProtectedReleases
from haylen.core.app_kind import AppKind
from haylen.core.configuration import Configuration
from haylen.core.errors.invalid_manifest import InvalidManifest
from haylen.core.repository import Repository
from haylen.core.resolved_product import ResolvedProduct
from haylen.native.native_library import NativeLibrary
from haylen.platforms.android.android_platform import AndroidPlatform
from haylen.platforms.android.android_template import AndroidTemplate


@dataclass(frozen=True)
class AndroidNativeBuild:
    """The native code of an Android app, which the external native build of Gradle compiles for every ABI of the product against the engine SDK of that ABI: the library `haylen-app` that the activity loads instead of the Lua player of the `haylen` library, with the sources of a C++ app or the Lua player with the bootstrap of a Lua release, and the native libraries of the app and its plugins that build from a CMake project. Prebuilt libraries go into `jniLibs` as they are. A Lua debug build compiles no app library, since it plays the package with the player of the `haylen` library."""

    APP_LIBRARY = "haylen-app"
    PLAYER_LIBRARY = "haylen"
    FOLDER = "native"
    CMAKE_LISTS = f"{FOLDER}/CMakeLists.txt"
    BOOTSTRAP = f"{FOLDER}/{ProtectedReleases.BOOTSTRAP_FILE}"
    PACKAGE = "app/src/main/assets/app"

    product: ResolvedProduct
    compiles_app: bool
    built: tuple[NativeLibrary, ...]
    prebuilt: tuple[NativeLibrary, ...]

    @staticmethod
    def of(product: ResolvedProduct) -> AndroidNativeBuild:
        libraries = product.primary.native_libraries
        compiles_app = product.app.kind is AppKind.CPP or product.configuration is Configuration.RELEASE
        built = tuple(library for library in libraries if library.cmake is not None)
        prebuilt = tuple(library for library in libraries if library.cmake is None)
        return AndroidNativeBuild(product, compiles_app, built, prebuilt)

    @property
    def library(self) -> str:
        """The library that the activity loads."""
        return AndroidNativeBuild.APP_LIBRARY if self.compiles_app else AndroidNativeBuild.PLAYER_LIBRARY

    @property
    def targets(self) -> tuple[str, ...]:
        """The CMake targets of the external native build, or none when the app compiles nothing."""
        own = (AndroidNativeBuild.APP_LIBRARY,) if self.compiles_app else ()
        return (*own, *(library.name for library in self.built))

    @property
    def abis(self) -> tuple[str, ...]:
        return tuple(project.target.abi for project in self.product.projects)

    def cmake_lists(self, release: bool) -> str:
        """The CMake project of the native code, which picks the SDK of each ABI by the ABI that Gradle builds."""
        lines = [
            f"set({AndroidTemplate.cmake('HAYLEN_SDK_' + project.target.abi)} {AndroidTemplate.cmake(project.sdk.file(AndroidPlatform.CMAKE_FILE).as_posix())})" for project in self.product.projects
        ]
        tokens = {"SDKS": "\n".join(lines), "APP": self.app_lines(release), "LIBRARIES": self.library_lines()}
        return AndroidTemplate.render(AndroidTemplate.NATIVE, tokens)

    def app_lines(self, release: bool) -> str:
        if not self.compiles_app:
            return ""
        # The package of the generated project is what the app ships, which Gradle takes into the assets itself.
        content = f'"${{CMAKE_CURRENT_LIST_DIR}}/../{AndroidNativeBuild.PACKAGE}"'
        arguments = [AndroidNativeBuild.APP_LIBRARY]
        cpp = self.product.app.cpp
        if cpp is not None:
            arguments += ["CPP", "SOURCES", *(AndroidTemplate.cmake(path.as_posix()) for path in cpp.files())]
        else:
            arguments.append("LUA")
        arguments += ["CONTENT", content]
        if release:
            arguments += ["BOOTSTRAP", f'"${{CMAKE_CURRENT_LIST_DIR}}/{ProtectedReleases.BOOTSTRAP_FILE}"']

        lines = ["", f"haylen_add_app({' '.join(arguments)})"]
        if cpp is not None and cpp.include_directories:
            folders = " ".join(AndroidTemplate.cmake(path.as_posix()) for path in cpp.include_directories)
            lines.append(f"target_include_directories({AndroidNativeBuild.APP_LIBRARY} PRIVATE {folders})")
        if cpp is not None and cpp.definitions:
            definitions = " ".join(AndroidTemplate.cmake(definition) for definition in cpp.definitions)
            lines.append(f"target_compile_definitions({AndroidNativeBuild.APP_LIBRARY} PRIVATE {definitions})")
        return "\n".join(lines) + "\n"

    def library_lines(self) -> str:
        """Adds the CMake project of every library once, whose target of the name of the library builds as a shared library."""
        if not self.built:
            return ""
        lines = ["", "set(BUILD_SHARED_LIBS ON)"]
        folders: list[Path] = []
        for library in self.built:
            if library.cmake in folders:
                continue
            folders.append(library.cmake)
            lines.append(f"add_subdirectory({AndroidTemplate.cmake(library.cmake.as_posix())} {AndroidTemplate.cmake('libraries/' + library.name)} EXCLUDE_FROM_ALL)")
        return "\n".join(lines) + "\n"

    def prebuilt_files(self) -> dict[str, Path]:
        """The prebuilt libraries in the `jniLibs` of the app module, from their folders of one subfolder per ABI, for the ABIs of the product."""
        files: dict[str, Path] = {}
        for library in self.prebuilt:
            folder = library.files[AndroidPlatform.PLATFORM]
            for abi in self.abis:
                built = folder / abi / f"lib{library.name}.so"
                if not built.is_file():
                    raise InvalidManifest(
                        f'The native library "{library.name}" has no `{Repository.shown(built)}`. Its Android files are a folder with one subfolder of libraries per ABI, and the app builds for "{abi}".',
                        origin=self.product.app.origin,
                    )
                files |= {f"app/src/main/jniLibs/{abi}/{path.name}": path for path in sorted((folder / abi).glob("*.so"))}
        return files
