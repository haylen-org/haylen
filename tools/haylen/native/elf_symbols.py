import io
import struct
from pathlib import Path

from elftools.common.exceptions import ELFError, ELFParseError
from elftools.dwarf.dwarfinfo import DWARFInfo
from elftools.elf.elffile import ELFFile
from elftools.elf.sections import SymbolTableSection

from haylen.core.errors.build_failed import BuildFailed
from haylen.core.repository import Repository
from haylen.platforms.symbol_resolution import SymbolResolution


class ElfSymbols:
    """Resolves the function of the engine that the verification looks for in an ELF file that keeps its symbol table and its line tables, such as the debug file of a Linux app or the library of an Android app as it links: the symbol table gives its address and the DWARF line program of the unit that covers it its file and line, read with pyelftools, as crash reporting tools of Python read them. The file is hostile input, so a damaged one fails with its name instead of resolving to a wrong line."""

    # The machine of 32-bit ARM, whose functions in Thumb code mark their addresses with the lowest bit.
    ARM = "EM_ARM"
    # The sections that only an unstripped file holds: its symbol table and its debug information.
    UNSTRIPPED = (".symtab", ".debug_")

    @staticmethod
    def unstripped_sections(data: bytes, origin: str) -> list[str]:
        """The sections of a binary that a stripped release no longer holds, its symbol table and its debug information, and fails with its origin when the bytes are no ELF file."""
        try:
            return [section.name for section in ELFFile(io.BytesIO(data)).iter_sections() if section.name.startswith(ElfSymbols.UNSTRIPPED)]
        except (ELFError, ELFParseError, IndexError, KeyError, ValueError, struct.error) as error:
            raise BuildFailed(f"The binary {origin} is damaged or no ELF file. Build the app again.") from error

    def resolve(self, file: Path) -> SymbolResolution | None:
        try:
            elf = ELFFile(io.BytesIO(file.read_bytes()))
            found = ElfSymbols.function(elf)
            if found is None:
                return None
            name, address = found
            location = ElfSymbols.locate(elf.get_dwarf_info(), address) if elf.has_dwarf_info() else None
        except (ELFError, ELFParseError, IndexError, KeyError, ValueError, struct.error) as error:
            raise BuildFailed(f"The symbols `{Repository.shown(file)}` are damaged. Build the app again.") from error
        if location is None:
            return SymbolResolution(name, "", 0)
        return SymbolResolution(name, *location)

    @staticmethod
    def function(elf: ELFFile) -> tuple[str, int] | None:
        """The name and the address of the function of the symbol table that the verification resolves. A part that the compiler splits off a function, such as its cold code, carries the name of the function with a suffix after a dot, and the address of a function in Thumb code drops the bit that marks it."""
        table = elf.get_section_by_name(".symtab")
        if not isinstance(table, SymbolTableSection):
            return None
        for symbol in table.iter_symbols():
            if symbol["st_info"]["type"] != "STT_FUNC" or "." in symbol.name or not SymbolResolution.names_function(symbol.name):
                continue
            address = symbol["st_value"]
            return symbol.name, address & ~1 if elf["e_machine"] == ElfSymbols.ARM else address
        return None

    @staticmethod
    def covers(dwarf: DWARFInfo, unit, address: int) -> bool:
        """Whether a compilation unit covers the address, by the range of its top entry or the list of its ranges."""
        top = unit.get_top_DIE()
        attributes = top.attributes
        if "DW_AT_ranges" in attributes:
            ranges = dwarf.range_lists().get_range_list_at_offset(attributes["DW_AT_ranges"].value, cu=unit)
            base = attributes["DW_AT_low_pc"].value if "DW_AT_low_pc" in attributes else 0
            for entry in ranges:
                if hasattr(entry, "base_address"):
                    base = entry.base_address
                    continue
                start, end = (entry.begin_offset, entry.end_offset) if entry.is_absolute else (base + entry.begin_offset, base + entry.end_offset)
                if start <= address < end:
                    return True
            return False
        if "DW_AT_low_pc" in attributes and "DW_AT_high_pc" in attributes:
            low = attributes["DW_AT_low_pc"].value
            high = attributes["DW_AT_high_pc"]
            end = high.value if high.form == "DW_FORM_addr" else low + high.value
            return low <= address < end
        return False

    @staticmethod
    def locate(dwarf: DWARFInfo, address: int) -> tuple[str, int] | None:
        """The file and the line of the last row at or before the address within the sequence of the line program that holds it."""
        for unit in dwarf.iter_CUs():
            if not ElfSymbols.covers(dwarf, unit, address):
                continue
            program = dwarf.line_program_for_CU(unit)
            if program is None:
                continue
            found = ElfSymbols.row(program, address)
            if found is not None:
                return found
        return None

    @staticmethod
    def row(program, address: int) -> tuple[str, int] | None:
        previous = None
        for entry in program.get_entries():
            state = entry.state
            if state is None:
                continue
            if previous is not None and previous.address <= address < state.address:
                return ElfSymbols.file_name(program, previous.file), previous.line
            previous = None if state.end_sequence else state
        return None

    @staticmethod
    def file_name(program, index: int) -> str:
        """The path of a file of a line program, joined with its directory. Files count from 0 in DWARF 5 and from 1 before it."""
        header = program.header
        entries = header["file_entry"]
        position = index if header["version"] >= 5 else index - 1
        if not 0 <= position < len(entries):
            return ""
        entry = entries[position]
        name = entry.name.decode("utf-8", "replace")
        directories = header["include_directory"]
        directory = entry.dir_index if header["version"] >= 5 else entry.dir_index - 1
        if name.startswith("/") or not 0 <= directory < len(directories):
            return name
        return f"{directories[directory].decode('utf-8', 'replace').rstrip('/')}/{name}"
