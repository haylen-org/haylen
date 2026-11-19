import re
from collections.abc import Iterable
from pathlib import Path

from haylen.core.errors.build_failed import BuildFailed
from haylen.core.tooling_context import ToolingContext


class AndroidLibraryCheck:
    """Checks that the player library of an ABI of the Android SDK needs exactly the system libraries that the engine lists for Android, from the entries `DT_NEEDED` that `llvm-readelf -d` of the NDK reads. The library links with `--as-needed`, so it records only the libraries whose symbols it uses, and a listed library that it does not need is one the engine links without using."""

    NEEDED = re.compile(r"\(NEEDED\)\s+Shared library: \[([^\]]+)\]")
    # The libraries that the clang driver of the NDK adds to the link of every shared library: the C library, the math library and the dynamic loader, which the engine needs without listing them.
    TOOLCHAIN = ("libc.so", "libm.so", "libdl.so")
    LIST = "HAYLEN_ANDROID_LIBRARIES"
    LIST_FILE = "engine/cmake/haylen-app.cmake"

    def __init__(self, context: ToolingContext) -> None:
        self.context = context

    def check(self, readelf: Path, library: Path, abi: str, listed: Iterable[str]) -> None:
        """Fails with the listed libraries that the library does not need and the libraries that it needs without a place in the list, named by their files, such as `libmediandk.so`."""
        needed = self.needed(readelf, library)
        files = [f"lib{name}.so" for name in listed]
        unused = [name for name in files if name not in needed]
        unlisted = [name for name in sorted(needed) if name not in files and name not in AndroidLibraryCheck.TOOLCHAIN]
        if unused:
            names = ", ".join(f'"{name}"' for name in unused)
            raise BuildFailed(
                f'The engine links {names} on Android, while `{library}` of "{abi}" uses none of their symbols. Remove them from "{AndroidLibraryCheck.LIST}" in `{AndroidLibraryCheck.LIST_FILE}` and from `docs/tools/distribution.md`.'
            )
        if unlisted:
            names = ", ".join(f'"{name}"' for name in unlisted)
            raise BuildFailed(
                f'The library `{library}` of "{abi}" needs {names}, which "{AndroidLibraryCheck.LIST}" in `{AndroidLibraryCheck.LIST_FILE}` does not list. List them there with the reason in `docs/tools/distribution.md`, or remove the code that uses them.'
            )

    def needed(self, readelf: Path, library: Path) -> set[str]:
        """The libraries that the entries `DT_NEEDED` of the dynamic section of the library name."""
        output = self.context.process.capture([readelf, "-d", library])
        return set(AndroidLibraryCheck.NEEDED.findall(output))
