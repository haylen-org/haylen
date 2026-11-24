import json
from pathlib import Path
from types import SimpleNamespace

import pytest

from haylen.core.errors.build_failed import BuildFailed
from haylen.core.errors.invalid_manifest import InvalidManifest
from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.engine.headless_browser import HeadlessBrowser
from haylen.engine.served_site import ServedSite
from haylen.engine.web_app_check import WebAppCheck
from haylen.platforms.verification_report import VerificationReport
from haylen.toolchains.emscripten import Emscripten
from tests.support.recorded_commands import RecordedCommands


class WebAppCheckTest:
    """The start check of an app on the web opens the page of the app on each graphics API in a browser of its own, checks the start and the loading of each page and stops the server and the browser in every case."""

    @staticmethod
    def report(**changes: object) -> dict:
        """The report of the driver for an app of development that started on WebGPU after a splash without a logo that loaded in order, with the fields that `changes` replaces."""
        splash = [
            {"percent": percent, "starting": starting, "ended": ended, "logo": None, "images": 0, "bar": True}
            for percent, starting, ended in (("0%", True, False), ("100%", False, False), ("100%", False, True))
        ]
        resources = [{"url": f"http://127.0.0.1:8000/{path}", "start": 1.0, "end": 2.0} for path in ("haylen.wasm", "app.zip")]
        started = {
            "drawsThrough": "webGpu",
            "firstFrame": True,
            "splashFailure": None,
            "exceptions": [],
            "log": [{"level": "info", "text": '[2026-11-16 10:00:00.000] [info] The app draws through "webGpu".'}],
            "splash": splash,
            "resources": resources,
            "expired": False,
            "stalled": False,
        }
        return {**started, **changes}

    @staticmethod
    def app(tmp_path: Path) -> Path:
        folder = tmp_path / "app"
        folder.mkdir()
        (folder / "haylen.json").write_text("{}", encoding="utf-8")
        return folder

    def test_only_the_apis_of_the_web_and_an_app_folder_are_checked(self, tmp_path):
        check = WebAppCheck(RecordedCommands(tmp_path).context, None)
        app = str(WebAppCheckTest.app(tmp_path))
        with pytest.raises(UnsupportedTarget, match='not "metal"'):
            check.run(app, "Debug", "metal")
        with pytest.raises(InvalidManifest, match=r'holds no "haylen\.json"'):
            check.run(str(tmp_path), "Debug", None)
        with pytest.raises(UnsupportedTarget, match='The configuration "RelWithDebInfo" is unknown'):
            check.run(app, "RelWithDebInfo", None)

    def test_each_api_opens_in_a_browser_of_its_own_and_a_failed_start_still_stops_the_server_and_the_browser(self, tmp_path, monkeypatch):
        ended = []
        monkeypatch.setattr(ServedSite, "__init__", lambda self, folder: None)
        monkeypatch.setattr(ServedSite, "__enter__", lambda self: self)
        monkeypatch.setattr(ServedSite, "__exit__", lambda self, *_: ended.append("server"))
        monkeypatch.setattr(ServedSite, "page", lambda self, query="", path="": f"http://127.0.0.1:8000/{query}")
        monkeypatch.setattr(HeadlessBrowser, "__enter__", lambda self: 9222)
        monkeypatch.setattr(HeadlessBrowser, "__exit__", lambda self, *_: ended.append(("browser", tuple(self.arguments))))
        monkeypatch.setattr(Emscripten, "node", staticmethod(lambda: Path("node")))
        site = tmp_path / "site"
        site.mkdir()
        (site / "config.json").write_text(json.dumps({"splash": {"logo": "splash.svg"}}), encoding="utf-8")
        product = SimpleNamespace(path=site, prepared=SimpleNamespace(product=SimpleNamespace(app=SimpleNamespace(name="Tests"))))
        pipeline = SimpleNamespace(resolve=lambda request, build_sdk: request, prepare=lambda resolved: resolved, build=lambda prepared: product, verify=lambda built: VerificationReport(()))

        reports = {"webGpu": WebAppCheckTest.report(), "webGl2": WebAppCheckTest.report(drawsThrough="webGl2", firstFrame=False, expired=True)}
        recorded = RecordedCommands(tmp_path, answer=lambda command: json.dumps(reports[command[3].rsplit("=", 1)[1]]))
        with pytest.raises(BuildFailed, match='The page of "Tests" on "webGl2" did not draw the first complete frame'):
            WebAppCheck(recorded.context, pipeline).run(str(WebAppCheckTest.app(tmp_path)), "Release", None)

        pages = [command[3] for command in recorded.commands]
        assert pages == ["http://127.0.0.1:8000/?graphicsApi=webGpu", "http://127.0.0.1:8000/?graphicsApi=webGl2"]
        assert ended == [("browser", HeadlessBrowser.SOFTWARE_GPU), ("browser", HeadlessBrowser.SOFTWARE_GPU), "server"]
