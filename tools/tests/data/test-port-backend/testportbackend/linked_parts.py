from dataclasses import dataclass
from pathlib import Path, PurePosixPath

from haylen.core.errors.invalid_contribution import InvalidContribution
from haylen.core.repository import Repository
from haylen.core.resolved_project import ResolvedProject


@dataclass(frozen=True)
class LinkedParts:
    """The parts of the plugins of an app for the test port, the contributions of the type `testPortPart`: the C++ sources of each plugin that the executable of the app compiles in, by their path inside the plugin folder, and the C functions that Lua reaches through `native.load` with the id of the plugin, which the source of the registration registers before `main` runs."""

    TYPE = "testPortPart"
    SCHEMA = {
        "type": "object",
        "additionalProperties": False,
        "required": ["sources", "symbols"],
        "properties": {
            "sources": {
                "type": "array",
                "minItems": 1,
                "uniqueItems": True,
                "items": {"type": "string", "pattern": "\\.(c|cc|cpp)$"},
                "description": 'The C and C++ sources of the part, relative to the plugin folder, such as "test-port/Greeter.cpp".',
            },
            "symbols": {
                "type": "array",
                "minItems": 1,
                "uniqueItems": True,
                "items": {"type": "string", "pattern": "^[A-Za-z_][A-Za-z0-9_]*$"},
                "description": "The C functions of the part that Lua calls, which the app registers under the id of the plugin.",
            },
        },
    }
    REGISTRATION = "LinkedParts.cpp"

    @dataclass(frozen=True)
    class Part:
        """The plugin folder, the sources relative to it and the symbols that one plugin links into the app."""

        owner: str
        folder: Path
        sources: tuple[str, ...]
        symbols: tuple[str, ...]

    parts: tuple[LinkedParts.Part, ...]

    @staticmethod
    def of(project: ResolvedProject) -> LinkedParts:
        """Gathers the parts of every plugin of a project, which accumulate, so each merged part has one owner, and refuses a source outside its plugin folder or one that does not exist."""
        folders: dict[str, Path] = {}
        sources: dict[str, list[str]] = {}
        symbols: dict[str, list[str]] = {}
        for contribution in project.of_type(LinkedParts.TYPE):
            owner = contribution.owners[0]
            folder = contribution.folders[0]
            for source in contribution.field("sources"):
                relative = PurePosixPath(source)
                path = folder / relative
                if relative.is_absolute() or ".." in relative.parts or not path.is_file():
                    raise InvalidContribution(
                        f'The part "{LinkedParts.TYPE}" of `{contribution.origins[0]}` names the source "{source}", which is no file inside `{Repository.shown(folder)}`.',
                        origin=contribution.origins[0],
                    )
                sources.setdefault(owner, []).append(relative.as_posix())
            folders[owner] = folder
            symbols.setdefault(owner, []).extend(contribution.field("symbols"))
        parts = tuple(LinkedParts.Part(owner, folders[owner], tuple(sources[owner]), tuple(symbols[owner])) for owner in sources)
        return LinkedParts(parts)

    def registration(self) -> str:
        """The C++ source that registers the symbols of every part under the id of its plugin when the executable starts, so the app keeps them and Lua finds them without the executable exporting them."""
        declarations = [f'extern "C" void {symbol}(void);' for part in self.parts for symbol in part.symbols]
        calls = []
        for part in self.parts:
            entries = ", ".join(f'{{"{symbol}", reinterpret_cast<void*>(&{symbol})}}' for symbol in part.symbols)
            calls.append(f'        haylen::platform::NativeLibraries::registerLinked("{part.owner}", {{{entries}}});')
        lines = [
            "// Written by the backend of the test port from the parts of the plugins that the app links.",
            '#include "haylen/platform/NativeLibraries.hpp"',
            "",
            *declarations,
            "",
            "class LinkedParts final {",
            "    static const bool registered;",
            "",
            "    static bool registerParts() {",
            *calls,
            "        return true;",
            "    }",
            "};",
            "",
            "const bool LinkedParts::registered = LinkedParts::registerParts();",
            "",
        ]
        return "\n".join(lines)
