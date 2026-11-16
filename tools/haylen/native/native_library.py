from collections.abc import Mapping
from dataclasses import dataclass
from pathlib import Path
from types import MappingProxyType

from haylen.core.errors.invalid_manifest import InvalidManifest
from haylen.core.repository import Repository


@dataclass(frozen=True)
class NativeLibrary:
    """A native library that Lua loads with `native.load`: prebuilt files by platform, or a CMake project that the backend of each listed platform builds once for its target. A static library lists the symbols that Lua reaches, which the app keeps when it links them."""

    name: str
    owner: str
    files: Mapping[str, Path]
    cmake: Path | None
    platforms: tuple[str, ...]
    static: bool
    symbols: tuple[str, ...]

    @staticmethod
    def parse(name: str, entry: Mapping[str, object], folder: Path, owner: str, origin: str) -> NativeLibrary:
        """Reads a library whose entry follows the schema of the `native` section, with its paths relative to `folder`, and checks that its files exist."""
        files = {platform: folder / str(path) for platform, path in dict(entry.get("files", {})).items()}
        cmake = folder / str(entry["cmake"]) if "cmake" in entry else None
        platforms = tuple(str(platform) for platform in entry.get("platforms", tuple(files)))
        static = entry.get("link", "dynamic") == "static"
        symbols = tuple(str(symbol) for symbol in entry.get("symbols", ()))

        where = f'The native library "{name}" of `{origin}`'
        for path in files.values():
            if not path.exists():
                raise InvalidManifest(f"{where} lists `{Repository.shown(path)}`, which does not exist.", origin=origin)
        if cmake is not None and not (cmake / "CMakeLists.txt").is_file():
            raise InvalidManifest(f'{where} names the CMake project `{Repository.shown(cmake)}`, which has no "CMakeLists.txt".', origin=origin)
        if static and not symbols:
            raise InvalidManifest(f'{where} links statically, so it lists the symbols that Lua calls in "symbols".', origin=origin)
        return NativeLibrary(name, owner, MappingProxyType(files), cmake, platforms, static, symbols)

    def ships_to(self, platform: str) -> bool:
        return platform in self.platforms
