import json
from collections.abc import Mapping
from dataclasses import dataclass
from pathlib import Path

from haylen.core.app_category import AppCategory
from haylen.core.app_kind import AppKind
from haylen.core.cpp_sources import CppSources
from haylen.core.errors.invalid_manifest import InvalidManifest
from haylen.core.frozen import Frozen
from haylen.core.repository import Repository
from haylen.core.schema_validator import SchemaValidator
from haylen.native.native_library import NativeLibrary
from haylen.plugins.plugin_reference import PluginReference


@dataclass(frozen=True)
class AppManifest:
    """The `haylen.json` of an app, read and checked once against its schema: the identity of the app, its category, its kind, its C++ code, its native libraries, its plugins and the sections of its generated projects. The engine reads the same file from the package, and the tooling never writes another manifest next to it."""

    FILE = "haylen.json"
    # The name and the version of an app that names none, the same ones that the engine reads from such a `haylen.json`, and the name in the one-word form of executables, files and packages.
    DEFAULT_NAME = "Haylen App"
    DEFAULT_SLUG = DEFAULT_NAME.lower().replace(" ", "-")
    DEFAULT_VERSION = "1.0.0"

    folder: Path
    document: Mapping[str, object]
    name: str
    identifier: str
    version: str
    build: int
    category: AppCategory | None
    kind: AppKind
    cpp: CppSources | None
    native: tuple[NativeLibrary, ...]
    plugins: tuple[PluginReference, ...]
    platforms: Mapping[str, Mapping[str, object]]

    @staticmethod
    def is_app(folder: Path) -> bool:
        return (folder / AppManifest.FILE).is_file()

    @staticmethod
    def load(folder: Path) -> AppManifest:
        """Reads `haylen.json` of an app folder and fails with every problem it has, each with its key."""
        path = folder / AppManifest.FILE
        origin = Repository.shown(path)
        if not path.is_file():
            raise InvalidManifest(f'The folder `{Repository.shown(folder)}` is not an app, because it holds no "{AppManifest.FILE}".', origin=origin)
        try:
            document = json.loads(path.read_text(encoding="utf-8"))
        except json.JSONDecodeError as error:
            raise InvalidManifest(f"The file `{origin}` is not JSON: {error.msg} at line {error.lineno}, column {error.colno}.", origin=origin) from None

        SchemaValidator.of("haylen.schema.json").require(document, origin)
        kind = AppKind(document.get("kind", AppKind.LUA.value))
        if kind is AppKind.CPP and "cpp" not in document:
            raise InvalidManifest(f'The file `{origin}` describes a C++ app, so it names its sources in the "cpp" section.', origin=origin)
        if kind is AppKind.LUA and "cpp" in document:
            raise InvalidManifest(f'The file `{origin}` has a "cpp" section, which only C++ apps have. Set "kind" to "cpp", or remove the section.', origin=origin)

        logo = document.get("splash", {}).get("logo")
        if logo and not (folder / "content" / logo).is_file():
            raise InvalidManifest(
                f'The "splash.logo" of `{origin}` names `{Repository.shown(folder / "content" / logo)}`, which does not exist. Give it the path of an image relative to "content".',
                origin=origin,
            )

        cpp = CppSources.parse(document["cpp"], folder, origin) if "cpp" in document else None
        if kind is AppKind.CPP and not cpp.lua and (folder / "source").exists():
            raise InvalidManifest(
                f'The C++ app `{origin}` runs no Lua, so nothing runs the Lua modules of `{Repository.shown(folder / "source")}`. Set "cpp.lua" to true to run them, or remove the folder.',
                origin=origin,
            )
        native = tuple(NativeLibrary.parse(name, entry, folder, "app", origin) for name, entry in document.get("native", {}).items())
        plugins = tuple(PluginReference.parse(identifier, entry) for identifier, entry in document.get("plugins", {}).items())
        return AppManifest(
            folder=folder.resolve(),
            document=Frozen.freeze(document),
            name=document.get("name") or AppManifest.DEFAULT_NAME,
            identifier=document["identifier"],
            version=document.get("version", AppManifest.DEFAULT_VERSION),
            build=document.get("build", 1),
            category=AppCategory(document["category"]) if "category" in document else None,
            kind=kind,
            cpp=cpp,
            native=native,
            plugins=plugins,
            platforms=Frozen.freeze(document.get("platforms", {})),
        )

    @property
    def runs_lua(self) -> bool:
        """Whether the app runs Lua and links the Lua layer: every Lua app, and a C++ app whose "cpp.lua" is true."""
        return self.kind is AppKind.LUA or self.cpp.lua

    @property
    def path(self) -> Path:
        return self.folder / AppManifest.FILE

    @property
    def origin(self) -> str:
        return Repository.shown(self.path)

    def section(self, name: str) -> Mapping[str, object]:
        """Returns a section of the document, such as `window`, or an empty one."""
        value = self.document.get(name)
        return value if isinstance(value, Mapping) else Frozen.freeze({})
