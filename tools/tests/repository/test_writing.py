import base64
import re

from haylen.core.repository import Repository
from tests.support.source_comments import SourceComments
from tests.support.tracked_files import TrackedFiles


class WritingTest:
    """Comments are sentences that start with a capital letter, the texts that Lua gives the interface start with one too, and no file names another engine or the authors and tools of third-party art."""

    # The markers of tools, which are no sentences.
    TOOL_MARKERS = ("clang-format", "NOLINT", "MARK:", "noqa", "type:", "pragma", "shellcheck", "#")
    ENDINGS = '.:!?)`"]'
    # The comments that clang-format writes after the brace that closes a namespace.
    NAMESPACE_END = re.compile(r"^\s*namespace [\w:]+\s*$")

    @staticmethod
    def wrong_line(text: str) -> bool:
        stripped = text.strip()
        return stripped[0].islower() or stripped[-1] not in WritingTest.ENDINGS

    @staticmethod
    def wrong_lines(comments: list[SourceComments.Comment]) -> list[tuple[int, str]]:
        """The numbered lines of the comments of a source that are no complete sentences. A line comment that goes on in an indented line, such as a table under its sentence, is no sentence of its own, but only under a comment that ends with a colon. Block comments and docstrings hold a sentence on every line of prose, besides the items of a list and the code between fences."""
        wrong = []
        follows = -1
        for comment in comments:
            if not comment.block:
                text = comment.text
                continues = comment.line == follows + 1 and text.startswith(("  ", "\t"))
                if continues or not text.strip():
                    follows = comment.line if continues else -1
                    continue
                follows = comment.line if text.rstrip().endswith(":") else -1
                if (comment.alone and text.strip().startswith(WritingTest.TOOL_MARKERS)) or (not comment.alone and WritingTest.NAMESPACE_END.match(text)):
                    continue
                if WritingTest.wrong_line(text):
                    wrong.append((comment.line, text.strip()))
                continue

            fenced = False
            listing = False
            for offset, line in enumerate(comment.text.splitlines()):
                prose = line.strip().lstrip("*").strip()
                if prose.startswith("```"):
                    fenced = not fenced
                    continue
                if fenced or not prose or (listing and line.startswith(("  ", "\t")) and not comment.text.startswith(line)):
                    continue
                listing = prose.endswith(":")
                item = prose.startswith(("- ", "* "))
                if prose[-1] not in WritingTest.ENDINGS or (not item and prose[0].islower()):
                    wrong.append((comment.line + offset, prose))
        return wrong

    def test_comments_are_sentences_that_start_with_a_capital_letter(self):
        wrong = []
        for name in TrackedFiles.names():
            if "/patches/" in name or SourceComments.language(name) is None:
                continue
            text = (Repository.ROOT / name).read_text(encoding="utf-8")
            wrong += [f"{name}:{number}: {line}" for number, line in WritingTest.wrong_lines(SourceComments.read(name, text))]
        assert wrong == []

    def test_the_check_reads_every_kind_of_comment(self):
        cases = {
            "Source.cpp": 'void draw(void* /*user*/) {\n    /* The middle.\n       half of a sentence */\n    const char* url = "https://example.com/*not*/";\n}  // namespace haylen::ui\n',
            "build.sh": "# the cache is saved here\n",
            "ci.yml": "jobs:\n  # Each job runs on Linux\n",
            "consumer-rules.pro": "# keep the bridge\n",
            "page.html": "<!-- the look of the splash -->\n",
            "manifest.xml": "<!-- The activity of the app. -->\n",
            "style.css": "/* The look of the splash. Change these variables to restyle it\n   with a file of the app. */\n",
            "script.py": '"""the tools of the art."""\n\n\nclass Tool:\n    """Draws the art.\n\n    - `panel.png`, the frame.\n\n    ```python\n    tool = Tool()\n    ```\n    """\n',
            "Fake.cpp": "// A sentence that ends.\n//   a fake continuation\n// A table follows:\n//   name   value\n",
        }
        found = {name: [line for _, line in WritingTest.wrong_lines(SourceComments.read(name, text))] for name, text in cases.items()}
        assert found == {
            "Source.cpp": ["user", "half of a sentence"],
            "build.sh": ["the cache is saved here"],
            "ci.yml": ["Each job runs on Linux"],
            "consumer-rules.pro": ["keep the bridge"],
            "page.html": ["the look of the splash"],
            "manifest.xml": [],
            "style.css": ["The look of the splash. Change these variables to restyle it", "with a file of the app."],
            "script.py": ["the tools of the art."],
            "Fake.cpp": ["a fake continuation"],
        }

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
        # The bytes of binary files hold the metadata that tools write. The names are kept encoded, so this test names none of them itself.
        words = base64.b64decode(
            "a2VubmV5CnJ1YmJlcmR1Y2sKcmFuZG9tbWluZApvcGVuZ2FtZWFydAphc2Vwcml0ZQp0ZXh0dXJlcGFja2VyCnBpeGVsbWF0b3IKZ2FtZXN5bnRoCnNvdW5kYm9vdGgKZmwgc3R1ZGlvCmxvZ2ljIGV4cHJlc3MKYWRtb2IKZmlyZWJhc2UKYnVubnltYXJrCmdvZG90CnVucmVhbCBlbmdpbmUKY29jb3MyZApkZWZvbGQKZ2FtZW1ha2VyCnBoYXNlcgpwaXhpanMKZ29vZ2xlIHNpZ24KZ29vZ2xlIGlkZW50aXR5CmVwaWMgb25saW5l"
        ).split(b"\n")
        names = re.compile(b"|".join(re.escape(word) for word in words), re.IGNORECASE)
        found = [name for name in TrackedFiles.names() if names.search((Repository.ROOT / name).read_bytes())]
        assert found == []
