import ast
import re
from dataclasses import dataclass
from pathlib import PurePosixPath


class SourceComments:
    """Reads the comments of a source of any language of the repository, with the line each one starts on: the line and block comments of the C family, of JavaScript, Java, Kotlin, Swift, GLSL and CSS outside their strings, the line comments of Lua, Python, CMake, shell, YAML, TOML and ProGuard, the comments of HTML and XML, and the docstrings of Python."""

    C_FAMILY = (".c", ".cpp", ".h", ".hpp", ".m", ".mm", ".js", ".mjs", ".java", ".kt", ".kts", ".swift", ".glsl", ".css")
    HASH = (".py", ".cmake", ".sh", ".yml", ".yaml", ".toml", ".pro", ".ini", ".properties")
    MARKUP = (".html", ".xml", ".plist", ".storyboard", ".xcprivacy", ".svg", ".manifest")
    # The prefix of a raw string of C++, such as `R"(` or `u8R"json(`, which no other identifier ends with.
    RAW_PREFIX = re.compile(r"(?<![\w])(?:u8|[uUL])?R$")

    @dataclass(frozen=True)
    class Comment:
        line: int
        text: str
        # Whether the comment stands alone on its lines, rather than after code on the line where it starts.
        alone: bool
        block: bool

    @staticmethod
    def language(name: str) -> str | None:
        path = PurePosixPath(name)
        if path.suffix in SourceComments.C_FAMILY:
            return "c"
        if path.suffix == ".lua":
            return "lua"
        if path.suffix in SourceComments.HASH or path.name == "CMakeLists.txt" or path.name.endswith(".cmake.in"):
            return "hash"
        if path.suffix in SourceComments.MARKUP:
            return "markup"
        return None

    @staticmethod
    def read(name: str, text: str) -> list[SourceComments.Comment]:
        language = SourceComments.language(name)
        if language == "c":
            return SourceComments.c_family(text, css=name.endswith(".css"))
        if language == "lua":
            return SourceComments.line_comments(text, "--")
        if language == "hash":
            found = SourceComments.line_comments(text, "#")
            return found + SourceComments.docstrings(text) if name.endswith(".py") else found
        if language == "markup":
            return [SourceComments.Comment(text.count("\n", 0, match.start()) + 1, match.group(1), True, True) for match in re.finditer(r"<!--(.*?)-->", text, re.DOTALL)]
        return []

    @staticmethod
    def line_comments(text: str, marker: str) -> list[SourceComments.Comment]:
        """The comments of a language whose comments run from a marker to the end of the line, read where the marker starts the line, since a marker after code may sit in a string."""
        found = []
        for number, line in enumerate(text.splitlines(), 1):
            stripped = line.lstrip()
            if stripped.startswith(marker) and not stripped.startswith(marker + "[[" if marker == "--" else marker + "!"):
                found.append(SourceComments.Comment(number, stripped[len(marker) :], True, False))
        return found

    @staticmethod
    def c_family(text: str, css: bool) -> list[SourceComments.Comment]:
        """The comments of a source of the C family, outside its string and character literals, raw strings and template strings included."""
        found = []
        index = 0
        line = 1
        line_start = 0
        length = len(text)
        while index < length:
            character = text[index]
            following = text[index + 1] if index + 1 < length else ""
            if character == "\n":
                line += 1
                line_start = index + 1
                index += 1
                continue
            alone = not text[line_start:index].strip()
            if character == "/" and following == "/" and not css:
                end = text.find("\n", index)
                end = length if end < 0 else end
                found.append(SourceComments.Comment(line, text[index + 2 : end], alone, False))
                index = end
                continue
            if character == "/" and following == "*":
                end = text.find("*/", index + 2)
                end = length if end < 0 else end
                body = text[index + 2 : end]
                found.append(SourceComments.Comment(line, body, alone, True))
                line += body.count("\n")
                if "\n" in body:
                    line_start = index + 2 + body.rfind("\n") + 1
                index = end + 2
                continue
            if character in "\"'`":
                end = SourceComments.literal_end(text, index)
                line += text.count("\n", index, end)
                if "\n" in text[index:end]:
                    line_start = text.rfind("\n", index, end) + 1
                index = end
                continue
            index += 1
        return found

    @staticmethod
    def literal_end(text: str, index: int) -> int:
        """The position after the literal that opens at `index`: a C++ raw string, a triple-quoted string, a template string, a string or a character, which ends at its line where it does not close, as an apostrophe of prose or a digit separator does."""
        character = text[index]
        if character == '"' and SourceComments.RAW_PREFIX.search(text[max(0, index - 4) : index]):
            opening = text.find("(", index)
            delimiter = text[index + 1 : opening]
            close = text.find(f"){delimiter}" + '"', opening)
            return len(text) if close < 0 else close + len(delimiter) + 2
        if text.startswith(character * 3, index) and character == '"':
            close = text.find('"""', index + 3)
            return len(text) if close < 0 else close + 3
        if character == "'" and index > 0 and text[index - 1].isalnum():
            return index + 1
        end = index + 1
        while end < len(text) and text[end] != character:
            if text[end] == "\\":
                end += 2
                continue
            if text[end] == "\n" and character != "`":
                return end
            end += 1
        return end + 1

    @staticmethod
    def docstrings(text: str) -> list[SourceComments.Comment]:
        """The docstrings of the module, the classes and the functions of a Python source."""
        found = []
        for node in ast.walk(ast.parse(text)):
            if isinstance(node, ast.Module | ast.ClassDef | ast.FunctionDef | ast.AsyncFunctionDef) and node.body:
                first = node.body[0]
                if isinstance(first, ast.Expr) and isinstance(first.value, ast.Constant) and isinstance(first.value.value, str):
                    found.append(SourceComments.Comment(first.lineno, first.value.value, True, True))
        return found
