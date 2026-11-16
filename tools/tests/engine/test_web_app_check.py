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
    """The start check of an app on the web passes only an app that drew its first frames through the API of its address without an error, names the first reason of a start that went wrong, and stops the server and the browser in every case."""

    PAGE = 'The page of "Tests" on "webGpu"'

    @staticmethod
    def report(**changes: object) -> dict:
        """The report of the driver for an app that started on WebGPU, with the fields that `changes` replaces."""
        started = {
            "drawsThrough": "webGpu",
            "firstFrame": True,
            "splashFailure": None,
            "exceptions": [],
            "log": [{"level": "info", "text": '[2026-11-16 10:00:00.000] [info] The app draws through "webGpu".'}],
            "expired": False,
            "stalled": False,
        }
        return {**started, **changes}

    @staticmethod
    def failure(report: dict) -> str:
        with pytest.raises(BuildFailed) as failed:
            WebAppCheck.check_report(report, "webGpu", WebAppCheckTest.PAGE)
        return failed.value.message

    @staticmethod
    def app(tmp_path: Path) -> Path:
        folder = tmp_path / "app"
        folder.mkdir()
        (folder / "haylen.json").write_text("{}", encoding="utf-8")
        return folder

    def test_an_app_that_drew_its_first_frames_through_the_api_of_its_address_passes(self):
        warning = {"level": "warning", "text": "[2026-11-16 10:00:00.000] [warning] The browser keeps no files for the page."}
        WebAppCheck.check_report(WebAppCheckTest.report(log=[*WebAppCheckTest.report()["log"], warning]), "webGpu", WebAppCheckTest.PAGE)

    def test_the_error_screen_of_the_app_fails_the_check_with_its_report_in_the_details(self):
        screen = '[2026-11-16 10:00:00.000] [error] The app stopped with an error\n\nThe chunk "87eb" is not on the device yet, so its file must be downloaded before it is read.'
        report = WebAppCheckTest.report(firstFrame=False, log=[*WebAppCheckTest.report()["log"], {"level": "error", "text": screen}])
        with pytest.raises(BuildFailed, match="showed the error screen of the app") as failed:
            WebAppCheck.check_report(report, "webGpu", WebAppCheckTest.PAGE)
        assert "is not on the device yet" in failed.value.details

    def test_each_failure_names_its_reason(self):
        error = {"level": "error", "text": "Saving user data failed."}
        cases = {
            "threw an error: TypeError: x is undefined at app.js:1:2": WebAppCheckTest.report(exceptions=["TypeError: x is undefined at app.js:1:2"]),
            "could not start the app: The app could not be loaded. The file was missing.": WebAppCheckTest.report(
                drawsThrough=None, firstFrame=False, splashFailure={"status": "The app could not be loaded.", "details": "The file was missing."}
            ),
            "logged an error in its console: Saving user data failed.": WebAppCheckTest.report(log=[error]),
            'did not log "The app draws through "webGpu"." within': WebAppCheckTest.report(drawsThrough=None, firstFrame=False, expired=True, log=[]),
            'drew through "webGl2" instead of "webGpu"': WebAppCheckTest.report(drawsThrough="webGl2"),
            "did not draw the first complete frame of the app": WebAppCheckTest.report(firstFrame=False, expired=True),
            "stopped answering": WebAppCheckTest.report(stalled=True),
        }
        for reason, report in cases.items():
            assert reason in WebAppCheckTest.failure(report)

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
        product = SimpleNamespace(path=tmp_path / "site", prepared=SimpleNamespace(product=SimpleNamespace(app=SimpleNamespace(name="Tests"))))
        pipeline = SimpleNamespace(resolve=lambda request: request, prepare=lambda resolved: resolved, build=lambda prepared: product, verify=lambda built: VerificationReport(()))

        reports = {"webGpu": WebAppCheckTest.report(), "webGl2": WebAppCheckTest.report(drawsThrough="webGl2", firstFrame=False, expired=True)}
        recorded = RecordedCommands(tmp_path, answer=lambda command: json.dumps(reports[command[3].rsplit("=", 1)[1]]))
        with pytest.raises(BuildFailed, match='The page of "Tests" on "webGl2" did not draw the first complete frame'):
            WebAppCheck(recorded.context, pipeline).run(str(WebAppCheckTest.app(tmp_path)), "Release", None)

        pages = [command[3] for command in recorded.commands]
        assert pages == ["http://127.0.0.1:8000/?graphicsApi=webGpu", "http://127.0.0.1:8000/?graphicsApi=webGl2"]
        assert ended == [("browser", HeadlessBrowser.SOFTWARE_GPU), ("browser", HeadlessBrowser.SOFTWARE_GPU), "server"]
