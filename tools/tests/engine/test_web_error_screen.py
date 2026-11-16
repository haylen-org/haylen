from pathlib import Path
from unittest import mock

from haylen.engine import web_checks
from haylen.engine.headless_browser import HeadlessBrowser
from haylen.engine.web_checks import WebChecks
from tests.support.recorded_commands import RecordedCommands


class WebErrorScreenTest:
    """The browsers of the error screen check draw through SwiftShader, so WebGPU starts without a GPU, and a graphics API that the command names draws every page of the checks through it."""

    @staticmethod
    def pages(tmp_path, monkeypatch, api: str | None) -> list[tuple[tuple[str, ...], str]]:
        """Runs the error screen and wake lock checks without a browser and returns the flags of each browser with the address of its page."""
        served = mock.MagicMock()
        served.page.side_effect = lambda query="", path="": f"http://127.0.0.1:8000/{path}{query}"
        site = mock.MagicMock()
        site.return_value.__enter__.return_value = served
        monkeypatch.setattr(web_checks, "ServedSite", site)
        monkeypatch.setattr(WebChecks, "site", lambda self, app: Path("site"))
        monkeypatch.setattr(WebChecks, "check_steps", staticmethod(lambda report, steps, page: None))
        opened = []
        monkeypatch.setattr(WebChecks, "drive", lambda self, driver, browser, purpose, page, *arguments: opened.append((tuple(browser), page)) or {})
        checks = WebChecks(RecordedCommands(tmp_path).context, None)
        checks.error_screen(api)
        checks.wake_lock(api)
        return opened

    def test_the_browsers_of_the_check_draw_through_swiftshader_so_webgpu_starts_without_a_gpu(self, tmp_path, monkeypatch):
        opened = WebErrorScreenTest.pages(tmp_path, monkeypatch, None)
        # Every mode opens once per graphics API, and the page with an unknown graphics API opens once more.
        screens = opened[: 2 * len(WebChecks.ERROR_SCREEN_STEPS) + 1]
        assert all(browser == HeadlessBrowser.SOFTWARE_GPU for browser, _ in screens)
        assert len(opened) == len(screens) + len(WebChecks.WAKE_LOCK_STEPS)

    def test_a_named_graphics_api_draws_every_page_of_the_checks_through_it(self, tmp_path, monkeypatch):
        # The page with an unknown graphics API draws its error screen through the first API that starts, so only the checks of that API open it.
        unknown = "http://127.0.0.1:8000/?graphicsApi=bogus"
        for api, opens_unknown in (("webGpu", True), ("webGl2", False)):
            named = f"http://127.0.0.1:8000/?graphicsApi={api}"
            pages = [page for _, page in WebErrorScreenTest.pages(tmp_path, monkeypatch, api)]
            assert pages == [named] * len(WebChecks.ERROR_SCREEN_STEPS) + [unknown] * opens_unknown + [named] * len(WebChecks.WAKE_LOCK_STEPS)
