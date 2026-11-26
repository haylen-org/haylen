import json
import struct
import zipfile
from pathlib import Path
from types import SimpleNamespace

import pytest

from haylen.core.errors.build_failed import BuildFailed
from haylen.native.elf_symbols import ElfSymbols
from haylen.platforms.android.android_verifier import AndroidVerifier
from haylen.platforms.apple.dsym_symbols import DsymSymbols
from haylen.platforms.symbol_files import SymbolFiles
from haylen.platforms.symbol_resolution import SymbolResolution
from haylen.platforms.verification_check import VerificationCheck
from haylen.platforms.web.wasm_symbols import WasmSymbols
from tests.support.recorded_commands import RecordedCommands


class SymbolsTest:
    """The symbols of Release builds: their one folder in the folders of a product, and the resolution of an address of the engine to its function, file and line in the symbol files of each platform, as crash reports resolve the frames of the engine, together with the check that the binaries that ship hold no symbols."""

    FRAME = "haylen::core::Engine::frame(double)"
    BASE64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"

    def recorded(self, folder: Path, answers: dict[str, str]) -> RecordedCommands:
        recorded = RecordedCommands(folder)
        recorded.answer = lambda command: next((answer for key, answer in answers.items() if key in " ".join(command)), "")
        return recorded

    def test_the_symbols_of_a_release_replace_the_folder_of_the_last_one(self, tmp_path: Path):
        built = tmp_path / "tree"
        (built / "App.app.dSYM" / "Contents").mkdir(parents=True)
        (built / "App.app.dSYM" / "Contents" / "Info.plist").write_text("plist", encoding="utf-8")
        (built / "libhaylen-app.so").write_bytes(b"elf")
        folder = tmp_path / "symbols"
        (folder / "old.debug").parent.mkdir(parents=True)
        (folder / "old.debug").write_bytes(b"old")

        placed = SymbolFiles.place(folder, {"App.app.dSYM": built / "App.app.dSYM", "arm64-v8a/libhaylen-app.so": built / "libhaylen-app.so"})
        assert placed == folder
        assert sorted(path.relative_to(folder).as_posix() for path in folder.rglob("*") if path.is_file()) == ["App.app.dSYM/Contents/Info.plist", "arm64-v8a/libhaylen-app.so"]

    def test_symbols_that_the_build_did_not_make_fail_the_build(self, tmp_path: Path):
        with pytest.raises(BuildFailed, match="made no symbols"):
            SymbolFiles.place(tmp_path / "symbols", {"app.debug": tmp_path / "tree" / "app.debug"})

    def line_program(self, version: int) -> bytes:
        """A line table of one sequence: the row of `Engine.cpp` at line 412 from 0x1000, the row of another file from 0x1020, and the end at 0x1030."""
        lengths = bytes([0, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 1])
        if version == 5:
            directories = b"\x01\x01\x08" + self.leb(1) + b"/work/engine/src/core\0"
            files = b"\x02\x01\x08\x02\x0b" + self.leb(2) + b"Engine.cpp\0\x00Other.cpp\0\x00"
            tables = directories + files
            engine = 0
        else:
            tables = b"/work/engine/src/core\0\0" + b"Engine.cpp\0\x01\0\0" + b"Other.cpp\0\x01\0\0" + b"\0"
            engine = 1
        header = bytes([1, 1, 1, 0xFB, 14, 13]) + lengths + tables
        program = b"\x00\x09\x02" + struct.pack("<Q", 0x1000) + b"\x04" + self.leb(engine) + b"\x03" + self.leb(411) + b"\x01"
        program += b"\x02" + self.leb(0x20) + b"\x04" + self.leb(engine + 1) + b"\x01" + b"\x02" + self.leb(0x10) + b"\x00\x01\x01"
        prefix = struct.pack("<H", version) + (b"\x08\x00" if version == 5 else b"")
        unit = prefix + struct.pack("<I", len(header)) + header + program
        return struct.pack("<I", len(unit)) + unit

    def abbreviations(self) -> bytes:
        """The one abbreviation of the units of the tests: a compilation unit without children, with its line table and the range of its code."""
        return b"\x01\x11\x00" + b"\x10\x17" + b"\x11\x01" + b"\x12\x06" + b"\x00\x00" + b"\x00"

    def unit(self, version: int) -> bytes:
        """A compilation unit whose code runs from 0x1000 to 0x1030 and whose line table starts the section of the line tables."""
        header = struct.pack("<HBBI", 5, 1, 8, 0) if version == 5 else struct.pack("<HIB", 4, 0, 8)
        entry = b"\x01" + struct.pack("<IQI", 0, 0x1000, 0x30)
        return struct.pack("<I", len(header) + len(entry)) + header + entry

    def elf(self, sections: list[tuple[str, int, bytes, int]]) -> bytes:
        """A 64-bit ELF file with these sections, each a name, a type, its content and the index of the section it links, after the empty section and before the table of the names."""
        names = b"\0"
        positions = []
        for name, *_ in [*sections, (".shstrtab", 3, b"", 0)]:
            positions.append(len(names))
            names += name.encode() + b"\0"
        contents = [content for _, _, content, _ in sections] + [names]
        offset = 64
        headers = [struct.pack("<IIQQQQIIQQ", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)]
        for (_, kind, _, link), content, position in zip([*sections, (".shstrtab", 3, b"", 0)], contents, positions, strict=True):
            entry_size = 24 if kind in (2, 11) else 0
            headers.append(struct.pack("<IIQQQQIIQQ", position, kind, 0, 0, offset, len(content), link, 0, 1, entry_size))
            offset += len(content)
        header = b"\x7fELF\x02\x01\x01" + b"\0" * 9 + struct.pack("<HHIQQQIHHHHHH", 3, 183, 1, 0, 0, offset, 0, 64, 0, 0, 64, len(headers), len(headers) - 1)
        return header + b"".join(contents) + b"".join(headers)

    def symbols_file(self, folder: Path, version: int, names: list[tuple[str, int]]) -> Path:
        strings = b"\0"
        table = struct.pack("<IBBHQQ", 0, 0, 0, 0, 0, 0)
        for name, address in names:
            table += struct.pack("<IBBHQQ", len(strings), 2, 0, 1, address, 16)
            strings += name.encode() + b"\0"
        file = folder / f"app-{version}.debug"
        sections = [(".text", 8, b"", 0), (".symtab", 2, table, 3), (".strtab", 3, strings, 0), (".debug_line", 1, self.line_program(version), 0)]
        sections += [(".debug_abbrev", 1, self.abbreviations(), 0), (".debug_info", 1, self.unit(version), 0)]
        file.write_bytes(self.elf(sections))
        return file

    def test_an_elf_file_resolves_the_engine_through_its_symbol_table_and_its_line_tables(self, tmp_path: Path):
        names = [("_ZN6haylen4core6Engine5frameEd.cold", 0x1020), ("_ZN6haylen4core6Engine5frameEd", 0x1000), ("main", 0x1028)]
        for version in (4, 5):
            file = self.symbols_file(tmp_path, version, names)
            found = ElfSymbols().resolve(file)
            assert found == SymbolResolution("_ZN6haylen4core6Engine5frameEd", "/work/engine/src/core/Engine.cpp", 412)
            assert SymbolResolution.verify(found, file, "desktop").outcome is VerificationCheck.Outcome.PASSED

    def test_symbols_without_the_line_tables_of_the_engine_fail_the_verification(self, tmp_path: Path):
        missing = self.symbols_file(tmp_path, 5, [("main", 0x1000)])
        check = SymbolResolution.verify(ElfSymbols().resolve(missing), missing, "desktop")
        assert check.outcome is VerificationCheck.Outcome.FAILED
        assert 'hold no function "haylen::core::Engine::frame"' in check.detail

        outside = self.symbols_file(tmp_path, 4, [("_ZN6haylen4core6Engine5frameEd", 0x2000)])
        check = SymbolResolution.verify(ElfSymbols().resolve(outside), outside, "desktop")
        assert check.outcome is VerificationCheck.Outcome.FAILED
        assert '"src/core/Engine.cpp"' in check.detail

    def test_damaged_line_tables_fail_with_their_file_instead_of_resolving(self, tmp_path: Path):
        file = self.symbols_file(tmp_path, 5, [("_ZN6haylen4core6Engine5frameEd", 0x1000)])
        data = bytearray(file.read_bytes())
        start = data.index(self.line_program(5))
        data[start : start + 4] = struct.pack("<I", 0x7FFFFFF0)
        file.write_bytes(bytes(data))
        with pytest.raises(BuildFailed, match="are damaged"):
            ElfSymbols().resolve(file)

    def test_a_dsym_resolves_the_engine_for_each_architecture_with_the_tools_of_xcode(self, tmp_path: Path):
        dsym = tmp_path / "Test App.app.dSYM"
        answers = {"nm": f"0000000100853e24 t {SymbolsTest.FRAME}\n", "atos": f"{SymbolsTest.FRAME} (in Test App) (/Users/dev/haylen/engine/src/core/Engine.cpp:412)\n"}
        recorded = self.recorded(tmp_path, answers)
        found = DsymSymbols(recorded.context).resolve(dsym, "Test App", "arm64")
        assert found == SymbolResolution(SymbolsTest.FRAME, "/Users/dev/haylen/engine/src/core/Engine.cpp", 412)
        dwarf = str(dsym / "Contents" / "Resources" / "DWARF" / "Test App")
        assert recorded.commands == [["xcrun", "nm", "-arch", "arm64", "--defined-only", "-C", dwarf], ["xcrun", "atos", "-o", dwarf, "-arch", "arm64", "--fullPath", "0x0000000100853e24"]]

    def leb(self, value: int) -> bytes:
        encoded = bytearray()
        while True:
            byte = value & 0x7F
            value >>= 7
            encoded.append(byte | (0x80 if value else 0))
            if not value:
                return bytes(encoded)

    def section(self, kind: int, content: bytes) -> bytes:
        return bytes([kind]) + self.leb(len(content)) + content

    def vlq(self, value: int) -> str:
        value = (-value << 1) | 1 if value < 0 else value << 1
        text = ""
        while True:
            digit = value & 31
            value >>= 5
            text += SymbolsTest.BASE64[digit | (32 if value else 0)]
            if not value:
                return text

    def module(self, folder: Path, custom: bytes) -> tuple[Path, int]:
        """Writes a module with one imported function and two defined ones, and returns it with the offset of the body of the second defined function."""
        types = self.section(1, b"\x01\x60\x00\x00")
        imports = self.section(2, b"\x01\x03env\x01f\x00\x00")
        functions = self.section(3, b"\x02\x00\x00")
        bodies = [b"\x00\x0b", b"\x00\x01\x01\x0b"]
        content = b"\x02" + b"".join(self.leb(len(body)) + body for body in bodies)
        code = self.section(10, content)
        head = b"\0asm\x01\0\0\0" + types + imports + functions
        second = len(head) + 1 + len(self.leb(len(content))) + 1 + len(self.leb(len(bodies[0]))) + len(bodies[0]) + len(self.leb(len(bodies[1])))
        module = folder / "haylen.wasm"
        module.write_bytes(head + code + custom)
        return module, second

    def test_a_webassembly_module_resolves_the_engine_through_its_symbol_map_and_its_source_map(self, tmp_path: Path):
        module, second = self.module(tmp_path, b"")
        symbol_map = tmp_path / "haylen.js.symbols"
        symbol_map.write_text(f"0:f\n1:main\n2:{SymbolsTest.FRAME}\n", encoding="utf-8")
        first = second - 3
        mappings = ",".join([self.vlq(first) + self.vlq(0) + self.vlq(4) + self.vlq(0), self.vlq(second + 1 - first) + self.vlq(1) + self.vlq(407) + self.vlq(0)])
        source_map = tmp_path / "haylen.wasm.map"
        source_map.write_text(json.dumps({"version": 3, "sources": ["../src/main.cpp", "../../engine/src/core/Engine.cpp"], "names": [], "mappings": mappings}), encoding="utf-8")

        symbols = WasmSymbols(module)
        assert symbols.unstripped_sections() == []
        found = symbols.resolve(symbol_map, source_map)
        assert found == SymbolResolution(SymbolsTest.FRAME, "../../engine/src/core/Engine.cpp", 412)
        assert found.matches()

    def test_a_webassembly_module_that_ships_its_symbols_names_their_sections(self, tmp_path: Path):
        custom = self.section(0, b"\x10sourceMappingURL\x0fhaylen.wasm.map") + self.section(0, b"\x04name\x00")
        module, _ = self.module(tmp_path, custom)
        assert WasmSymbols(module).unstripped_sections() == ["sourceMappingURL", "name"]

    def test_a_stripped_elf_file_holds_neither_its_symbol_table_nor_its_debug_information(self):
        stripped = self.elf([(".text", 1, b"", 0), (".dynsym", 11, b"", 3), (".dynstr", 3, b"", 0)])
        assert ElfSymbols.unstripped_sections(stripped, "test") == []
        unstripped = self.elf([(".text", 1, b"", 0), (".symtab", 2, b"", 4), (".debug_line", 1, b"", 0), (".strtab", 3, b"", 0)])
        assert ElfSymbols.unstripped_sections(unstripped, "test") == [".symtab", ".debug_line"]

    def test_the_bundle_of_a_release_carries_the_debug_information_of_each_abi(self, tmp_path: Path):
        abis = ("arm64-v8a", "x86_64")
        product = SimpleNamespace(prepared=SimpleNamespace(product=SimpleNamespace(projects=[SimpleNamespace(target=SimpleNamespace(abi=abi)) for abi in abis])))
        bundle = tmp_path / "app.aab"
        with zipfile.ZipFile(bundle, "w") as archive:
            archive.writestr(f"{AndroidVerifier.BUNDLE_SYMBOLS}/arm64-v8a/libhaylen-app.so.dbg", b"elf")
        checks = AndroidVerifier.bundle_symbols(product, bundle)
        assert [check.outcome for check in checks] == [VerificationCheck.Outcome.FAILED]
        assert '"libhaylen-app.so" for "x86_64"' in checks[0].detail

        with zipfile.ZipFile(bundle, "a") as archive:
            archive.writestr(f"{AndroidVerifier.BUNDLE_SYMBOLS}/x86_64/libhaylen-app.so.dbg", b"elf")
        assert [check.outcome for check in AndroidVerifier.bundle_symbols(product, bundle)] == [VerificationCheck.Outcome.PASSED]

    def test_damaged_webassembly_symbols_fail_with_their_files_instead_of_resolving(self, tmp_path: Path):
        module, second = self.module(tmp_path, b"")
        truncated = tmp_path / "truncated" / "haylen.wasm"
        truncated.parent.mkdir()
        truncated.write_bytes(module.read_bytes()[: second - 2] + b"\x0a\xff\xff\xff")
        with pytest.raises(BuildFailed, match="is damaged"):
            WasmSymbols(truncated)

        symbol_map = tmp_path / "haylen.js.symbols"
        symbol_map.write_text(f"2:{SymbolsTest.FRAME}\n", encoding="utf-8")
        source_map = tmp_path / "haylen.wasm.map"
        for mappings in ("!!!", "g" * 40):
            source_map.write_text(json.dumps({"sources": ["Engine.cpp"], "mappings": mappings}), encoding="utf-8")
            with pytest.raises(BuildFailed, match="are damaged"):
                WasmSymbols(module).resolve(symbol_map, source_map)

        source_map.write_text(json.dumps({"sources": ["Engine.cpp"], "mappings": self.vlq(second) + self.vlq(-1) + self.vlq(0) + self.vlq(0)}), encoding="utf-8")
        assert WasmSymbols(module).resolve(symbol_map, source_map) == SymbolResolution(SymbolsTest.FRAME, "", 0)
