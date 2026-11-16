import base64
import re

from haylen.core.repository import Repository
from tests.support.tracked_files import TrackedFiles


class WritingTest:
    """Comments are sentences that start with a capital letter, the texts that Lua gives the interface start with one too, and no file names another engine or the authors and tools of third-party art."""

    # The comment marker of each kind of source, with the patterns of its files.
    MARKERS = {
        "//": ("*.c", "*.cpp", "*.h", "*.hpp", "*.m", "*.mm", "*.js", "*.java", "*.kt", "*.kts", "*.swift", "*.glsl"),
        "--": ("*.lua",),
        "#": ("*.py", "*.cmake", "*CMakeLists.txt"),
    }
    # The markers of tools, which are no sentences.
    TOOL_MARKERS = ("clang-format", "NOLINT", "MARK:", "noqa", "type:", "pragma", "!", "#")

    @staticmethod
    def wrong_comment(line: str, marker: str) -> str | None:
        """The text of a line comment that is no complete sentence, or nothing. A comment that goes on in an indented line, such as a table under its sentence, is no sentence of its own."""
        stripped = line.lstrip()
        if not stripped.startswith(marker) or stripped.startswith((marker * 2 if marker != "--" else "---", "--[[")):
            return None
        text = stripped[len(marker) :]
        if not text.strip() or text.startswith(("  ", "\t")) or text.strip().startswith(WritingTest.TOOL_MARKERS):
            return None
        if text.strip()[0].islower() or text.rstrip()[-1] not in '.:!?)`"]':
            return text.strip()
        return None

    def test_line_comments_are_sentences_that_start_with_a_capital_letter(self):
        wrong = []
        for marker, patterns in WritingTest.MARKERS.items():
            for name in TrackedFiles.names(*patterns):
                if "/patches/" in name:
                    continue
                for number, line in enumerate((Repository.ROOT / name).read_text(encoding="utf-8").splitlines(), 1):
                    text = WritingTest.wrong_comment(line, marker)
                    if text is not None:
                        wrong.append(f"{name}:{number}: {text}")
        assert wrong == []

    def test_texts_that_lua_gives_the_interface_start_with_a_capital_letter(self):
        # A value without a space is data, such as an id, a path or a word that the app echoes, and code shown as code keeps its own form.
        option = re.compile(r"""(?<![\w.])(text|title|hint|caption|message|prompt|label|subtitle|description|tooltip)\s*=\s*(['"])((?:\\.|(?!\2).)*)\2""")
        names = ("iOS", "iPhone", "iPad", "iPadOS", "macOS", "tvOS", "visionOS")
        wrong = []
        for name in TrackedFiles.names("samples/*.lua", "templates/*.lua", "engine/bench/*.lua"):
            for number, line in enumerate((Repository.ROOT / name).read_text(encoding="utf-8").splitlines(), 1):
                if line.lstrip().startswith("--") or "'monospace'" in line:
                    continue
                for match in option.finditer(line):
                    value = match.group(3)
                    if re.search(r"bind\s*=\s*\{[^}]*$", line[: match.start()]) or " " not in value or "(" in value or value.startswith(names):
                        continue
                    if value[0].islower():
                        wrong.append(f"{name}:{number}: {value}")
        assert wrong == []

    def test_no_file_names_another_engine_or_the_authors_and_tools_of_third_party_art(self):
        # The project plan records the requests of the owner as they were made, and the bytes of binary files hold the metadata that tools write. The names are kept encoded, so this test names none of them itself.
        words = base64.b64decode(
            "a2VubmV5CnJ1YmJlcmR1Y2sKcmFuZG9tbWluZApvcGVuZ2FtZWFydAphc2Vwcml0ZQp0ZXh0dXJlcGFja2VyCnBpeGVsbWF0b3IKZ2FtZXN5bnRoCnNvdW5kYm9vdGgKZmwgc3R1ZGlvCmxvZ2ljIGV4cHJlc3MKYWRtb2IKZmlyZWJhc2UKYnVubnltYXJrCmdvZG90CnVucmVhbCBlbmdpbmUKY29jb3MyZApkZWZvbGQKZ2FtZW1ha2VyCnBoYXNlcgpwaXhpanMKZ29vZ2xlIHNpZ24KZ29vZ2xlIGlkZW50aXR5CmVwaWMgb25saW5l"
        ).split(b"\n")
        names = re.compile(b"|".join(re.escape(word) for word in words), re.IGNORECASE)
        found = [name for name in TrackedFiles.names() if name != "PROJECT.md" and names.search((Repository.ROOT / name).read_bytes())]
        assert found == []
