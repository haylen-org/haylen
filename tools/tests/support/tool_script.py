import importlib.util
import types

from haylen.core.repository import Repository


class ToolScript:
    """Loads a script of `tools` whose name holds dashes, such as `check-image-edges.py`, as a module, so the tests call its class."""

    @staticmethod
    def load(name: str) -> types.ModuleType:
        path = Repository.ROOT / "tools" / name
        specification = importlib.util.spec_from_file_location(path.stem.replace("-", "_"), path)
        module = importlib.util.module_from_spec(specification)
        specification.loader.exec_module(module)
        return module
