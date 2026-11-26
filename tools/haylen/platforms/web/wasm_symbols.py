import json
from pathlib import Path

from haylen.core.errors.build_failed import BuildFailed
from haylen.core.repository import Repository
from haylen.platforms.symbol_resolution import SymbolResolution


class WasmSymbols:
    """Reads the WebAssembly module of a site with the symbols that its Release build keeps beside it: the symbol map of Emscripten, which names each function by its index, and the source map, which names the file and the line of each offset of the code. A stack trace of a browser names a frame by the index of its function and its offset in the module, so these files resolve it as the verification resolves the function of the engine. The module itself holds neither a name section nor debug information, nor the address of its source map."""

    MAGIC = b"\0asm"
    CUSTOM = 0
    IMPORTS = 2
    CODE = 10
    IMPORTED_FUNCTION = 0
    # The sections that only a module with its symbols holds: debug information, the names of its functions and the address of its source map.
    UNSTRIPPED = (".debug_", "name", "sourceMappingURL")
    BASE64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
    # The numbers of a module and of its source map fit in 64 bits, so a longer one is damage.
    NUMBER_BITS = 64

    def __init__(self, module: Path) -> None:
        self.module = module
        self.data = module.read_bytes()
        if self.data[:4] != WasmSymbols.MAGIC:
            raise BuildFailed(f"The module `{Repository.shown(module)}` is no WebAssembly module. Build the app again.")
        try:
            self.sections = self.read_sections()
        except (IndexError, ValueError) as error:
            raise BuildFailed(f"The module `{Repository.shown(module)}` is damaged. Build the app again.") from error

    def leb(self, position: int) -> tuple[int, int]:
        """Reads an unsigned LEB128 number and returns it with the position after it."""
        value = 0
        shift = 0
        while True:
            if shift > WasmSymbols.NUMBER_BITS:
                raise ValueError("A number of the module is longer than 64 bits.")
            byte = self.data[position]
            position += 1
            value |= (byte & 0x7F) << shift
            shift += 7
            if byte < 0x80:
                return value, position

    def name(self, position: int) -> tuple[str, int]:
        size, position = self.leb(position)
        return self.data[position : position + size].decode("utf-8"), position + size

    def read_sections(self) -> list[tuple[int, int, int, str]]:
        """The sections of the module as their id, the start and the end of their content, and the name of a custom section."""
        sections = []
        position = 8
        while position < len(self.data):
            kind = self.data[position]
            size, start = self.leb(position + 1)
            end = start + size
            if end > len(self.data):
                raise ValueError("A section of the module reaches past its end.")
            name = self.name(start)[0] if kind == WasmSymbols.CUSTOM else ""
            sections.append((kind, start, end, name))
            position = end
        return sections

    def unstripped_sections(self) -> list[str]:
        return [name for kind, _, _, name in self.sections if kind == WasmSymbols.CUSTOM and name.startswith(WasmSymbols.UNSTRIPPED)]

    def imported_functions(self) -> int:
        count = 0
        for kind, start, _, _ in self.sections:
            if kind != WasmSymbols.IMPORTS:
                continue
            entries, position = self.leb(start)
            for _ in range(entries):
                _, position = self.name(position)
                _, position = self.name(position)
                description = self.data[position]
                position = self.skip_import(description, position + 1)
                count += description == WasmSymbols.IMPORTED_FUNCTION
        return count

    def skip_import(self, description: int, position: int) -> int:
        """Skips the description of an import: the type of a function, the limits of a table or a memory, the type of a global or the type of a tag."""
        if description == 0:
            return self.leb(position)[1]
        if description == 1:
            return self.skip_limits(position + 1)
        if description == 2:
            return self.skip_limits(position)
        if description == 3:
            return position + 2
        if description == 4:
            return self.leb(position + 1)[1]
        raise ValueError("An import of the module has a kind that WebAssembly does not define.")

    def skip_limits(self, position: int) -> int:
        flags = self.data[position]
        position = self.leb(position + 1)[1]
        return self.leb(position)[1] if flags & 1 else position

    def body(self, index: int) -> tuple[int, int] | None:
        """The offsets in the module where the body of a defined function starts and ends."""
        for kind, start, _, _ in self.sections:
            if kind != WasmSymbols.CODE:
                continue
            count, position = self.leb(start)
            if index >= count:
                return None
            for _ in range(index):
                size, position = self.leb(position)
                position += size
            size, position = self.leb(position)
            return position, position + size
        return None

    @staticmethod
    def vlq(text: str, position: int) -> tuple[int, int]:
        """Reads a number of a source map in base64 VLQ and returns it with the position after it."""
        value = 0
        shift = 0
        while True:
            if shift > WasmSymbols.NUMBER_BITS:
                raise ValueError("A number of the source map is longer than 64 bits.")
            digit = WasmSymbols.BASE64.index(text[position])
            position += 1
            value |= (digit & 31) << shift
            shift += 5
            if not digit & 32:
                return (-(value >> 1) if value & 1 else value >> 1), position

    @staticmethod
    def locations(mappings: str) -> list[tuple[int, int, int]]:
        """The offsets of the module that a source map maps, each with the index of its source and its line counted from 0, in the order of the map."""
        found = []
        offset = source = line = 0
        for segment in mappings.split(","):
            if not segment:
                continue
            fields = []
            position = 0
            while position < len(segment):
                value, position = WasmSymbols.vlq(segment, position)
                fields.append(value)
            offset += fields[0]
            if len(fields) >= 4:
                source += fields[1]
                line += fields[2]
                found.append((offset, source, line))
        return found

    def resolve(self, symbol_map: Path, source_map: Path) -> SymbolResolution | None:
        """Resolves the function of the engine through the symbol map and the source map, which the reader treats as hostile, so a damaged file fails with its name instead of resolving to something wrong."""
        try:
            return self.find(symbol_map, source_map)
        except (IndexError, KeyError, TypeError, ValueError) as error:
            raise BuildFailed(f"The symbols `{Repository.shown(symbol_map.parent)}` or the module `{Repository.shown(self.module)}` are damaged. Build the app again.") from error

    def find(self, symbol_map: Path, source_map: Path) -> SymbolResolution | None:
        names = {}
        for line in symbol_map.read_text(encoding="utf-8").splitlines():
            index, _, name = line.partition(":")
            names[name] = int(index)
        function = next((name for name in names if SymbolResolution.names_function(name)), None)
        if function is None:
            return None

        imported = self.imported_functions()
        body = self.body(names[function] - imported) if names[function] >= imported else None
        if body is None:
            return SymbolResolution(function, "", 0)
        document = json.loads(source_map.read_text(encoding="utf-8"))
        sources = document["sources"]
        inside = [(source, line) for offset, source, line in WasmSymbols.locations(document["mappings"]) if body[0] <= offset < body[1]]
        if not inside or not 0 <= inside[0][0] < len(sources) or inside[0][1] < 0:
            return SymbolResolution(function, "", 0)
        source, line = inside[0]
        return SymbolResolution(function, str(sources[source]), line + 1)
