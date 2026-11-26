import re
from pathlib import Path

from haylen.core.tooling_context import ToolingContext
from haylen.platforms.symbol_resolution import SymbolResolution


class DsymSymbols:
    """Resolves the function of the engine that the verification looks for in the dSYM of an Apple app with the tools of Xcode, as crash reports symbolicate it: `nm` finds its address in the symbol table that the dSYM keeps for each architecture, and `atos` its file and line in the debug information."""

    # A line of `nm` names the address, the kind and the demangled name of a symbol.
    SYMBOL = re.compile(r"^([0-9a-fA-F]+) [tT] (.+)$")
    # An answer of `atos` names the function, the image and the file with its line.
    ANSWER = re.compile(r"^(.+) \(in .+\) \((.+):(\d+)\)$")

    def __init__(self, context: ToolingContext) -> None:
        self.context = context

    def resolve(self, dsym: Path, executable: str, architecture: str) -> SymbolResolution | None:
        """Resolves the function in the debug file of the executable inside the dSYM, for one architecture of the app."""
        dwarf = dsym / "Contents" / "Resources" / "DWARF" / executable
        listing = self.context.process.capture(["xcrun", "nm", "-arch", architecture, "--defined-only", "-C", dwarf])
        addresses = [match[1] for line in listing.splitlines() if (match := DsymSymbols.SYMBOL.match(line.strip())) and SymbolResolution.names_function(match[2])]
        if not addresses:
            return None

        answer = self.context.process.capture(["xcrun", "atos", "-o", dwarf, "-arch", architecture, "--fullPath", f"0x{addresses[0]}"]).strip()
        found = DsymSymbols.ANSWER.match(answer)
        if found is None:
            return SymbolResolution(answer, "", 0)
        return SymbolResolution(found[1], found[2], int(found[3]))
