from collections.abc import Iterable

from haylen.core.errors.invalid_manifest import InvalidManifest
from haylen.native.native_library import NativeLibrary


class StaticLinking:
    """The one rule of the native libraries that link statically into an app, which every backend follows: a target whose SDK slice loads no dynamic library links every library statically, iOS and tvOS apps link the libraries that ask for it statically, which some SDKs require, and every other target loads its libraries dynamically. A library that links statically lists the symbols that Lua calls, which the app keeps."""

    # The platforms whose apps may choose to link a library statically while they load dynamic libraries too.
    CHOOSING_PLATFORMS = ("ios", "tvos")
    DYNAMIC_LIBRARIES = "dynamicLibraries"

    @staticmethod
    def links_statically(library: NativeLibrary, capabilities: Iterable[str]) -> bool:
        """Whether a library that ships to a target links statically, which every library does where the SDK slice loads no dynamic library."""
        return library.static or StaticLinking.DYNAMIC_LIBRARIES not in set(capabilities)

    @staticmethod
    def check(libraries: Iterable[NativeLibrary], platform: str, target_name: str, capabilities: Iterable[str]) -> None:
        """Fails on a library that links statically where the target loads it dynamically, and on a library without symbols where the target links every library statically."""
        loads_dynamic = StaticLinking.DYNAMIC_LIBRARIES in set(capabilities)
        for library in libraries:
            if library.static and loads_dynamic and platform not in StaticLinking.CHOOSING_PLATFORMS:
                raise InvalidManifest(
                    f'The native library "{library.name}" of "{library.owner}" links statically, which only the apps of iOS and tvOS and the apps of targets that load no dynamic library do, and the apps of "{target_name}" load it dynamically. Leave "{platform}" out of its platforms, or make it dynamic.'
                )
            if not loads_dynamic and not library.symbols:
                raise InvalidManifest(
                    f'The native library "{library.name}" of "{library.owner}" is dynamic, and the apps of "{target_name}" load no dynamic library. Link it statically with "link": "static" and list the symbols that Lua calls in "symbols".'
                )
