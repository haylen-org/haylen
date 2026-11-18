import json
import os
import shutil
import sys
import threading
from unittest import mock

import pytest

from haylen.core.errors.build_failed import BuildFailed
from haylen.development.console_line import ConsoleLine
from haylen.development.run_session import RunSession
from tests.development.development_fixture import DevelopmentFixture


class RunSessionTest:
    @staticmethod
    def recording_app(sent: list[dict]) -> mock.Mock:
        """An app connected to the development server, which keeps the text messages it receives."""
        return mock.Mock(send=lambda opcode, payload: sent.append(json.loads(payload)) or True, send_batch=mock.Mock(return_value=True))

    def test_the_console_runs_its_lines_in_every_app_and_keeps_its_line_under_their_results(self, tmp_path, monkeypatch):
        fixture = DevelopmentFixture(tmp_path, monkeypatch)
        server = fixture.server()
        sent: list[dict] = []
        client = RunSessionTest.recording_app(sent)
        assert server.send_line("gold 50") == 0

        server.clients.append(client)
        session = RunSession(fixture.context, server)
        session.press("d")
        for key in [*"gold 55", "Backspace", "0", "Enter", *"  help ", "Enter"]:
            session.press(key)
        assert fixture.output.getvalue().endswith("\r\x1b[2K" + ConsoleLine.PROMPT)
        server.receive(client, {"type": "consoleResult", "id": 2, "ok": True, "output": 'Gold 50, the "shop" pays.'})
        server.receive(client, {"type": "consoleResult", "id": 3, "ok": False, "output": "There is no command."})
        session.press("Escape")
        session.press("q")

        assert [message for message in sent if message["type"] == "console"] == [{"type": "console", "id": 2, "line": "gold 50"}, {"type": "console", "id": 3, "line": "help"}]
        assert sum(message["type"] == "consoleCommands" for message in sent) == 3
        printed = fixture.output.getvalue()
        assert RunSession.CONSOLE_HINT.split(".")[0] in printed
        assert '\r\x1b[2KGold 50, the "shop" pays.\n\r\x1b[2K> ' in printed
        assert "Error: There is no command." in printed
        assert printed.endswith("\r\x1b[2K")
        assert not session.typing
        assert session.quitting.is_set()

    def test_enter_on_an_empty_line_leaves_the_console_and_gives_the_keys_back(self, tmp_path, monkeypatch):
        fixture = DevelopmentFixture(tmp_path, monkeypatch)
        server = fixture.server()
        sent: list[dict] = []
        server.clients.append(RunSessionTest.recording_app(sent))
        session = RunSession(fixture.context, server)

        for key in ["d", " ", "Enter", "s"]:
            session.press(key)

        assert not session.typing
        assert [message["line"] for message in sent if message["type"] == "console"] == ["stats"]

    def test_the_console_lists_the_commands_of_the_apps_with_their_help_broken_between_words(self, tmp_path, monkeypatch):
        monkeypatch.setattr(shutil, "get_terminal_size", lambda: os.terminal_size((60, 24)))
        fixture = DevelopmentFixture(tmp_path, monkeypatch)
        fixture.output.isatty = lambda: True
        server = fixture.server()
        sent: list[dict] = []
        client = RunSessionTest.recording_app(sent)
        server.clients.append(client)
        help_text = 'Shows the statistics as "off", "compact" or "full", or moves them to the next mode when the line leaves the mode out.'
        commands = [
            {"name": "restart", "usage": "restart", "help": "Restarts the app.", "arguments": []},
            {"name": "stats", "usage": "stats [word]", "help": help_text, "arguments": ["string?"]},
        ]
        server.receive(client, {"type": "consoleCommands", "commands": [*commands, {"name": "broken", "arguments": []}]})
        session = RunSession(fixture.context, server)

        session.press("d")
        for line in ("help", "help stats", "help missing"):
            for key in [*line, "Enter"]:
                session.press(key)

        assert [message for message in sent if message["type"] == "console"] == []
        printed = [line.split("\r\x1b[2K")[-1] for line in fixture.output.getvalue().split("\n")]
        assert "  restart       Restarts the app." in printed
        listed = printed.index('  stats [word]  Shows the statistics as "off", "compact" or')
        assert printed[listed + 1 : listed + 3] == ['                "full", or moves them to the next mode when', "                the line leaves the mode out."]
        assert printed.count('  stats [word]  Shows the statistics as "off", "compact" or') == 2
        assert 'Error: There is no command "missing". Type "help" to list the commands.' in printed

    def test_the_keys_of_the_tools_run_their_lines_in_the_apps(self, tmp_path, monkeypatch):
        fixture = DevelopmentFixture(tmp_path, monkeypatch)
        server = fixture.server()
        sent: list[dict] = []
        server.clients.append(RunSessionTest.recording_app(sent))
        session = RunSession(fixture.context, server)

        for key in "sipg":
            session.press(key)

        assert [message["line"] for message in sent] == ["stats", "inspector", "draw", "gpuCapture"]

    def test_the_keys_send_what_changed_and_restart_the_apps(self, tmp_path, monkeypatch):
        fixture = DevelopmentFixture(tmp_path, monkeypatch)
        server = fixture.server()
        sent: list[dict] = []
        client = RunSessionTest.recording_app(sent)
        server.clients.append(client)
        session = RunSession(fixture.context, server)

        fixture.write("source/level.lua", "return {edited = true}")
        for key in "rRr":
            session.press(key)

        assert [file["path"] for file in client.send_batch.call_args.args[0]["files"]] == ["source/level.lua"]
        assert sent == [{"type": "restart"}]
        assert "Nothing changed since the last files the apps received." in fixture.output.getvalue()

    def test_the_keys_of_a_run_quit_clear_and_list_and_a_run_without_development_offers_only_those(self, tmp_path, monkeypatch):
        fixture = DevelopmentFixture(tmp_path, monkeypatch, {"FORCE_COLOR": "1"})
        stop = mock.Mock()
        session = RunSession(fixture.context)
        assert list(session.keys) == ["c", "q", "h"]

        session.show([("App", "Demo")], stop)
        for key in "rRdsipgx":
            session.press(key)
        session.press("c")
        session.press("q")

        stop.assert_called_once_with()
        assert session.quitting.is_set()
        assert "\x1b[2J" in fixture.output.getvalue()
        assert "Quit" in fixture.output.getvalue()
        assert list(RunSession(fixture.context, fixture.server()).keys) == ["r", "R", "d", "s", "i", "p", "g", "c", "q", "h"]

    def test_a_run_that_quits_ends_its_process_without_an_error_and_a_failing_process_fails(self, tmp_path, monkeypatch):
        fixture = DevelopmentFixture(tmp_path, monkeypatch)
        session = RunSession(fixture.context)
        session.show([("App", "Demo")])

        threading.Timer(0.2, session.quit).start()
        session.follow([sys.executable, "-c", "import time; time.sleep(30)"])

        assert session.quitting.is_set()
        with pytest.raises(BuildFailed, match="failed with exit code 3"):
            RunSession(fixture.context).follow([sys.executable, "-c", "raise SystemExit(3)"])

    def test_the_lines_of_the_app_pass_through_the_function_of_the_line(self, tmp_path, monkeypatch):
        fixture = DevelopmentFixture(tmp_path, monkeypatch)
        script = "import sys; print('Kept.'); print('Dropped.'); print('Error.', file=sys.stderr)"

        RunSession(fixture.context).follow([sys.executable, "-c", script], line=lambda text: None if text == "Dropped." else text)

        assert sorted(fixture.output.getvalue().splitlines()) == ["Error.", "Kept."]
