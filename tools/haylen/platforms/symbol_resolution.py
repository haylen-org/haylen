from dataclasses import dataclass
from pathlib import Path

from haylen.core.repository import Repository
from haylen.platforms.verification_check import VerificationCheck


@dataclass(frozen=True)
class SymbolResolution:
    """Where the symbol files of a Release build place an address of the engine: the function, the source file and the line, as a crash report of the app shows a frame of the engine. The verification resolves the start of `haylen::core::Engine::frame`, through which every frame of an app passes, so a release whose symbols lack the line tables of the engine SDK fails."""

    FUNCTION = "haylen::core::Engine::frame"
    # The same function in the symbol tables of ELF files, by the start of its mangled name, which its parameters follow.
    MANGLED = "_ZN6haylen4core6Engine5frameE"
    FILE = "src/core/Engine.cpp"
    CHECK = "symbols"

    function: str
    file: str
    line: int

    @staticmethod
    def names_function(name: str) -> bool:
        """Whether a demangled, undecorated or mangled name is the function of the engine that the verification resolves, with or without its parameters."""
        return name == SymbolResolution.FUNCTION or name.startswith((f"{SymbolResolution.FUNCTION}(", SymbolResolution.MANGLED))

    def matches(self) -> bool:
        return SymbolResolution.names_function(self.function) and self.file.replace("\\", "/").endswith(SymbolResolution.FILE) and self.line > 0

    @staticmethod
    def verify(found: SymbolResolution | None, symbols: Path, source: str) -> VerificationCheck:
        """The check of the symbols of a release that resolved the function of the engine to what was found, or to nothing."""
        shown = Repository.shown(symbols)
        rebuild = 'Build the engine SDK again with "haylen sdk build" and then the app.'
        if found is None:
            return VerificationCheck.failed(
                SymbolResolution.CHECK, f'The symbols `{shown}` hold no function "{SymbolResolution.FUNCTION}" of the engine, so crash reports would show no frame of the engine. {rebuild}', source
            )
        if not found.matches():
            detail = f'The symbols `{shown}` resolve "{SymbolResolution.FUNCTION}" to "{found.function}" at line {found.line} of "{found.file}", while it lies in "{SymbolResolution.FILE}", so crash reports would show the frames of the engine wrong. {rebuild}'
            return VerificationCheck.failed(SymbolResolution.CHECK, detail, source)
        return VerificationCheck.passed(SymbolResolution.CHECK, f'The symbols `{shown}` resolve an address of the engine to "{found.function}" at line {found.line} of `{found.file}`.', source)
