from pathlib import Path

import pytest

from haylen.core.errors.build_failed import BuildFailed
from haylen.engine.web_checks import WebChecks


class WebPageLogsTest:
    """A page whose check fails keeps its whole log in a file that the error names, while the error shows its last lines."""

    def test_a_failed_page_keeps_its_whole_log_in_a_file(self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch):
        monkeypatch.setattr(WebChecks, "PAGE_LOGS", tmp_path / "web-checks")
        lines = [f"Line {index} of the page." for index in range(WebChecks.SHOWN_LOG_LINES * 3)]
        report = {"steps": [{"name": "ready", "reached": True}], "failure": 'The step "ran" did not hold.', "log": lines}

        with pytest.raises(BuildFailed) as failure:
            WebChecks.check_steps(report, ["ready", "ran"], "The page of the editor check in the Worker mode")

        kept = tmp_path / "web-checks" / "the-page-of-the-editor-check-in-the-worker-mode.log"
        assert kept.read_text(encoding="utf-8") == "\n".join(lines) + "\n"
        assert str(kept) in str(failure.value)
        assert failure.value.details == "\n".join(lines[-WebChecks.SHOWN_LOG_LINES :])

    def test_a_page_that_passed_keeps_no_log(self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch):
        monkeypatch.setattr(WebChecks, "PAGE_LOGS", tmp_path / "web-checks")
        report = {"steps": [{"name": "ready", "reached": True}], "failure": None, "log": ["A line."]}

        WebChecks.check_steps(report, ["ready"], "The page of the fullscreen check")

        assert not (tmp_path / "web-checks").exists()
