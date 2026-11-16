from pathlib import Path

from haylen.core.repository import Repository
from tests.support.tracked_files import TrackedFiles


class EngineSources:
    """The C, C++, Objective-C and Objective-C++ files of the engine by their path in the repository, with their text, for the tests of the rules of its layers."""

    SUFFIXES = frozenset({".h", ".hpp", ".c", ".cpp", ".m", ".mm"})

    @staticmethod
    def read() -> dict[str, str]:
        return {name: (Repository.ROOT / name).read_text(encoding="utf-8") for name in TrackedFiles.names("engine") if Path(name).suffix in EngineSources.SUFFIXES}
