import json
import shutil
from pathlib import Path

import pytest

from haylen.platforms.desktop.desktop_backend import DesktopBackend
from haylen.platforms.platform_backend import PlatformBackend
from haylen.platforms.web.web_backend import WebBackend
from tests.platforms.desktop.desktop_fixture import DesktopFixture


class GeneratedProjectTest:
    """The projects that the desktop and web backends generate are disposable and deterministic: a preparation with the same inputs writes nothing, a deleted project comes back the same, and a Lua and a C++ app with the same plugin get the same parts of it."""

    TARGETS = ("windows", "linux", "web")

    @pytest.fixture
    def fixture(self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> DesktopFixture:
        return DesktopFixture(tmp_path, monkeypatch)

    @staticmethod
    def backend(fixture: DesktopFixture, target: str) -> PlatformBackend:
        return WebBackend(fixture.context) if target == "web" else DesktopBackend(fixture.context)

    @staticmethod
    def add_plugin(fixture: DesktopFixture, app: Path) -> None:
        contributions = [
            {"type": "urlScheme", "platforms": ["windows", "linux"], "scheme": "garden"},
            {"type": "webModule", "platforms": ["web"], "path": "web/garden.js"},
        ]
        files = {"web/garden.js": "export default function load(context) {}\n", "web/shared.js": "export const shared = 1;\n"}
        fixture.plugin(app, "garden", {"windows": "native", "linux": "native", "web": "native"}, contributions, files)

    @pytest.mark.parametrize("target", TARGETS)
    def test_preparing_again_with_the_same_inputs_writes_nothing(self, fixture: DesktopFixture, target: str):
        app = fixture.app({"plugins": {"garden": {}}}, "garden")
        GeneratedProjectTest.add_plugin(fixture, app)
        backend = GeneratedProjectTest.backend(fixture, target)
        product = fixture.resolve(app, target)
        folders = DesktopFixture.folders(product)

        first = backend.prepare(product, folders)
        times = {path: path.stat().st_mtime_ns for path in folders.project.rglob("*") if path.is_file()}
        second = backend.prepare(fixture.resolve(app, target), folders)
        assert first.changed
        assert not second.changed
        assert {path: path.stat().st_mtime_ns for path in folders.project.rglob("*") if path.is_file()} == times

    @pytest.mark.parametrize("target", TARGETS)
    def test_a_deleted_project_comes_back_the_same(self, fixture: DesktopFixture, target: str):
        app = fixture.app({"plugins": {"garden": {}}}, "garden")
        GeneratedProjectTest.add_plugin(fixture, app)
        backend = GeneratedProjectTest.backend(fixture, target)
        product = fixture.resolve(app, target)
        folders = DesktopFixture.folders(product)

        backend.prepare(product, folders)
        before = DesktopFixture.files(folders.project)
        (folders.project / "app.cmake" if target != "web" else folders.project / "index.html").unlink()
        backend.prepare(fixture.resolve(app, target), folders)
        assert DesktopFixture.files(folders.project) == before

        shutil.rmtree(folders.project)
        backend.prepare(fixture.resolve(app, target), folders)
        assert DesktopFixture.files(folders.project) == before

    @pytest.mark.parametrize("target", TARGETS)
    def test_a_lua_and_a_cpp_app_with_the_same_plugin_get_the_same_plugin_parts(self, fixture: DesktopFixture, target: str):
        lua = fixture.app({"plugins": {"garden": {}}}, "lua/garden")
        cpp = fixture.cpp_app({"plugins": {"garden": {}}}, "cpp/garden")
        projects = []
        for app in (lua, cpp):
            GeneratedProjectTest.add_plugin(fixture, app)
            product = fixture.resolve(app, target)
            projects.append(GeneratedProjectTest.backend(fixture, target).prepare(product, DesktopFixture.folders(product)).project)

        parts = {"windows": ("links.iss",), "linux": ("resources/app.desktop",), "web": ("plugins/garden/garden.js", "plugins/garden/shared.js")}[target]
        for part in parts:
            assert (projects[0] / part).read_bytes() == (projects[1] / part).read_bytes(), part
        if target == "web":
            assert GeneratedProjectTest.plugins(projects[0]) == GeneratedProjectTest.plugins(projects[1])

    @staticmethod
    def plugins(project: Path) -> list:
        return json.loads((project / "config.json").read_text(encoding="utf-8"))["plugins"]
