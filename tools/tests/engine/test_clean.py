import os

import pytest

from haylen.cli.application import Application
from haylen.cli.command_session import CommandSession
from haylen.core.errors.build_failed import BuildFailed
from haylen.core.folders import Folders
from haylen.core.repository import Repository
from tests.support.recorded_commands import RecordedCommands


class CleanTest:
    """The command "haylen engine clean" removes the build folder, says when it was clean already, and a folder that stays is an error and never a clean."""

    @staticmethod
    def clean(tmp_path) -> RecordedCommands:
        parser, commands = Application.parser()
        arguments = parser.parse_args(["engine", "clean"])
        recorded = RecordedCommands(tmp_path)
        commands[arguments.command_path].run(arguments, CommandSession(recorded.context, False))
        return recorded

    def test_clean_removes_the_build_folder_and_says_when_it_was_already_clean(self, tmp_path, monkeypatch):
        build = tmp_path / "build"
        (build / "engine" / "macos-debug").mkdir(parents=True)
        monkeypatch.setattr(Repository, "BUILD", build)
        for message in ("Cleaned the build folder.", "The build folder is already clean."):
            recorded = CleanTest.clean(tmp_path)
            assert not build.exists()
            assert recorded.output.getvalue().strip() == f"\u2713 {message}"

    @pytest.mark.skipif(os.name == "nt" or os.geteuid() == 0, reason="Only a POSIX user who is not root keeps the files of a read-only folder.")
    def test_a_folder_that_stays_is_an_error_and_not_a_clean(self, tmp_path, monkeypatch):
        locked = tmp_path / "build" / "apps"
        locked.mkdir(parents=True)
        (locked / "app.log").write_text("", encoding="utf-8")
        # A folder whose files cannot go, as a running app or an editor keeps them on Windows, holds the build folder in place.
        locked.chmod(0o500)
        try:
            monkeypatch.setattr(Repository, "BUILD", tmp_path / "build")
            with pytest.raises(BuildFailed, match="could not be removed"):
                CleanTest.clean(tmp_path)
        finally:
            locked.chmod(0o700)

    def test_removing_a_folder_that_does_not_exist_yet_does_nothing(self, tmp_path):
        Folders.remove(tmp_path / "missing")
