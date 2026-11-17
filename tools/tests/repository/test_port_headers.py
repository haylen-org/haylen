import re

from haylen.core.repository import Repository


class PortHeadersTest:
    """A port reaches the engine through the public API of `engine/include/haylen/port` and the other public headers alone, so a port outside the repository never depends on a file that the SDK does not install. The test port proves it: its sources include no header of `engine/src` and its CMake files compile no source of the engine."""

    PORT = Repository.ENGINE / "tests" / "data" / "port"
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

    def test_the_port_reaches_only_public_headers_of_the_engine(self):
        internal = {name for name in PortHeadersTest.reached() if not (Repository.ENGINE / "include" / name).is_file()}
        assert sorted(internal) == []
