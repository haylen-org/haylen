import ctypes
from pathlib import Path

from haylen.core.errors.build_failed import BuildFailed
from haylen.core.repository import Repository
from haylen.platforms.symbol_resolution import SymbolResolution


class PdbSymbols:
    """Resolves the function of the engine that the verification looks for in the PDB of a Windows app with DbgHelp, the symbol library of Windows that debuggers and crash reports read PDB files with: the function by its name, then the file and the line of its address. It runs only on Windows, where DbgHelp is part of the system."""

    # The options of DbgHelp: undecorated names, line numbers, and failures as results instead of dialogs.
    OPTIONS = 0x2 | 0x10 | 0x200
    # A PDB loads as a module at an address and with a size of its own choice, since no image is loaded.
    BASE = 0x10000000
    SIZE = 0x40000000
    NAME_LENGTH = 1024
    # The handle that names the session of DbgHelp, which needs no process when it reads files.
    SESSION = 0x4859

    class Symbol(ctypes.Structure):
        _fields_ = [
            ("SizeOfStruct", ctypes.c_ulong),
            ("TypeIndex", ctypes.c_ulong),
            ("Reserved", ctypes.c_ulonglong * 2),
            ("Index", ctypes.c_ulong),
            ("Size", ctypes.c_ulong),
            ("ModBase", ctypes.c_ulonglong),
            ("Flags", ctypes.c_ulong),
            ("Value", ctypes.c_ulonglong),
            ("Address", ctypes.c_ulonglong),
            ("Register", ctypes.c_ulong),
            ("Scope", ctypes.c_ulong),
            ("Tag", ctypes.c_ulong),
            ("NameLen", ctypes.c_ulong),
            ("MaxNameLen", ctypes.c_ulong),
            ("Name", ctypes.c_wchar * 1),
        ]

    class Line(ctypes.Structure):
        _fields_ = [
            ("SizeOfStruct", ctypes.c_ulong),
            ("Key", ctypes.c_void_p),
            ("LineNumber", ctypes.c_ulong),
            ("FileName", ctypes.c_wchar_p),
            ("Address", ctypes.c_ulonglong),
        ]

    def resolve(self, pdb: Path) -> SymbolResolution | None:
        library = ctypes.WinDLL("dbghelp", use_last_error=True)
        library.SymInitializeW.argtypes = [ctypes.c_void_p, ctypes.c_wchar_p, ctypes.c_bool]
        library.SymCleanup.argtypes = [ctypes.c_void_p]
        library.SymLoadModuleExW.restype = ctypes.c_ulonglong
        library.SymLoadModuleExW.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_wchar_p, ctypes.c_wchar_p, ctypes.c_ulonglong, ctypes.c_ulong, ctypes.c_void_p, ctypes.c_ulong]
        library.SymFromNameW.argtypes = [ctypes.c_void_p, ctypes.c_wchar_p, ctypes.POINTER(PdbSymbols.Symbol)]
        library.SymGetLineFromAddrW64.argtypes = [ctypes.c_void_p, ctypes.c_ulonglong, ctypes.POINTER(ctypes.c_ulong), ctypes.POINTER(PdbSymbols.Line)]
        session = ctypes.c_void_p(PdbSymbols.SESSION)
        library.SymSetOptions(PdbSymbols.OPTIONS)
        if not library.SymInitializeW(session, None, False):
            raise BuildFailed(f"DbgHelp of Windows does not start, with error {ctypes.get_last_error()}, so the symbols of the app cannot be checked. Run the command again.")
        try:
            return self.find(library, session, pdb)
        finally:
            library.SymCleanup(session)

    def find(self, library: ctypes.CDLL, session: ctypes.c_void_p, pdb: Path) -> SymbolResolution | None:
        if not library.SymLoadModuleExW(session, None, str(pdb), None, PdbSymbols.BASE, PdbSymbols.SIZE, None, 0):
            raise BuildFailed(f"DbgHelp of Windows does not read `{Repository.shown(pdb)}`, with error {ctypes.get_last_error()}. Build the app again.")

        buffer = ctypes.create_string_buffer(ctypes.sizeof(PdbSymbols.Symbol) + PdbSymbols.NAME_LENGTH * ctypes.sizeof(ctypes.c_wchar))
        symbol = PdbSymbols.Symbol.from_buffer(buffer)
        symbol.SizeOfStruct = ctypes.sizeof(PdbSymbols.Symbol)
        symbol.MaxNameLen = PdbSymbols.NAME_LENGTH
        if not library.SymFromNameW(session, SymbolResolution.FUNCTION, ctypes.pointer(symbol)):
            return None
        name = ctypes.wstring_at(ctypes.addressof(symbol) + PdbSymbols.Symbol.Name.offset, symbol.NameLen)

        line = PdbSymbols.Line()
        line.SizeOfStruct = ctypes.sizeof(PdbSymbols.Line)
        displacement = ctypes.c_ulong()
        if not library.SymGetLineFromAddrW64(session, symbol.Address, ctypes.byref(displacement), ctypes.byref(line)):
            return SymbolResolution(name, "", 0)
        return SymbolResolution(name, line.FileName, line.LineNumber)
