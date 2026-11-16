import dataclasses
import json
import zipfile
from pathlib import Path

import pytest

from haylen.core.configuration import Configuration
from haylen.core.errors.invalid_option import InvalidOption
from haylen.core.repository import Repository
from haylen.platforms.launch_request import LaunchRequest
from haylen.platforms.web.web_backend import WebBackend
from haylen.platforms.web.web_project import WebProject
from haylen.platforms.web.web_site import WebSite
from haylen.toolchains.emscripten import Emscripten
from tests.content.content_fixture import ContentFixture
from tests.platforms.desktop.desktop_fixture import DesktopFixture


class WebBackendTest:
    """The site of an app: the page of the template with the page scripts of the app, the web modules of its plugins in load order, the splash of `haylen.json` and, in Debug, the zipped package that the prebuilt web player plays."""

    @pytest.fixture
    def fixture(self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> DesktopFixture:
        return DesktopFixture(tmp_path, monkeypatch)

    def app(self, fixture: DesktopFixture, document: dict | None = None) -> Path:
        app = fixture.app({"plugins": {"banner": {"parameters": {"color": "#1D3557"}}}, **(document or {})}, "garden")
        module = "export default function load(context) {\n    context.registerScreen('banner', () => {});\n}\n"
        files = {"web/banner.js": module, "web/style.js": "export const style = 1;\n", "android/Banner.kt": "class Banner\n"}
        parameters = {"color": {"type": "string", "default": "#000000", "description": "The color."}}
        fixture.plugin(app, "banner", {"web": "native"}, [{"type": "webModule", "platforms": ["web"], "path": "web/banner.js"}], files, parameters)
        return app

    def test_the_site_holds_the_page_scripts_and_the_web_modules_of_the_plugins(self, fixture: DesktopFixture):
        app = self.app(fixture, {"platforms": {"web": {"scripts": ["native/web/app.js"], "files": {"loader.css": "native/web/loader.css"}}}})
        (app / "native" / "web").mkdir(parents=True)
        (app / "native" / "web" / "app.js").write_text("Module.preRun = [];\n", encoding="utf-8")
        (app / "native" / "web" / "loader.css").write_text(":root {}\n", encoding="utf-8")
        product = fixture.resolve(app, "web")
        project = WebBackend(fixture.context).prepare(product, DesktopFixture.folders(product)).project

        index = (project / "index.html").read_text(encoding="utf-8")
        assert '<script src="loader.js"></script>\n    <script src="scripts/native/web/app.js"></script>' in index
        assert index.count('src="loader.js"') == 1
        assert (project / "scripts" / "native" / "web" / "app.js").is_file()
        assert (project / "loader.css").read_text(encoding="utf-8") == ":root {}\n"
        assert sorted(path.name for path in (project / "plugins" / "banner").iterdir()) == ["banner.js", "style.js"]

        config = json.loads((project / "config.json").read_text(encoding="utf-8"))
        assert config["plugins"] == [{"id": "banner", "version": "1.0.0", "module": "plugins/banner/banner.js", "parameters": {"color": "#1D3557"}}]
        assert config["splash"] == {"logo": "splash.svg", "background": "rgba(0, 0, 0, 1.000)"}
        with zipfile.ZipFile(project / "app.zip") as archive:
            assert "plugins/banner/plugin.json" in archive.namelist()
        assert not (project / WebProject.RUNTIME).exists()

    def test_a_cpp_app_links_a_runtime_of_its_own_against_the_web_sdk(self, fixture: DesktopFixture):
        product = fixture.resolve(fixture.cpp_app(), "web")
        folders = DesktopFixture.folders(product)
        project = WebBackend(fixture.context).prepare(product, folders).project
        cmake = (project / WebProject.RUNTIME / "CMakeLists.txt").read_text(encoding="utf-8")
        assert f"set(haylen_DIR [[{product.primary.sdk.file('cmake').as_posix()}]])" in cmake
        assert f"CONTENT [[{folders.package.as_posix()}]])" in cmake
        assert (project / "app.zip").is_file()

    def test_a_debug_site_takes_the_prebuilt_player_and_the_sizes_of_its_downloads(self, fixture: DesktopFixture):
        backend = WebBackend(fixture.context)
        product = fixture.resolve(self.app(fixture), "web")
        built = backend.build(backend.prepare(product, DesktopFixture.folders(product)))

        config = json.loads((built.path / "config.json").read_text(encoding="utf-8"))
        assert config["sizes"] == {"app.zip": (built.path / "app.zip").stat().st_size, "haylen.wasm": (built.path / "haylen.wasm").stat().st_size}
        assert "release" not in config
        assert backend.verify(built).passed

        (built.path / "plugins" / "banner" / "banner.js").unlink()
        assert [check.detail for check in backend.verify(built).failures] == ['The site lacks the web module of the plugin "banner" that "config.json" lists. Prepare the app again.']

    def test_every_site_holds_the_page_side_and_the_worker_of_the_worker_mode(self, fixture: DesktopFixture):
        backend = WebBackend(fixture.context)
        product = fixture.resolve(self.app(fixture), "web")
        built = backend.build(backend.prepare(product, DesktopFixture.folders(product)))
        assert (built.path / "haylen-page.js").is_file()
        assert (built.path / "haylen-worker.js").is_file()

        (built.path / "haylen-worker.js").unlink()
        assert [check.detail for check in backend.verify(built).failures] == ['The site lacks "haylen-worker.js", which a page of the app loads.']

    def test_a_release_follows_the_release_that_the_request_names(self, fixture: DesktopFixture, tmp_path: Path, monkeypatch: pytest.MonkeyPatch):
        content = ContentFixture(tmp_path / "content", monkeypatch)
        monkeypatch.setattr(Repository, "APPS", tmp_path / "apps")
        monkeypatch.setattr(WebBackend, "releases", lambda backend: content.releases)
        product = fixture.resolve(self.app(fixture), "web", Configuration.RELEASE)
        shipped = tmp_path / "shipped"
        followed = dataclasses.replace(product, request=dataclasses.replace(product.request, previous_release=shipped))
        WebBackend(fixture.context).prepare(followed, DesktopFixture.folders(followed))

        build = content.commands("build")[0]
        assert build[build.index("--previous") + 1] == str(shipped.resolve())

    def test_a_release_site_minifies_its_page_scripts_and_leaves_the_runtime(self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch):
        site = tmp_path / "site"
        for name in ("loader.js", "haylen.js", "haylen-content-store.js", "scripts/app.js", "plugins/banner/banner.js", "plugins/banner/style.css"):
            (site / name).parent.mkdir(parents=True, exist_ok=True)
            (site / name).write_text("const value = 1;\n", encoding="utf-8")
        fixture = DesktopFixture(tmp_path / "fixture", monkeypatch)
        commands = []
        monkeypatch.setattr(Emscripten, "node", staticmethod(lambda: Path("node")))
        monkeypatch.setattr(fixture.context.process, "run", lambda command, **_: commands.append(command))
        WebSite(fixture.context, tmp_path / "project", site).minify()

        minified = {Path(command[2]).relative_to(site).as_posix(): command for command in commands}
        assert sorted(minified) == ["loader.js", "plugins/banner/banner.js", "scripts/app.js"]
        assert "--module" in minified["plugins/banner/banner.js"]
        assert "--module" not in minified["loader.js"]

    def test_the_isolating_opener_policy_refuses_plugins_that_open_screens(self, fixture: DesktopFixture):
        backend = WebBackend(fixture.context)
        product = fixture.resolve(self.app(fixture), "web")
        built = backend.build(backend.prepare(product, DesktopFixture.folders(product)))
        with pytest.raises(InvalidOption, match='The plugins "banner" open screens'):
            backend.run(built, LaunchRequest(None, True, "127.0.0.1", 0, 0, {"coop": "same-origin"}))
