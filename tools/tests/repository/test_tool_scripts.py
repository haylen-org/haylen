import ast

from haylen.core.repository import Repository


class ToolScriptTest:
    """The scripts of `tools` beside the tooling follow the code rules of the repository: one class per file, no free functions and no variables outside a class, and a script that runs as a command calls the static `main` of its class from its `__main__` guard."""

    def test_scripts_follow_the_code_rules(self):
        problems = []
        for path in sorted((Repository.ROOT / "tools").glob("*.py")):
            tree = ast.parse(path.read_text(encoding="utf-8"))
            classes = [node.name for node in tree.body if isinstance(node, ast.ClassDef)]
            functions = [node.name for node in tree.body if isinstance(node, ast.FunctionDef | ast.AsyncFunctionDef)]
            variables = [node.lineno for node in tree.body if isinstance(node, ast.Assign | ast.AnnAssign | ast.AugAssign)]
            if len(classes) != 1:
                problems.append(f"{path.name} holds the classes {classes}")
            if functions:
                problems.append(f"{path.name} defines the free functions {functions}")
            if variables:
                problems.append(f"{path.name} assigns variables outside a class at the lines {variables}")
            guards = [node for node in tree.body if isinstance(node, ast.If)]
            for guard in guards:
                called = ast.unparse(guard.body[0]) if len(guard.body) == 1 else ""
                if ast.unparse(guard.test) != "__name__ == '__main__'" or called != f"{classes[0]}.main()":
                    problems.append(f"{path.name} runs {ast.unparse(guard)!r} instead of the main of its class")
        assert problems == []
