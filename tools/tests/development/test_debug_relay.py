import io
import json
import socket

from haylen.core.package_layout import PackageLayout
from haylen.development.debug_message import DebugMessage
from haylen.development.debug_relay import DebugRelay
from tests.development.development_fixture import DevelopmentFixture


class DebugRelayTest:
    class App:
        """An app connected to the development server, which keeps the messages of the debugger that the relay sends it."""

        def __init__(self) -> None:
            self.messages: list[dict] = []

        def send(self, opcode: int, payload: bytes) -> bool:
            self.messages.append(json.loads(payload)["message"])
            return True

    def test_messages_keep_their_framing_and_source_paths_turn_into_package_paths_and_back(self):
        message = {"seq": 1, "type": "request", "command": "setBreakpoints", "arguments": {"source": {"path": "/app/source/main.lua"}}}
        assert DebugMessage.read(io.BytesIO(DebugMessage.frame(message))) == message
        assert DebugMessage.read(io.BytesIO(b"Content-Length: 10\r\n\r\n{}")) is None

        frames = {"body": {"stackFrames": [{"source": {"name": "main.lua", "path": "source/main.lua"}}, {"name": "[C]"}]}}
        converted = DebugMessage.source_paths(frames, lambda path: "/app/" + path)
        assert converted["body"]["stackFrames"][0]["source"]["path"] == "/app/source/main.lua"
        assert converted["body"]["stackFrames"][1] == {"name": "[C]"}

    def test_the_relay_holds_requests_until_an_app_connects_and_configures_every_app_that_connects_later(self, tmp_path, monkeypatch):
        fixture = DevelopmentFixture(tmp_path, monkeypatch)
        relay = DebugRelay(PackageLayout.of(fixture.folder, {}), 0, fixture.context.terminal)
        relay.start()
        editor = socket.create_connection(("127.0.0.1", relay.port), timeout=5)
        stream = editor.makefile("rb")
        script = str(fixture.folder / "source" / "main.lua")

        try:
            editor.sendall(DebugMessage.frame({"seq": 1, "type": "request", "command": "initialize", "arguments": {}}))
            editor.sendall(DebugMessage.frame({"seq": 2, "type": "request", "command": "setBreakpoints", "arguments": {"source": {"path": script}, "breakpoints": [{"line": 3}]}}))
            DevelopmentFixture.wait(lambda: len(relay.pending) == 2)

            # The first app receives what the editor sent, with package paths, and its answers reach the editor with the paths of this machine.
            first = DebugRelayTest.App()
            relay.app_connected(first)
            assert [message["command"] for message in first.messages] == ["initialize", "setBreakpoints"]
            assert first.messages[1]["arguments"]["source"]["path"] == "source/main.lua"
            answer = {
                "seq": 1,
                "type": "response",
                "request_seq": 2,
                "success": True,
                "command": "setBreakpoints",
                "body": {"breakpoints": [{"verified": True, "source": {"path": "source/main.lua"}}]},
            }
            relay.from_app(first, answer)
            relay.from_app(first, {"seq": 2, "type": "event", "event": "initialized"})
            assert DebugMessage.read(stream)["body"]["breakpoints"][0]["source"]["path"] == script
            assert DebugMessage.read(stream)["event"] == "initialized"

            # An app that connects later, such as one that restarted, gets the configuration again, and the editor hears none of its answers to it.
            relay.app_left(first)
            second = DebugRelayTest.App()
            relay.app_connected(second)
            assert [message["command"] for message in second.messages] == ["initialize", "setBreakpoints"]
            assert all(message["seq"] > DebugRelay.REPLAY_SEQUENCE for message in second.messages)
            relay.from_app(second, {"seq": 1, "type": "response", "request_seq": second.messages[1]["seq"], "success": True, "command": "setBreakpoints"})
            relay.from_app(second, {"seq": 2, "type": "event", "event": "initialized"})
            relay.from_app(first, {"seq": 3, "type": "event", "event": "output", "body": {"output": "From an app that left."}})
            relay.from_app(second, {"seq": 3, "type": "event", "event": "stopped", "body": {"reason": "breakpoint"}})
            assert DebugMessage.read(stream)["event"] == "stopped"

            # A second editor waits for the first to leave, and an editor that leaves lets the app forget it.
            intruder = socket.create_connection(("127.0.0.1", relay.port), timeout=5)
            assert intruder.recv(1) == b""
            intruder.close()
            stream.close()
            editor.close()
            DevelopmentFixture.wait(lambda: second.messages[-1]["command"] == "disconnect")
            assert relay.configuration == {}
        finally:
            stream.close()
            editor.close()
            relay.close()
