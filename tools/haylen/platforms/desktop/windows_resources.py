from haylen.core.errors.invalid_manifest import InvalidManifest
from haylen.platforms.desktop.desktop_app import DesktopApp


class WindowsResources:
    """The resources of a Windows executable: the header that gives the resource script of the project the values of `haylen.json` for the version resource, whose four numbers are the three of the version and the build number, and the reader of the resources that a built executable holds, which the verification reads the application manifest with."""

    MANIFEST = 24
    # The version resource holds numbers of 16 bits.
    MAXIMUM_NUMBER = 65535
    HEADER = "resources/HaylenApp.h"

    @staticmethod
    def text(value: str) -> str:
        """Writes a value as a string of a resource script, where a quote doubles and a backslash starts an escape."""
        return '"' + value.replace("\\", "\\\\").replace('"', '""').replace("\n", "\\n") + '"'

    @staticmethod
    def header(app: DesktopApp, origin: str) -> str:
        version = [*(int(part) for part in app.version.split(".")), 0, 0][:3]
        numbers = [*version, app.build]
        if max(numbers) > WindowsResources.MAXIMUM_NUMBER:
            raise InvalidManifest(
                f'The version resource of Windows holds numbers up to {WindowsResources.MAXIMUM_NUMBER}, and the "version" "{app.version}" or the "build" "{app.build}" of `{origin}` goes past it.',
                origin=origin,
            )
        strings = {
            "NAME": app.name,
            "DESCRIPTION": app.description,
            "COMPANY": app.company,
            "COPYRIGHT": app.copyright,
            "EXECUTABLE": f"{app.executable}.exe",
            "VERSION": app.version,
        }
        lines = [
            "// The values of haylen.json for the resources of the executable, which the tooling writes.",
            *(f"#define HAYLEN_APP_{name} {WindowsResources.text(value)}" for name, value in strings.items()),
            f"#define HAYLEN_APP_VERSION_NUMBERS {', '.join(str(number) for number in numbers)}",
            "",
        ]
        return "\n".join(lines)

    @staticmethod
    def read(data: bytes, kind: int) -> list[bytes]:
        """Reads the resources of one type from a Windows executable: the data of every name and language under the type in the resource directory of the PE file, or nothing when the file has none."""
        if data[:2] != b"MZ":
            return []
        header = WindowsResources.number(data, 0x3C, 4)
        if data[header : header + 4] != b"PE\0\0":
            return []
        sections = WindowsResources.number(data, header + 6, 2)
        optional_size = WindowsResources.number(data, header + 20, 2)
        optional = header + 24
        directories = optional + (112 if WindowsResources.number(data, optional, 2) == 0x20B else 96)
        root_rva = WindowsResources.number(data, directories + 16, 4)
        if not root_rva:
            return []

        table = optional + optional_size
        mapped = []
        for index in range(sections):
            entry = table + index * 40
            size, address, raw_size, raw = (WindowsResources.number(data, entry + offset, 4) for offset in (8, 12, 16, 20))
            mapped.append((address, max(size, raw_size), raw))

        root = WindowsResources.offset(mapped, root_rva)
        found = []
        for name, child in WindowsResources.entries(data, root):
            if name != kind or not child & 0x80000000:
                continue
            for _, language_table in WindowsResources.entries(data, root + (child & 0x7FFFFFFF)):
                for _, leaf in WindowsResources.entries(data, root + (language_table & 0x7FFFFFFF)):
                    rva = WindowsResources.number(data, root + leaf, 4)
                    size = WindowsResources.number(data, root + leaf + 4, 4)
                    start = WindowsResources.offset(mapped, rva)
                    found.append(data[start : start + size])
        return found

    @staticmethod
    def number(data: bytes, offset: int, size: int) -> int:
        return int.from_bytes(data[offset : offset + size], "little")

    @staticmethod
    def offset(mapped: list[tuple[int, int, int]], rva: int) -> int:
        """The offset in the file of an address of the image, from the section that maps it."""
        return next(raw + rva - address for address, size, raw in mapped if address <= rva < address + size)

    @staticmethod
    def entries(data: bytes, directory: int) -> list[tuple[int, int]]:
        """The name or ID and the offset of every entry of a resource directory."""
        count = WindowsResources.number(data, directory + 12, 2) + WindowsResources.number(data, directory + 14, 2)
        return [(WindowsResources.number(data, directory + 16 + index * 8, 4), WindowsResources.number(data, directory + 20 + index * 8, 4)) for index in range(count)]
