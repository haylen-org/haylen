import ast
import re

from haylen.core.repository import Repository
from tests.support.tracked_files import TrackedFiles


class MessagePathsTest:
    """The messages of the tooling speak to an app developer who never saw the code of the engine, so no message that a command raises, prints or lists as a problem holds the path of a JSON document, such as an index in brackets or a path from the root of a document."""

    # An index of a list after a name, with a number or a value that the message fills in, the root of a JSON path, and the key of the slices of an SDK manifest.
    PATH = re.compile(r"\w\[(?:\d+|\{\})\]|\$\.|\bslices\[")
    # The methods of the terminal that print messages, and the lists that collect the problems of a manifest before a command raises them.
    PRINTS = frozenset({"step", "info", "note", "success", "warning", "error", "item", "group"})
    COLLECTORS = ("problem", "reason")

    @staticmethod
    def template(node: ast.expr) -> str | None:
        """The text of a string or an f-string, with each value it fills in as a pair of braces, or nothing for any other expression."""
        if isinstance(node, ast.Constant) and isinstance(node.value, str):
            return node.value
        if isinstance(node, ast.JoinedStr):
            return "".join(part.value if isinstance(part, ast.Constant) else "{}" for part in node.values)
        return None

    @staticmethod
    def messages(source: str) -> list[tuple[int, str]]:
        """The messages of a module: the first argument of every raised error, of every printing method of the terminal and of every append to a list of problems or reasons."""
        found = []
        tree = ast.parse(source)
        raised = {id(node.exc) for node in ast.walk(tree) if isinstance(node, ast.Raise) and node.exc is not None}
        for node in ast.walk(tree):
            if not isinstance(node, ast.Call) or not node.args:
                continue
            function = node.func
            name = function.attr if isinstance(function, ast.Attribute) else function.id if isinstance(function, ast.Name) else ""
            owner = function.value.id if isinstance(function, ast.Attribute) and isinstance(function.value, ast.Name) else ""
            collects = name == "append" and owner.startswith(MessagePathsTest.COLLECTORS)
            if id(node) in raised or name in MessagePathsTest.PRINTS or collects:
                text = MessagePathsTest.template(node.args[0])
                if text is not None:
                    found.append((node.lineno, text))
        return found

    def test_the_rule_finds_a_json_path_in_a_message(self):
        source = 'raise InvalidManifest(f"The key \\"slices[{index}]\\" needs a value.")\nproblems.append("The key $.window is wrong.")\nterminal.info("Item 1 of \\"plugins\\" is fine.")\n'
        assert [line for line, text in MessagePathsTest.messages(source) if MessagePathsTest.PATH.search(text)] == [1, 2]

    def test_no_message_of_the_tooling_holds_a_json_path(self):
        wrong = []
        for name in TrackedFiles.names("tools/haylen/*.py"):
            for line, text in MessagePathsTest.messages((Repository.ROOT / name).read_text(encoding="utf-8")):
                if MessagePathsTest.PATH.search(text):
                    wrong.append(f"{name}:{line}: {text}")
        assert wrong == []
