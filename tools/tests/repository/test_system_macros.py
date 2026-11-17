import re
from pathlib import Path

from haylen.core.repository import Repository


class SystemMacrosTest:
    """The portable code of the engine never asks which system it compiles for, so a port of any system builds it as it is. What differs between systems lives in the folders of the platforms, in the folders `posix` and `windows` of the system interfaces that CMake picks one of, in the folders of the GPU devices and their helpers, and in the public headers of Apple plugins."""

    PLATFORM_FOLDERS = frozenset({"android", "apple", "headless", "linux", "posix", "sdl", "web", "windows", "worker"})
    SYSTEM_FOLDERS = frozenset({"posix", "windows"})
    SOURCE_SUFFIXES = frozenset({".c", ".cpp", ".h", ".hpp", ".in", ".mm"})
    CONDITION = re.compile(r"^\s*#\s*(?:if|ifdef|ifndef|elif)\b(.*)$", re.MULTILINE)
    SYSTEM_MACRO = re.compile(r"\b(?:_WIN32|_WIN64|__APPLE__|__MACH__|TARGET_OS_\w+|__ANDROID__|__EMSCRIPTEN__|__linux__|__unix__|__FreeBSD__|HAYLEN_PORT)\b")

    @staticmethod
    def is_system_specific(path: Path) -> bool:
        """Whether a source lives where code of one system belongs."""
        relative = path.relative_to(Repository.ENGINE)
        parts = relative.parts
        if parts[:2] == ("src", "platform") and len(parts) > 3 and parts[2] in SystemMacrosTest.PLATFORM_FOLDERS:
            return True
        if parts[:4] == ("include", "haylen", "platform", "apple"):
            return True
        if any(part in SystemMacrosTest.SYSTEM_FOLDERS for part in parts[:-1]):
            return True
        # A folder of the GPU devices holds the CMake file that adds it to the build, such as `device.cmake`.
        return parts[:2] == ("src", "gpu") and len(parts) > 3 and any((Repository.ENGINE / "src" / "gpu" / parts[2]).glob("*.cmake"))

    def test_portable_code_names_no_system(self):
        found = []
        for root in (Repository.ENGINE / "src", Repository.ENGINE / "include"):
            for path in sorted(root.rglob("*")):
                if path.suffix not in SystemMacrosTest.SOURCE_SUFFIXES or SystemMacrosTest.is_system_specific(path):
                    continue
                for condition in SystemMacrosTest.CONDITION.findall(path.read_text(encoding="utf-8")):
                    if SystemMacrosTest.SYSTEM_MACRO.search(condition):
                        found.append(f"{path.relative_to(Repository.ROOT).as_posix()}: #if{condition}")
        assert found == []
