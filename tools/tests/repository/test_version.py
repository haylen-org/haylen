import re
import tomllib

from haylen.core.repository import Repository
from tests.support.tracked_files import TrackedFiles


class VersionTest:
    """The file `engine/VERSION` holds the version of Haylen, and every build and test takes it from there, so a new version changes one file and the `pyproject.toml` that repeats it. The guides and the plan, which show the output of commands and tell of releases, may show it as text."""

    # The files that hold the version, and the pages whose text shows it in the output of commands.
    HOLDERS = ("engine/VERSION", "pyproject.toml", "uv.lock")
    SHOWN = ("AGENTS.md", "KANBAN.md", "docs/")

    @staticmethod
    def copies(text: str, version: str) -> list[int]:
        """The lines of a text that hold the version as a version of its own, rather than as a part of a longer number."""
        copy = re.compile(rf"(?<![\w.]){re.escape(version)}(?![\w]|\.\d)")
        return [number for number, line in enumerate(text.splitlines(), 1) if copy.search(line)]

    def test_the_tooling_package_carries_the_version_of_the_engine(self):
        # The build backend of uv reads the version of the package only from its own file, so it repeats the version that `engine/VERSION` holds.
        project = tomllib.loads((Repository.ROOT / "pyproject.toml").read_text(encoding="utf-8"))
        assert project["project"]["version"] == Repository.engine_version()

    def test_no_other_file_copies_the_version(self):
        version = Repository.engine_version()
        assert VersionTest.copies(f'ENGINE_VERSION = "{version}"', version) == [1]
        assert VersionTest.copies(f"SwiftShader 4.{version}, 10.{version} and {version}.4", version) == []
        copying = []
        for name in TrackedFiles.names():
            if name in VersionTest.HOLDERS or name.startswith(VersionTest.SHOWN):
                continue
            content = (Repository.ROOT / name).read_bytes()
            if b"\0" not in content:
                copying += [f"{name}:{number}" for number in VersionTest.copies(content.decode("utf-8", errors="replace"), version)]
        assert copying == []
