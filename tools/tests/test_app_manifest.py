import json
import re

import pytest

from haylen.core.app_category import AppCategory
from haylen.core.app_kind import AppKind
from haylen.core.app_manifest import AppManifest
from haylen.core.errors.invalid_manifest import InvalidManifest
from haylen.core.repository import Repository
from tests.support.tooling_fixture import ToolingFixture


class AppManifestTest:
    def test_reads_the_identity_kind_and_plugins(self, tmp_path):
        fixture = ToolingFixture(tmp_path)
        app = fixture.app({"build": 7, "plugins": {"camera": {"parameters": {"usage": "Scans."}}}})

        manifest = AppManifest.load(app)

        assert (manifest.name, manifest.identifier, manifest.version, manifest.build, manifest.kind) == (
            "Test App",
            "com.haylen.tests",
            "1.0.0",
            7,
            AppKind.LUA,
        )
        assert manifest.plugins[0].id == "camera"
        assert manifest.plugins[0].parameters["usage"] == "Scans."

    def test_an_app_without_a_version_takes_the_version_of_the_engine(self, tmp_path):
        app = ToolingFixture(tmp_path).app({})
        document = json.loads((app / AppManifest.FILE).read_text(encoding="utf-8"))
        del document["version"]
        (app / AppManifest.FILE).write_text(json.dumps(document), encoding="utf-8")

        assert (AppManifest.load(app).version, AppManifest.load(app).build) == (AppManifest.DEFAULT_VERSION, 1)
        engine = (Repository.ENGINE / "include" / "haylen" / "core" / "AppConfig.hpp").read_text(encoding="utf-8")
        assert f'std::string version = "{AppManifest.DEFAULT_VERSION}";' in engine, "The tooling and the engine read a haylen.json without a version alike."

    def test_fails_with_every_problem_and_its_key(self, tmp_path):
        app = ToolingFixture(tmp_path).app({"orientation": "sideways", "window": {"width": "wide", "highDpi": True}, "build": 0})

        with pytest.raises(InvalidManifest) as raised:
            AppManifest.load(app)

        message = raised.value.message
        assert 'The key "orientation" has the unknown value "sideways". It is "landscape", "portrait" or "any".' in message
        assert 'The key "window.width" must be a whole number.' in message
        assert 'The key "window" has the unknown keys "highDpi".' in message
        assert 'The key "build" is 0, and it must be at least 1.' in message

    def test_an_app_names_a_category_of_the_vocabulary_or_none(self, tmp_path):
        fixture = ToolingFixture(tmp_path)
        assert AppManifest.load(fixture.app({"category": "education"})).category is AppCategory.EDUCATION
        assert AppManifest.load(fixture.app({}, "uncategorized")).category is None

        unknown = fixture.app({"category": "racing"}, "racing")
        with pytest.raises(InvalidManifest) as raised:
            AppManifest.load(unknown)
        values = '"game", "entertainment", "education", "productivity", "utility", "graphics", "music", "video", "social" or "developer"'
        assert f'The key "category" has the unknown value "racing". It is {values}.' in raised.value.message

    def test_a_cpp_app_names_its_sources(self, tmp_path):
        fixture = ToolingFixture(tmp_path)
        with pytest.raises(InvalidManifest, match=re.escape('names its sources in the "cpp" section')):
            AppManifest.load(fixture.app({"kind": "cpp"}))

        app = fixture.app({"kind": "cpp", "cpp": {"sources": ["src"]}}, "cpp-app")
        (app / "src").mkdir()
        (app / "src" / "App.cpp").write_text("int value = 1;\n", encoding="utf-8")

        manifest = AppManifest.load(app)

        assert manifest.kind is AppKind.CPP
        assert [path.name for path in manifest.cpp.files()] == ["App.cpp"]

    def test_a_cpp_app_runs_lua_only_when_its_section_says_so(self, tmp_path):
        fixture = ToolingFixture(tmp_path)
        without = fixture.app({"kind": "cpp", "cpp": {"sources": ["src"]}}, "without")
        scripted = fixture.app({"kind": "cpp", "cpp": {"sources": ["src"], "lua": True}}, "scripted")
        for app in (without, scripted):
            (app / "src").mkdir()

        assert AppManifest.load(fixture.app({}, "lua")).runs_lua
        assert not AppManifest.load(without).runs_lua
        assert AppManifest.load(scripted).runs_lua

        (without / "source").mkdir()
        with pytest.raises(InvalidManifest, match=re.escape("runs no Lua, so nothing runs the Lua modules of")):
            AppManifest.load(without)

    def test_a_lua_app_has_no_cpp_section(self, tmp_path):
        app = ToolingFixture(tmp_path).app({"cpp": {"sources": ["src"]}})
        with pytest.raises(InvalidManifest, match=re.escape("which only C++ apps have")):
            AppManifest.load(app)

    def test_names_a_folder_without_a_manifest(self, tmp_path):
        with pytest.raises(InvalidManifest, match=re.escape('holds no "haylen.json"')):
            AppManifest.load(tmp_path)

    def test_refuses_text_that_is_not_json(self, tmp_path):
        (tmp_path / AppManifest.FILE).write_text("{", encoding="utf-8")
        with pytest.raises(InvalidManifest, match=re.escape("is not JSON")):
            AppManifest.load(tmp_path)

    def test_the_document_is_frozen(self, tmp_path):
        manifest = AppManifest.load(ToolingFixture(tmp_path).app({"lua": {"autoloads": ["state"]}}))
        with pytest.raises(TypeError):
            manifest.document["name"] = "Other"
        assert manifest.document["lua"]["autoloads"] == ("state",)
        assert json.loads((manifest.folder / AppManifest.FILE).read_text(encoding="utf-8"))["lua"]["autoloads"] == ["state"]
