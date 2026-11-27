import re
from pathlib import Path

from haylen.core.repository import Repository


class SystemMacrosTest:
    """The portable code of the engine never asks which system it compiles for and includes no header of a system, so a port of any system builds it as it is. What differs between systems lives in the folders of the platforms, in the folders `posix` and `windows` of the system interfaces that CMake picks one of, in the folders of the GPU devices and their helpers, and in the public headers of Apple plugins."""

    PLATFORM_FOLDERS = frozenset({"android", "apple", "headless", "linux", "posix", "sdl", "web", "windows", "worker"})
    SYSTEM_FOLDERS = frozenset({"posix", "windows"})
    SOURCE_SUFFIXES = frozenset({".c", ".cpp", ".h", ".hpp", ".in", ".mm"})
    CONDITION = re.compile(r"^\s*#\s*(?:if|ifdef|ifndef|elif)\b(.*)$", re.MULTILINE)
    # The headers of the systems and their frameworks, by their names or their folders, which portable code never includes or asks for.
    SYSTEM_HEADERS = r"(?:windows|winsock2|ws2tcpip|shellapi|shlobj|objbase|combaseapi|unistd|fcntl|dlfcn|pthread|poll|netdb|spawn|dirent|termios|crt_externs|TargetConditionals|jni|emscripten)\.h|(?:sys|mach|mach-o|arpa|netinet|android|emscripten|X11|Foundation|CoreFoundation|UIKit|AppKit)/[^>]+|wayland-[^>]+"
    SYSTEM_INCLUDE = re.compile(rf"^\s*#\s*(?:include|import)\s*<({SYSTEM_HEADERS})>", re.MULTILINE)
    HAS_INCLUDE = re.compile(rf"__has_include\s*\(\s*<({SYSTEM_HEADERS})>\s*\)")
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

    @staticmethod
    def system_names(text: str) -> list[str]:
        """The conditions on a system and the system headers that a source names."""
        found = [f"#if{condition}" for condition in SystemMacrosTest.CONDITION.findall(text) if SystemMacrosTest.SYSTEM_MACRO.search(condition) or SystemMacrosTest.HAS_INCLUDE.search(condition)]
        return found + [f"<{header}>" for header in SystemMacrosTest.SYSTEM_INCLUDE.findall(text)]

    def test_portable_code_names_no_system(self):
        found = []
        for root in (Repository.ENGINE / "src", Repository.ENGINE / "include", Repository.ENGINE / "lua" / "src", Repository.ENGINE / "lua" / "include"):
            for path in sorted(root.rglob("*")):
                if path.suffix not in SystemMacrosTest.SOURCE_SUFFIXES or SystemMacrosTest.is_system_specific(path):
                    continue
                found += [f"{path.relative_to(Repository.ROOT).as_posix()}: {name}" for name in SystemMacrosTest.system_names(path.read_text(encoding="utf-8"))]
        assert found == []

    def test_the_scan_finds_every_way_to_name_a_system(self):
        source = "#include <unistd.h>\n#import <Foundation/Foundation.h>\n#include <sys/stat.h>\n#if defined(__APPLE__)\n#endif\n#if __has_include(<windows.h>)\n#endif\n#include <vector>\n#if __has_include(<format>)\n#endif\n"
        assert SystemMacrosTest.system_names(source) == ["#if defined(__APPLE__)", "#if __has_include(<windows.h>)", "<unistd.h>", "<Foundation/Foundation.h>", "<sys/stat.h>"]
