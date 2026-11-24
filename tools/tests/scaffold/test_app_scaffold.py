import json
from pathlib import Path

import pytest

from haylen.cli.application import Application
from haylen.cli.command_session import CommandSession
from haylen.core.app_kind import AppKind
from haylen.core.app_manifest import AppManifest
from haylen.core.repository import Repository
from tests.support.tooling_fixture import ToolingFixture


class AppScaffoldTest:
    """The apps that `haylen new` creates from the common layer of `templates` and the layer of their kind."""

    COMMON = Repository.TEMPLATES / "common"
    SETTINGS = (".gitignore", ".editorconfig", ".clang-format")

    @staticmethod
    def new(tmp_path: Path, words: list[str]) -> Path:
        session = CommandSession(ToolingFixture(tmp_path).context, False)
        parser, commands = Application.parser()
        arguments = parser.parse_args(["new", *words])
        commands[arguments.command_path].run(arguments, session)
        return Path(words[0])

    @staticmethod
    def layer(folder: Path) -> set[str]:
        return {path.relative_to(folder).as_posix() for path in folder.rglob("*") if path.is_file() and path.name != ".DS_Store"}

    @pytest.mark.parametrize(("words", "kind", "code"), [([], AppKind.LUA, "source/main.lua"), (["--kind", "cpp"], AppKind.CPP, "src/App.cpp")])
    def test_a_new_app_takes_the_common_layer_and_then_the_layer_of_its_kind(self, tmp_path: Path, words: list[str], kind: AppKind, code: str):
        app = AppScaffoldTest.new(tmp_path, [str(tmp_path / "notes"), *words])
        layer = Repository.TEMPLATES / kind.value

        assert AppScaffoldTest.layer(app) == AppScaffoldTest.layer(AppScaffoldTest.COMMON) | AppScaffoldTest.layer(layer)
        assert (app / code).read_bytes() == (layer / code).read_bytes()
        assert all((app / name).read_bytes() == (AppScaffoldTest.COMMON / name).read_bytes() for name in AppScaffoldTest.SETTINGS)
        assert AppManifest.load(app).kind is kind

    def test_a_new_cpp_app_names_its_kind_and_its_sources_and_runs_no_lua(self, tmp_path: Path):
        app = AppScaffoldTest.new(tmp_path, [str(tmp_path / "game"), "--kind", "cpp"])
        document = json.loads((app / AppManifest.FILE).read_text(encoding="utf-8"))

        assert (document["kind"], document["cpp"]) == ("cpp", {"sources": ["src"]})
        assert not (app / "source").exists()
        assert not AppManifest.load(app).runs_lua

    @pytest.mark.parametrize("kind", list(AppKind))
    def test_no_file_repeats_between_the_common_layer_and_the_layer_of_a_kind(self, kind: AppKind):
        assert AppScaffoldTest.layer(AppScaffoldTest.COMMON) & AppScaffoldTest.layer(Repository.TEMPLATES / kind.value) == set()
