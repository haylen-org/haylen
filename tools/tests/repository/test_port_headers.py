import re

from haylen.core.repository import Repository


class PortHeadersTest:
    """A port reaches the engine through its public headers alone, so a port outside the repository never depends on files that the SDK does not install. The test port still includes some internal headers of `engine/src` and compiles one internal source, which this test lists, and it fails on any other one, so the list only shrinks."""

    PORT = Repository.ENGINE / "tests" / "data" / "port"
    # The internal headers and sources of the engine that the test port still reaches, until the work of the stream of the engine ports moves what a port needs into `engine/include/haylen/port/`.
    INTERNAL = frozenset(
        {
            "gpu/DeviceFactory.hpp",
            "gpu/NullDevice.hpp",
            "gpu/PortDevice.hpp",
            "platform/BridgeRelay.hpp",
            "platform/CrashGuard.hpp",
            "platform/NativeHost.hpp",
            "platform/NativeWindow.hpp",
            "platform/PixelDensityCap.hpp",
            "platform/Player.hpp",
            "platform/ScreenRelay.hpp",
            "platform/Services.hpp",
            "src/gpu/NullDevice.cpp",
        }
    )
    INCLUDE = re.compile(r'^#\s*include\s*"([^"]+)"', re.MULTILINE)
    ENGINE_SOURCE = re.compile(r"\$\{HAYLEN_ENGINE_DIR\}/(src/[^\s\"]+)")

    @staticmethod
    def reached() -> set[str]:
        """The headers of the engine that the sources of the port include, apart from its own files, and the engine sources that its CMake files compile."""
        reached = set()
        for path in sorted(PortHeadersTest.PORT.iterdir()):
            text = path.read_text(encoding="utf-8") if path.suffix in (".cpp", ".hpp", ".cmake") else ""
            reached |= {name for name in PortHeadersTest.INCLUDE.findall(text) if not (PortHeadersTest.PORT / name).is_file()}
            reached |= set(PortHeadersTest.ENGINE_SOURCE.findall(text))
        return reached

    def test_the_port_reaches_no_internal_file_of_the_engine_beyond_the_known_ones(self):
        internal = {name for name in PortHeadersTest.reached() if not (Repository.ENGINE / "include" / name).is_file()}
        assert sorted(internal - PortHeadersTest.INTERNAL) == []

    def test_the_known_internal_files_are_still_reached(self):
        assert sorted(PortHeadersTest.INTERNAL - PortHeadersTest.reached()) == []
