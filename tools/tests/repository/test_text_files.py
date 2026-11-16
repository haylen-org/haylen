import ast

from haylen.core.repository import Repository
from tests.support.tracked_files import TrackedFiles


class TextFileTest:
    """Windows reads and writes text in its own code page by default, which turns a character of a manifest into a byte that no UTF-8 tool reads, so every text that the Python of the tooling and its scripts reads or writes names its encoding."""

    @staticmethod
    def without_encoding(name: str) -> list[str]:
        tree = ast.parse((Repository.ROOT / name).read_text(encoding="utf-8"))
        calls = [node for node in ast.walk(tree) if isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute) and node.func.attr in ("read_text", "write_text")]
        return [f"{name}:{node.lineno}" for node in calls if not any(keyword.arg == "encoding" for keyword in node.keywords)]

    def test_every_text_file_is_read_and_written_as_utf_8(self):
        names = TrackedFiles.names("tools/*.py")
        assert names
        assert [problem for name in names for problem in TextFileTest.without_encoding(name)] == []
