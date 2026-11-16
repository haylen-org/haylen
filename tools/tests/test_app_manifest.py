import json
import re

import pytest

from haylen.core.app_kind import AppKind
from haylen.core.app_manifest import AppManifest
from haylen.core.errors.invalid_manifest import InvalidManifest
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

    def test_fails_with_every_problem_and_its_key(self, tmp_path):
        app = ToolingFixture(tmp_path).app({"orientation": "sideways", "window": {"width": "wide", "highDpi": True}, "build": 0})

        with pytest.raises(InvalidManifest) as raised:
            AppManifest.load(app)

        message = raised.value.message
        assert 'The key "orientation" has the unknown value "sideways". It is "landscape", "portrait" or "any".' in message
        assert 'The key "window.width" must be a whole number.' in message
        assert 'The key "window" has the unknown keys "highDpi".' in message
        assert 'The key "build" is 0, and it must be at least 1.' in message

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
        manifest = AppManifest.load(ToolingFixture(tmp_path).app({"autoload": ["state"]}))
        with pytest.raises(TypeError):
            manifest.document["name"] = "Other"
        assert manifest.document["autoload"] == ("state",)
        assert json.loads((manifest.folder / AppManifest.FILE).read_text(encoding="utf-8"))["autoload"] == ["state"]
