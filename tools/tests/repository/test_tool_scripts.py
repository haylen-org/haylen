import ast

from haylen.core.repository import Repository


class ToolScriptTest:
    """The scripts of `tools` beside the tooling follow the code rules of the repository: one class per file, no free functions and no variables outside a class, and a script that runs as a command calls the static `main` of its class from its `__main__` guard."""

    @staticmethod
    def problems(name: str, text: str) -> list[str]:
        """What a script breaks of the code rules, such as a second class, a free function, a variable outside its class or a `__main__` guard that runs something else than the `main` of its class."""
        tree = ast.parse(text)
        classes = [node.name for node in tree.body if isinstance(node, ast.ClassDef)]
        functions = [node.name for node in tree.body if isinstance(node, ast.FunctionDef | ast.AsyncFunctionDef)]
        variables = [node.lineno for node in tree.body if isinstance(node, ast.Assign | ast.AnnAssign | ast.AugAssign)]
        problems = []
        if len(classes) != 1:
            problems.append(f"{name} holds the classes {classes}")
        if functions:
            problems.append(f"{name} defines the free functions {functions}")
        if variables:
            problems.append(f"{name} assigns variables outside a class at the lines {variables}")
        main = f"{classes[0]}.main()" if len(classes) == 1 else None
        for guard in (node for node in tree.body if isinstance(node, ast.If)):
            called = ast.unparse(guard.body[0]) if len(guard.body) == 1 else ""
            if ast.unparse(guard.test) != "__name__ == '__main__'" or called != main:
                problems.append(f"{name} runs {ast.unparse(guard)!r} instead of the main of its class")
        return problems

    def test_scripts_follow_the_code_rules(self):
        problems = []
        for path in sorted((Repository.ROOT / "tools").glob("*.py")):
            problems += ToolScriptTest.problems(path.name, path.read_text(encoding="utf-8"))
        assert problems == []

    def test_a_script_without_a_class_fails_the_rules_instead_of_the_check(self):
        assert ToolScriptTest.problems("draw.py", "def draw():\n    pass\n\nif __name__ == '__main__':\n    draw()\n") == [
            "draw.py holds the classes []",
            "draw.py defines the free functions ['draw']",
            "draw.py runs \"if __name__ == '__main__':\\n    draw()\" instead of the main of its class",
        ]
