from dataclasses import dataclass
from pathlib import Path

from haylen.core.errors.invalid_manifest import InvalidManifest
from haylen.core.repository import Repository


@dataclass(frozen=True)
class CppSources:
    """The C++ code of a C++ app from the `cpp` section of `haylen.json`: its source files and folders, its include folders, its preprocessor definitions and whether it runs Lua, which links the Lua layer of the engine. The backend of each target compiles them once against the engine SDK."""

    SUFFIXES = frozenset({".c", ".cc", ".cpp", ".cxx", ".m", ".mm"})
    HEADER_SUFFIXES = frozenset({".h", ".hh", ".hpp", ".hxx", ".inl"})

    sources: tuple[Path, ...]
    include_directories: tuple[Path, ...]
    definitions: tuple[str, ...]
    lua: bool

    @staticmethod
    def parse(section: dict, folder: Path, origin: str) -> CppSources:
        sources = tuple(folder / str(path) for path in section["sources"])
        include_directories = tuple(folder / str(path) for path in section.get("includeDirectories", ()))
        for path in (*sources, *include_directories):
            if not path.exists():
                raise InvalidManifest(f'The "cpp" section of `{origin}` names `{Repository.shown(path)}`, which does not exist.', origin=origin)
        return CppSources(sources, include_directories, tuple(str(definition) for definition in section.get("definitions", ())), bool(section.get("lua", False)))

    def files(self) -> tuple[Path, ...]:
        """Every source file that compiles, in the order of their paths, from the files and the folders that the section names."""
        found: set[Path] = set()
        for path in self.sources:
            if path.is_file():
                found.add(path)
                continue
            found.update(file for file in path.rglob("*") if file.is_file() and file.suffix in CppSources.SUFFIXES)
        return tuple(sorted(found))

    def headers(self) -> tuple[Path, ...]:
        found: set[Path] = set()
        for path in (*self.sources, *self.include_directories):
            if path.is_dir():
                found.update(file for file in path.rglob("*") if file.is_file() and file.suffix in CppSources.HEADER_SUFFIXES)
        return tuple(sorted(found))
