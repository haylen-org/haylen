import struct

from haylen.core.errors.build_failed import BuildFailed


class ElfLibrary:
    """Reads the symbols that an ELF shared library defines in its dynamic symbol table, the ones that the dynamic linker and the `System.loadLibrary` of Java find, which a stripped release library keeps."""

    MAGIC = b"\x7fELF"
    # The class of a file in its identification: 32-bit or 64-bit.
    CLASSES = {1: "32", 2: "64"}
    # The byte order of a file in its identification, as `struct` writes it.
    BYTE_ORDERS = {1: "<", 2: ">"}
    DYNAMIC_SYMBOLS = 11
    UNDEFINED = 0
    # For each class, the layout of the fields of the file header from the offset of the section headers on, with the place of that offset, the layout of a section header and the layout of a symbol, with the place of the index of its section.
    HEADER = {"32": ("IIHHHHH", 32), "64": ("QIHHHHH", 40)}
    SECTION = {"32": "IIIIIIIIII", "64": "IIQQQQIIQQ"}
    SYMBOL = {"32": ("IIIBBH", 5), "64": ("IBBHQQ", 3)}

    @staticmethod
    def defined_symbols(data: bytes, origin: str) -> set[str]:
        """The names of the symbols that the library defines, and fails with its origin when the bytes are no ELF library."""
        if data[:4] != ElfLibrary.MAGIC or data[4] not in ElfLibrary.CLASSES or data[5] not in ElfLibrary.BYTE_ORDERS:
            raise BuildFailed(f"The library {origin} is no ELF file. Build the app again.")
        try:
            return ElfLibrary.parse(data, ElfLibrary.CLASSES[data[4]], ElfLibrary.BYTE_ORDERS[data[5]])
        except (struct.error, IndexError, ValueError) as error:
            raise BuildFailed(f"The library {origin} ends early or points past its end. Build the app again.") from error

    @staticmethod
    def parse(data: bytes, width: str, order: str) -> set[str]:
        layout, position = ElfLibrary.HEADER[width]
        section_offset, _, _, _, _, section_size, section_count = struct.unpack_from(order + layout, data, position)
        sections = [struct.unpack_from(order + ElfLibrary.SECTION[width], data, section_offset + index * section_size) for index in range(section_count)]

        symbol_layout, section_field = ElfLibrary.SYMBOL[width]
        symbol_format = order + symbol_layout
        symbol_size = struct.calcsize(symbol_format)
        symbols: set[str] = set()
        for section in sections:
            _, kind, _, _, offset, size, link, *_ = section
            if kind != ElfLibrary.DYNAMIC_SYMBOLS:
                continue

            names = sections[link][4]
            for entry in range(offset, offset + size, symbol_size):
                symbol = struct.unpack_from(symbol_format, data, entry)
                if symbol[section_field] == ElfLibrary.UNDEFINED or symbol[0] == 0:
                    continue
                start = names + symbol[0]
                end = data.index(b"\0", start)
                symbols.add(data[start:end].decode("utf-8"))
        return symbols
