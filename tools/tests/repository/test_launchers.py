import os
import subprocess

import pytest

from haylen.core.repository import Repository


class LauncherTest:
    """The launchers `haylen` and `haylen.cmd` only run the package of the tooling through uv, from any folder and through a link, and say what to install when uv is missing."""

    @pytest.mark.skipif(os.name == "nt", reason="The shell launcher runs on macOS and Linux.")
    def test_the_launcher_runs_the_tooling_from_any_folder_and_through_a_link(self, tmp_path):
        link = tmp_path / "haylen"
        link.symlink_to(Repository.ROOT / "haylen")
        for command in (Repository.ROOT / "haylen", link):
            result = subprocess.run([str(command), "--version"], cwd=tmp_path, capture_output=True, text=True, check=False)
            assert result.returncode == 0, result.stderr
            assert result.stdout.strip() == f"Haylen {Repository.engine_version()}"

    @pytest.mark.skipif(os.name == "nt", reason="The shell launcher runs on macOS and Linux.")
    def test_the_launcher_says_what_to_install_without_uv(self, tmp_path):
        result = subprocess.run([str(Repository.ROOT / "haylen"), "--help"], cwd=tmp_path, capture_output=True, text=True, env={"PATH": "/usr/bin:/bin"}, check=False)
        assert result.returncode == 1
        assert 'The command "uv" was not found.' in result.stderr

    def test_the_windows_launcher_hands_every_argument_to_the_tooling(self):
        script = (Repository.ROOT / "haylen.cmd").read_text(encoding="utf-8")
        assert 'uv run --project "%~dp0." --frozen haylen %*' in script
        assert "exit /b %errorlevel%" in script
