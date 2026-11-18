import plistlib
from dataclasses import dataclass
from pathlib import Path

from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.core.folders import Folders
from haylen.core.repository import Repository
from haylen.core.resolved_product import ResolvedProduct
from haylen.core.tooling_context import ToolingContext
from haylen.native.cmake_tree import CMakeTree
from haylen.native.native_library import NativeLibrary
from haylen.native.static_linking import StaticLinking
from haylen.native.xc_framework import XcFramework
from haylen.platforms.apple.apple_sdk_files import AppleSdkFiles
from haylen.platforms.apple.apple_target import AppleTarget
from haylen.toolchains.apple_sdk import AppleSdk


class AppleNativeLibraries:
    """Brings the native libraries of an app and its plugins to an Apple target: a prebuilt file, or the slice of a prebuilt xcframework, or a CMake project that it builds for every architecture of the product against the CMake package of the Apple SDK and joins with `lipo`. The app embeds a dynamic library, as a framework bundle on iOS, iPadOS, tvOS and visionOS, and links a static one, whose symbols that Lua reaches the registry of `HaylenNativeSymbols.mm` keeps and registers for `haylen.native` and `ffi.C`. Which libraries link statically follows the rule of `StaticLinking`, which the resolution already checked."""

    REGISTRY = "HaylenNativeSymbols.mm"
    # The platform that the bundle of a framework names for each SDK of the systems that load frameworks.
    SUPPORTED_PLATFORMS = {"iphoneos": "iPhoneOS", "iphonesimulator": "iPhoneSimulator", "appletvos": "AppleTVOS", "appletvsimulator": "AppleTVSimulator", "xros": "XROS", "xrsimulator": "XRSimulator"}

    @dataclass(frozen=True)
    class Placement:
        """The libraries that the app embeds and the ones it links, and the source of the registry of the static symbols when it links any."""

        embedded: tuple[Path, ...]
        linked: tuple[Path, ...]
        registry: str | None

    def __init__(self, context: ToolingContext, product: ResolvedProduct, target: AppleTarget, build: Path) -> None:
        self.context = context
        self.product = product
        self.target = target
        self.build = build

    def place(self) -> AppleNativeLibraries.Placement:
        embedded: list[Path] = []
        linked: list[Path] = []
        statics: list[NativeLibrary] = []
        capabilities = self.product.primary.sdk.slice.capabilities
        for library in self.product.primary.native_libraries:
            static = StaticLinking.links_statically(library, capabilities)
            built = self.cmake(library, static) if library.cmake is not None else self.prebuilt(library)
            if static:
                linked.append(built)
                statics.append(library)
            else:
                embedded.append(built)
        registry = AppleNativeLibraries.registry(statics) if statics else None
        return AppleNativeLibraries.Placement(tuple(embedded), tuple(linked), registry)

    def prebuilt(self, library: NativeLibrary) -> Path:
        """The prebuilt file of the platform, or the slice of the target in a prebuilt xcframework."""
        path = library.files[self.target.platform]
        if path.suffix != ".xcframework":
            return path
        library_path = XcFramework.library(path, self.target.xcframework_platform, self.target.xcframework_variant)
        if library_path is not None:
            return library_path
        raise UnsupportedTarget(
            f'The xcframework `{Repository.shown(path)}` of the native library "{library.name}" has no slice for "{self.target.name}". Add that slice to it, or leave "{self.target.platform}" out of the platforms of the library.'
        )

    def cmake(self, library: NativeLibrary, static: bool) -> Path:
        """Builds the target named after a library for every architecture of the product and joins them, wrapping a dynamic library of iOS, iPadOS, tvOS or visionOS into the framework bundle those systems load."""
        folder = self.build / "native" / library.name
        extension = "a" if static else "dylib"
        architectures = [project.target.architecture for project in self.product.projects]
        built = [self.compile(library, static, architecture, folder / architecture) / f"lib{library.name}.{extension}" for architecture in architectures]
        joined = folder / f"lib{library.name}.{extension}"
        self.context.process.run(["lipo", "-create", *built, "-output", joined])
        if static or self.target.runs_on_mac:
            return joined
        return self.framework(library, joined, folder / f"{library.name}.framework")

    def compile(self, library: NativeLibrary, static: bool, architecture: str, folder: Path) -> Path:
        output = folder / "out"
        tree = CMakeTree(self.context, folder / "build")
        command: list[object] = [
            "cmake",
            "-S",
            library.cmake,
            "-B",
            tree.folder,
            *CMakeTree.generator(),
            "-DCMAKE_BUILD_TYPE=Release",
            f"-DBUILD_SHARED_LIBS={'OFF' if static else 'ON'}",
            f"-Dhaylen_DIR={self.product.primary.sdk.file(AppleSdkFiles.CMAKE)}",
            *AppleSdk.cmake_options(self.target.slice, architecture),
        ]
        command += [f"-DCMAKE_{kind}_OUTPUT_DIRECTORY{suffix}={output}" for kind in ("LIBRARY", "ARCHIVE", "RUNTIME") for suffix in ("", "_RELEASE")]
        self.context.terminal.step(f'Building the native library "{library.name}" for "{self.target.name}" on "{architecture}"')
        tree.configure(command)
        tree.build([library.name], "Release")
        return output

    def framework(self, library: NativeLibrary, binary: Path, framework: Path) -> Path:
        app = self.product.app
        Folders.remove(framework)
        framework.mkdir(parents=True)
        (framework / library.name).write_bytes(binary.read_bytes())
        self.context.process.run(["install_name_tool", "-id", f"@rpath/{library.name}.framework/{library.name}", framework / library.name])
        info = {
            "CFBundleDevelopmentRegion": "en",
            "CFBundleExecutable": library.name,
            "CFBundleIdentifier": f"{app.identifier}.native.{library.name.replace('_', '-')}",
            "CFBundleInfoDictionaryVersion": "6.0",
            "CFBundleName": library.name,
            "CFBundlePackageType": "FMWK",
            "CFBundleShortVersionString": app.version,
            "CFBundleSupportedPlatforms": [AppleNativeLibraries.SUPPORTED_PLATFORMS[self.target.sdk]],
            "CFBundleVersion": str(app.build),
            "MinimumOSVersion": str(self.product.primary.minimum_system_version.value),
        }
        (framework / "Info.plist").write_bytes(plistlib.dumps(info, sort_keys=True))
        return framework

    @staticmethod
    def registry(libraries: list[NativeLibrary]) -> str:
        """The source that links the symbols of the static libraries into the app and registers them by library, so dead code stripping keeps what the table refers to and Lua finds the symbols without the app exporting them."""
        declarations = [f'extern "C" void {symbol}(void);' for library in libraries for symbol in library.symbols]
        registrations = []
        for library in libraries:
            entries = ", ".join(f'{{"{symbol}", reinterpret_cast<void*>(&{symbol})}}' for symbol in library.symbols)
            registrations.append(f'    haylen::platform::NativeLibraries::registerLinked("{library.name}", {{{entries}}});')
        lines = [
            "// Written by the Apple backend from the native libraries of `haylen.json` and its plugins that the app links statically.",
            "#import <Foundation/Foundation.h>",
            "",
            '#include "haylen/platform/NativeLibraries.hpp"',
            "",
            *declarations,
            "",
            "@interface HaylenNativeSymbols : NSObject",
            "@end",
            "",
            "@implementation HaylenNativeSymbols",
            "",
            "+ (void)load {",
            *registrations,
            "}",
            "",
            "@end",
            "",
        ]
        return "\n".join(lines)
