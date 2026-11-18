import tomllib

from haylen.core.repository import Repository


class VersionTest:
    def test_the_tooling_package_carries_the_version_of_the_engine(self):
        # The build backend of uv reads the version of the package only from its own file, so it repeats the version that `engine/VERSION` holds.
        project = tomllib.loads((Repository.ROOT / "pyproject.toml").read_text(encoding="utf-8"))
        assert project["project"]["version"] == Repository.engine_version()
