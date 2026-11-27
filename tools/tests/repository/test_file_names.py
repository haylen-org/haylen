import ast
import re
import tomllib
from pathlib import Path

from haylen.core.repository import Repository
from tests.support.tracked_files import TrackedFiles


class FileNameTest:
    """Files and folders use dashes, and an underscore appears only where a name must be an identifier: Python modules that other files or entry points import, the packages and the tests that pytest discovers by `test_*.py`, Android resources, and the issue templates folder that GitHub reads. Every folder is lowercase, besides the names that a platform gives its folders and the tokens of the plugin template, and so is every file, besides the sources whose name is the name of their class, the names that a platform or a tool reads, the files that every repository names in capitals and the language tags of the translations."""

    # The folders whose names a platform gives: the issue templates of GitHub, the source set of the instrumented tests of Gradle, and the asset catalogs of Xcode with their image sets.
    PLATFORM_FOLDERS = re.compile(r"^(?:ISSUE_TEMPLATE|androidTest|Assets\.xcassets|Content\.imageset|\{\{[A-Z]+\}\})$")

    @staticmethod
    def imported(names: list[str]) -> set[str]:
        """Every module name that a Python file of the repository imports or that an entry point of a `pyproject.toml` names, by each part of its dotted path."""
        found: set[str] = set()
        for name in names:
            if name.endswith("pyproject.toml"):
                document = tomllib.loads((Repository.ROOT / name).read_text(encoding="utf-8"))
                points = document.get("project", {}).get("entry-points", {}).values()
                found.update(part for group in points for value in group.values() for part in value.partition(":")[0].split("."))
                continue
            if not name.endswith(".py"):
                continue
            for node in ast.walk(ast.parse((Repository.ROOT / name).read_text(encoding="utf-8"))):
                if isinstance(node, ast.Import):
                    found.update(part for alias in node.names for part in alias.name.split("."))
                elif isinstance(node, ast.ImportFrom) and node.module and node.level == 0:
                    found.update(node.module.split("."))
                    found.update(alias.name for alias in node.names)
        return found

    @staticmethod
    def allowed(name: str, imported: set[str]) -> bool:
        if name.startswith(".github/ISSUE_TEMPLATE/"):
            return "_" not in name.removeprefix(".github/ISSUE_TEMPLATE/")
        folder, separator, _ = name.partition("/src/main/res/")
        if separator:
            return "_" not in folder
        path = Path(name)
        if path.suffix != ".py" or "_" in path.parent.as_posix():
            return False
        return path.stem in imported or path.name.startswith("test_") or path.name == "__init__.py"

    # The sources whose names are the names of their classes, also as the templates that CMake configures.
    CLASS_SOURCES = re.compile(r"^[A-Z][A-Za-z0-9+]*\.(?:c|cpp|h|hpp|m|mm|swift|kt|java)(?:\.in)?$")
    # The files whose names a platform or a tool reads, and the files that every repository names in capitals.
    PLATFORM_FILES = frozenset(
        {
            "AGENTS.md",
            "AndroidManifest.xml",
            "CLOUD.md",
            "CMakeLists.txt",
            "CONTRIBUTING.md",
            "Contents.json",
            "FUNDING.yml",
            "Info.plist",
            "Info.plist.in",
            "KANBAN.md",
            "LICENSE",
            "LaunchScreen.storyboard",
            "PrivacyInfo.xcprivacy",
            "README.md",
            "VERSION",
        }
    )
    # A translation takes the canonical language tag of its language as its name, such as `pt-BR.json`.
    LANGUAGE_TAG = re.compile(r"^[a-z]{2,3}(?:-[A-Z][a-z]{3})?(?:-[A-Z]{2})?\.json$")

    @staticmethod
    def capital_allowed(name: str) -> bool:
        file = Path(name).name
        return file == file.lower() or "{{" in file or file in FileNameTest.PLATFORM_FILES or bool(FileNameTest.CLASS_SOURCES.match(file) or FileNameTest.LANGUAGE_TAG.match(file))

    def test_files_are_lowercase_where_nothing_requires_capitals(self):
        names = ("docs/lua-api/hotReload.md", "content/Logo.png", "source/MainMenu.lua", "engine/src/core/Engine.cpp", "content/locale/pt-BR.json", "README.md", "src/main/AndroidManifest.xml")
        assert [name for name in names if not FileNameTest.capital_allowed(name)] == ["docs/lua-api/hotReload.md", "content/Logo.png", "source/MainMenu.lua"]
        assert [name for name in TrackedFiles.names() if not FileNameTest.capital_allowed(name)] == []

    def test_files_use_dashes_where_nothing_requires_underscores(self):
        names = TrackedFiles.names()
        imported = FileNameTest.imported(names)
        assert [name for name in names if "_" in name and not FileNameTest.allowed(name, imported)] == []

    def test_folders_are_lowercase(self):
        assert [name for name in ("ISSUE_TEMPLATE", "Assets.xcassets", "{{PACKAGE}}", "Shaders", "myFolder") if not FileNameTest.PLATFORM_FOLDERS.match(name)] == ["Shaders", "myFolder"]
        folders = {"/".join(Path(name).parts[: depth + 1]) for name in TrackedFiles.names() for depth in range(len(Path(name).parts) - 1)}
        assert sorted(folder for folder in folders if Path(folder).name != Path(folder).name.lower() and not FileNameTest.PLATFORM_FOLDERS.match(Path(folder).name)) == []
