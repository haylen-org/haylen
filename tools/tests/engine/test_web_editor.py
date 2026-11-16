from pathlib import Path
from unittest import mock

import pytest

from haylen.engine import web_checks
from haylen.engine.headless_browser import HeadlessBrowser
from haylen.engine.web_checks import WebChecks
from haylen.platforms.web.web_port import WebPort
from tests.support.recorded_commands import RecordedCommands


class WebEditorTest:
    """The editor check serves its page next to the web player of the web SDK and opens it on its own and in the Worker mode, each with the steps of its mode."""

    @pytest.mark.parametrize(("api", "query"), [(None, ""), ("webGl2", "graphicsApi=webGl2")])
    def test_the_page_opens_on_its_own_and_in_the_worker_mode_next_to_the_web_player(self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch, api: str | None, query: str):
        player = tmp_path / "player"
        player.mkdir()
        for name in WebPort.PLAYER_FILES:
            (player / name).write_text(f"// {name}\n", encoding="utf-8")
        monkeypatch.setattr(WebChecks, "web_player", lambda self: player)

        sites: list[set[str]] = []

        def served_site(folder: Path) -> mock.MagicMock:
            sites.append({path.name for path in folder.iterdir()})
            assert (folder / "index.html").read_bytes() == (WebChecks.DRIVERS / WebChecks.EDITOR_PAGE).read_bytes()
            served = mock.MagicMock()
            served.page.side_effect = lambda query="", path="": f"http://127.0.0.1:8000/{path}{query}"
            site = mock.MagicMock()
            site.__enter__.return_value = served
            return site

        monkeypatch.setattr(web_checks, "ServedSite", served_site)
        opened = []
        monkeypatch.setattr(WebChecks, "drive", lambda self, driver, browser, purpose, page, *arguments: opened.append((driver, tuple(browser), page, arguments)) or {})
        checked = []
        monkeypatch.setattr(WebChecks, "check_steps", staticmethod(lambda report, steps, page: checked.append((steps, page))))
        recorded = RecordedCommands(tmp_path)
        WebChecks(recorded.context, None).editor(api)

        assert sites == [{*WebPort.PLAYER_FILES, "index.html"}]
        alone = f"?{query}" if query else ""
        worker = f"?{query}&hosting=worker" if query else "?hosting=worker"
        assert opened == [
            ("editor-check.mjs", HeadlessBrowser.SOFTWARE_GPU, f"http://127.0.0.1:8000/{alone}", ()),
            ("editor-check.mjs", HeadlessBrowser.SOFTWARE_GPU, f"http://127.0.0.1:8000/{worker}", ("worker",)),
        ]
        assert checked == [(WebChecks.EDITOR_STEPS, "The page of the editor check"), (WebChecks.WORKER_STEPS, "The page of the editor check in the Worker mode")]
        assert "both with the runtime on the page and in the Worker mode" in recorded.output.getvalue()
