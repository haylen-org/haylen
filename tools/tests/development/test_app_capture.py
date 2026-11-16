import argparse
import functools
import json
import threading
from unittest import mock

import pytest

from haylen.cli.command_session import CommandSession
from haylen.cli.commands.app_capture import AppCaptureCommand
from haylen.core.errors.development_failed import DevelopmentFailed
from haylen.development.development_handler import DevelopmentHandler
from haylen.development.development_record import DevelopmentRecord
from haylen.development.local_server import LocalServer
from tests.development.development_fixture import DevelopmentFixture


class AppCaptureTest:
    def test_the_capture_reaches_the_run_through_the_record_of_its_server_and_its_token(self, tmp_path, monkeypatch):
        fixture = DevelopmentFixture(tmp_path, monkeypatch)
        server = fixture.server()
        written = {"ok": True, "path": "/captures/one.gputrace", "tool": "metalFrameCapture", "frames": 1}

        def send(opcode: int, payload: bytes) -> bool:
            message = json.loads(payload)
            threading.Thread(target=answer, args=(message,)).start()
            return True

        def answer(message: dict) -> None:
            server.receive(app, {"type": "consoleResult", "id": message["id"], "ok": True, "output": "Capturing the next frame."})
            server.receive(app, {"type": "report", "gpuCapture": written})

        app = mock.Mock(send=send)
        server.clients.append(app)
        listener = LocalServer(("127.0.0.1", 0), functools.partial(DevelopmentHandler, development=server))
        threading.Thread(target=listener.serve_forever, daemon=True).start()
        command = AppCaptureCommand()
        arguments = argparse.Namespace(app=str(fixture.folder), frames=1)
        session = CommandSession(fixture.context, structured=False)

        try:
            with pytest.raises(DevelopmentFailed, match="No run of the app"):
                command.run(arguments, session)

            server.start("0.0.0.0", listener.server_address[1], None)
            record = DevelopmentRecord.path(fixture.folder)
            assert record.stat().st_mode & 0o777 == 0o600
            assert DevelopmentRecord.read(fixture.folder).address == f"http://127.0.0.1:{listener.server_address[1]}"
            assert command.run(arguments, session) == {"results": [written]}
            assert 'The app wrote the GPU capture of 1 frame with "metalFrameCapture" to /captures/one.gputrace.' in fixture.output.getvalue()

            server.close()
            assert not record.exists()
            with pytest.raises(DevelopmentFailed, match="No run of the app"):
                command.run(arguments, session)
        finally:
            listener.shutdown()
            listener.server_close()

    def test_a_number_of_frames_is_a_whole_number_of_at_least_one(self):
        assert AppCaptureCommand.frames("3") == 3
        for text in ("0", "-1", "two"):
            with pytest.raises(argparse.ArgumentTypeError, match="is no number of frames"):
                AppCaptureCommand.frames(text)
