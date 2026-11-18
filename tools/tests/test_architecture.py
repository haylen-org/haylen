import ast
from pathlib import Path

from haylen.core.repository import Repository


class ArchitectureTest:
    """The rules of the code of the tooling that its design depends on: the core never imports a concrete backend, the imports form no cycle, and every module follows the code rules of the repository."""

    PACKAGE = Repository.ROOT / "tools" / "haylen"
    TESTS = Repository.ROOT / "tools" / "tests"
    # The layers that know no backend: they reach backends only through the contracts of `haylen.platforms` and the registry.
    CORE_LAYERS = ("haylen.core", "haylen.plugins", "haylen.sdk", "haylen.native")

    @staticmethod
    def modules(root: Path) -> dict[str, Path]:
        modules = {}
        for path in sorted(root.rglob("*.py")):
            relative = path.relative_to(root.parent).with_suffix("")
            modules[".".join(relative.parts)] = path
        return modules

    @staticmethod
    def imports(path: Path) -> set[str]:
        found = set()
        for node in ast.walk(ast.parse(path.read_text(encoding="utf-8"))):
            if isinstance(node, ast.ImportFrom) and node.module:
                found.add(node.module)
            elif isinstance(node, ast.Import):
                found.update(alias.name for alias in node.names)
        return {name for name in found if name.startswith("haylen")}

    @staticmethod
    def nested_functions(tree: ast.Module) -> list[str]:
        """The functions that a function of a module defines inside itself, where a method of its class belongs."""
        nested = []
        for node in ast.walk(tree):
            if not isinstance(node, ast.FunctionDef | ast.AsyncFunctionDef):
                continue
            nested += [inner.name for inner in ast.walk(node) if inner is not node and isinstance(inner, ast.FunctionDef | ast.AsyncFunctionDef)]
        return nested

    @staticmethod
    def is_contract(module: str) -> bool:
        """Tells a module of the contracts, which lie directly in `haylen.platforms`, from a module of a concrete backend in a package under it."""
        parts = module.split(".")
        return parts[:2] != ["haylen", "platforms"] or len(parts) == 3

    def test_the_core_imports_no_concrete_backend(self):
        problems = []
        for module, path in ArchitectureTest.modules(ArchitectureTest.PACKAGE).items():
            layered = module.startswith(ArchitectureTest.CORE_LAYERS) or (module.startswith("haylen.platforms.") and ArchitectureTest.is_contract(module))
            if not layered:
                continue
            problems += [f"{module} imports {name}" for name in ArchitectureTest.imports(path) if not ArchitectureTest.is_contract(name)]
        assert problems == []

    def test_the_imports_form_no_cycle(self):
        modules = ArchitectureTest.modules(ArchitectureTest.PACKAGE)
        graph = {module: {name for name in ArchitectureTest.imports(path) if name in modules} for module, path in modules.items()}
        state: dict[str, str] = {}
        cycles: list[str] = []
        for module in graph:
            if module not in state:
                ArchitectureTest.visit(module, [module], graph, state, cycles)
        assert cycles == []

    @staticmethod
    def visit(module: str, trail: list[str], graph: dict[str, set[str]], state: dict[str, str], cycles: list[str]) -> None:
        """Walks the imports of a module depth first and records each cycle that leads back to a module of the trail."""
        state[module] = "visiting"
        for name in sorted(graph[module]):
            if state.get(name) == "visiting":
                cycles.append(" -> ".join([*trail[trail.index(name) :], name]))
            elif name not in state:
                ArchitectureTest.visit(name, [*trail, name], graph, state, cycles)
        state[module] = "done"

    def test_modules_follow_the_code_rules(self):
        problems = []
        for root in (ArchitectureTest.PACKAGE, ArchitectureTest.TESTS):
            for module, path in ArchitectureTest.modules(root).items():
                text = path.read_text(encoding="utf-8")
                if path.name == "__init__.py":
                    if text:
                        problems.append(f"{module} is not empty")
                    continue
                tree = ast.parse(text)
                if any(isinstance(node, ast.Name | ast.alias) and getattr(node, "id", getattr(node, "name", "")) == "TYPE_CHECKING" for node in ast.walk(tree)):
                    problems.append(f"{module} uses TYPE_CHECKING")
                if any(isinstance(node, ast.ImportFrom) and node.module == "__future__" for node in tree.body):
                    problems.append(f"{module} imports from __future__")
                if root == ArchitectureTest.PACKAGE:
                    problems += [f"{module} nests the function {name}" for name in ArchitectureTest.nested_functions(tree)]
                classes = [node for node in tree.body if isinstance(node, ast.ClassDef)]
                functions = [node.name for node in tree.body if isinstance(node, ast.FunctionDef | ast.AsyncFunctionDef)]
                variables = [node for node in tree.body if isinstance(node, ast.Assign | ast.AnnAssign)]
                if len(classes) > 1:
                    problems.append(f"{module} holds {len(classes)} classes")
                if functions:
                    problems.append(f"{module} defines the free functions {functions}")
                if variables:
                    problems.append(f"{module} defines variables outside a class")
        assert problems == []
