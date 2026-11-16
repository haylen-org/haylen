import json

from haylen.development.development_server import DevelopmentServer
from haylen.development.site_run import SiteRun
from tests.development.development_fixture import DevelopmentFixture


class SiteRunTest:
    def test_only_development_runs_tell_the_page_to_connect(self, tmp_path, monkeypatch):
        fixture = DevelopmentFixture(tmp_path, monkeypatch)
        site = tmp_path / "site"
        site.mkdir()
        fixture.layout.zip(site / SiteRun.ARCHIVE)
        (site / SiteRun.CONFIG).write_text(json.dumps({"name": "App"}), encoding="utf-8")

        with SiteRun(fixture.context, fixture.product("Release"), DevelopmentFixture.launch(development=False), site) as run:
            assert run.development is None
            assert [name for name, _ in run.rows()] == ["App", "Page", "Site"]
        assert "development" not in json.loads((site / SiteRun.CONFIG).read_text(encoding="utf-8"))

        with SiteRun(fixture.context, fixture.product(), DevelopmentFixture.launch(development=True), site) as run:
            config = json.loads((site / SiteRun.CONFIG).read_text(encoding="utf-8"))
            assert config == {"name": "App", "development": {"path": DevelopmentServer.PATH, "token": run.development.token}}
            assert len(run.development.token) == 12
            assert int(run.development.token, 16) >= 0
            assert run.development.manifest == fixture.manifest
            assert [name for name, _ in run.rows()] == ["App", "Page", "Site", "Hot reload", "Debugger"]
