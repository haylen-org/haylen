import io
import struct
import zipfile
from collections.abc import Mapping, Sequence
from pathlib import Path


class NativeBinaries:
    """Writes the smallest binaries that the verification of Android apps reads: a class file with native methods, an Android library that holds such classes, and an ELF shared library whose dynamic symbol table defines some symbols and needs others."""

    NATIVE = 0x0100
    PUBLIC = 0x0001
    DYNAMIC_SYMBOLS = 11
    STRINGS = 3
    # A constant of a long, which takes two entries of the constant pool, and an attribute, so the reader skips both.
    LONG = 5

    @staticmethod
    def constant(pool: list[bytes], entry: bytes) -> int:
        """Adds a constant to a pool and returns its index, which counts from one."""
        pool.append(entry)
        return len(pool)

    @staticmethod
    def text(pool: list[bytes], value: str) -> int:
        encoded = value.encode("utf-8")
        return NativeBinaries.constant(pool, b"\x01" + struct.pack(">H", len(encoded)) + encoded)

    @staticmethod
    def class_file(name: str, methods: Sequence[tuple[str, str, bool]]) -> bytes:
        """A class of the binary name `name` with a field and methods of a name, a descriptor and whether they are native, whose pool also holds a long, which takes two entries."""
        pool: list[bytes] = []
        this_class = NativeBinaries.constant(pool, b"\x07" + struct.pack(">H", NativeBinaries.text(pool, name)))
        NativeBinaries.constant(pool, bytes([NativeBinaries.LONG]) + struct.pack(">q", 1))
        pool.append(b"")
        attribute = NativeBinaries.text(pool, "Code")
        field = struct.pack(">HHHH", NativeBinaries.PUBLIC, NativeBinaries.text(pool, "count"), NativeBinaries.text(pool, "I"), 1) + struct.pack(">HI", attribute, 2) + bytes(2)

        encoded = []
        for method, descriptor, native in methods:
            flags = NativeBinaries.PUBLIC | (NativeBinaries.NATIVE if native else 0)
            body = b"" if native else struct.pack(">HI", attribute, 2) + bytes(2)
            encoded.append(struct.pack(">HHHH", flags, NativeBinaries.text(pool, method), NativeBinaries.text(pool, descriptor), 0 if native else 1) + body)

        header = bytes.fromhex("cafebabe") + struct.pack(">HHH", 0, 65, len(pool) + 1)
        members = struct.pack(">HHHH", NativeBinaries.PUBLIC, this_class, 0, 0) + struct.pack(">H", 1) + field + struct.pack(">H", len(methods)) + b"".join(encoded)
        return header + b"".join(pool) + members + struct.pack(">H", 0)

    @staticmethod
    def library(path: Path, classes: Mapping[str, bytes]) -> Path:
        """An Android library whose `classes.jar` holds the classes by their binary names."""
        jar = io.BytesIO()
        with zipfile.ZipFile(jar, "w") as archive:
            for name, data in classes.items():
                archive.writestr(f"{name}.class", data)
        path.parent.mkdir(parents=True, exist_ok=True)
        with zipfile.ZipFile(path, "w") as archive:
            archive.writestr("classes.jar", jar.getvalue())
        return path

    @staticmethod
    def elf(defined: Sequence[str], needed: Sequence[str] = (), wide: bool = True) -> bytes:
        """A shared library of 64-bit or 32-bit ELF in little endian, whose dynamic symbols define `defined` and leave `needed` undefined."""
        names = b"\0"
        symbols = []
        for name, section in [*((name, 1) for name in defined), *((name, 0) for name in needed)]:
            offset = len(names)
            names += name.encode("utf-8") + b"\0"
            symbols.append((offset, section))
        symbol_format = "<IBBHQQ" if wide else "<IIIBBH"
        table = struct.pack(symbol_format, *((0, 0, 0, 0, 0, 0)))
        for offset, section in symbols:
            values = (offset, 0x12, 0, section, 0x1000, 0) if wide else (offset, 0x1000, 0, 0x12, 0, section)
            table += struct.pack(symbol_format, *values)

        header_size = 64 if wide else 52
        section_format = "<IIQQQQIIQQ" if wide else "<IIIIIIIIII"
        section_size = struct.calcsize(section_format)
        names_offset = header_size
        table_offset = names_offset + len(names)
        sections_offset = table_offset + len(table)
        sections = struct.pack(section_format, *([0] * 10))
        sections += struct.pack(section_format, 0, NativeBinaries.STRINGS, 0, 0, names_offset, len(names), 0, 0, 1, 0)
        sections += struct.pack(section_format, 0, NativeBinaries.DYNAMIC_SYMBOLS, 0, 0, table_offset, len(table), 1, 1, 8, struct.calcsize(symbol_format))

        identification = b"\x7fELF" + bytes([2 if wide else 1, 1, 1]) + bytes(9)
        if wide:
            header = identification + struct.pack("<HHIQQQIHHHHHH", 3, 183, 1, 0, 0, sections_offset, 0, header_size, 0, 0, section_size, 3, 0)
        else:
            header = identification + struct.pack("<HHIIIIIHHHHHH", 3, 40, 1, 0, 0, sections_offset, 0, header_size, 0, 0, section_size, 3, 0)
        return header + names + table + sections
