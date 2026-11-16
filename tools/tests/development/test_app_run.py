import json
import os
import re
import shutil
import signal
import socket
import sys
import urllib.parse
from unittest import mock

import pytest

from haylen.cli.terminated import Terminated
from haylen.core.errors.development_failed import DevelopmentFailed
from haylen.development.app_run import AppRun
from haylen.development.development_address import DevelopmentAddress
from haylen.development.development_reach import DevelopmentReach
from haylen.development.development_record import DevelopmentRecord
from haylen.development.development_server import DevelopmentServer
from haylen.development.websocket import WebSocket
from tests.development.development_fixture import DevelopmentFixture


class AppRunTest:
    def test_apps_reach_the_server_by_how_their_backend_says_they_reach_this_machine(self, monkeypatch):
        monkeypatch.setattr(DevelopmentAddress, "local_network", staticmethod(lambda: "192.168.1.20"))
        path = f"{DevelopmentServer.PATH}?token=token"

        loopback = DevelopmentAddress.of(DevelopmentReach.LOOPBACK, "127.0.0.1")
        assert (loopback.listen, loopback.websocket(8000, "token")) == ("127.0.0.1", f"ws://127.0.0.1:8000{path}")
        network = DevelopmentAddress.of(DevelopmentReach.NETWORK, "127.0.0.1")
        assert (network.listen, network.websocket(9000, "token")) == ("0.0.0.0", f"ws://192.168.1.20:9000{path}")
        named = DevelopmentAddress.of(DevelopmentReach.NETWORK, "10.0.0.5")
        assert (named.listen, named.websocket(9000, "token")) == ("10.0.0.5", f"ws://10.0.0.5:9000{path}")
        every = DevelopmentAddress.of(DevelopmentReach.LOOPBACK, "0.0.0.0")
        assert (every.listen, every.websocket(9000, "token")) == ("0.0.0.0", f"ws://127.0.0.1:9000{path}")

    def test_a_device_run_on_a_machine_without_a_network_stops_with_what_to_do(self, monkeypatch):
        probe = mock.MagicMock()
        probe.__enter__.return_value.connect.side_effect = OSError(51, "Network is unreachable")
        monkeypatch.setattr(socket, "socket", mock.Mock(return_value=probe))

        with pytest.raises(DevelopmentFailed, match=re.escape('no address on a network that devices reach: Network is unreachable. Join a network, or name the address with "--host".')):
            DevelopmentAddress.of(DevelopmentReach.NETWORK, "127.0.0.1")

    def test_only_development_launches_connect_to_a_server_that_knows_the_shipped_package(self, tmp_path, monkeypatch):
        fixture = DevelopmentFixture(tmp_path, monkeypatch)
        shipped = tmp_path / "shipped"
        shutil.copytree(fixture.folder, shipped)
        fixture.write("source/level.lua", "return {edited = true}")
        product = fixture.product()

        with AppRun(fixture.context, product, DevelopmentFixture.launch(development=False), DevelopmentReach.LOOPBACK, shipped) as run:
            assert (run.developing, run.arguments, run.address, run.port) == (False, [], None, None)
            assert [name for name, _ in run.rows("This Mac")] == ["App", "Device", "Configuration"]
        assert DevelopmentRecord.read(fixture.folder) is None

        port = DevelopmentFixture.free_port()
        with AppRun(fixture.context, product, DevelopmentFixture.launch(development=True, port=port), DevelopmentReach.LOOPBACK, shipped) as run:
            assert run.developing
            assert run.port == port
            assert run.arguments == ["--dev-server", run.address]
            assert [name for name, _ in run.rows("This Mac")] == ["App", "Device", "Configuration", "Hot reload", "Debugger"]
            assert DevelopmentRecord.read(fixture.folder).address == f"http://127.0.0.1:{port}"
            token = urllib.parse.parse_qs(urllib.parse.urlsplit(run.address).query)["token"][0]

            with socket.create_connection(("127.0.0.1", port), timeout=5) as connection:
                connection.sendall(b"GET /haylen.json HTTP/1.1\r\nHost: localhost\r\n\r\n")
                assert b"404" in connection.recv(64)
            connection, stream, status = DevelopmentFixture.connect(port, token)
            assert "101" in status
            connection.sendall(DevelopmentFixture.masked(WebSocket.TEXT, DevelopmentFixture.hello("", 0)))
            _, _, message = WebSocket.read_frame(stream)
            assert [file["path"] for file in json.loads(message)["files"]] == ["source/level.lua"]
            stream.close()
            connection.close()
        assert DevelopmentRecord.read(fixture.folder) is None

    def test_a_port_that_something_else_uses_fails_with_what_to_do(self, tmp_path, monkeypatch):
        fixture = DevelopmentFixture(tmp_path, monkeypatch)
        product = fixture.product()

        with socket.create_server(("127.0.0.1", 0)) as taken:
            launch = DevelopmentFixture.launch(development=True, port=taken.getsockname()[1])
            with (
                pytest.raises(DevelopmentFailed, match=r'The development server cannot listen on port .* pick another one with "--port"\.'),
                AppRun(fixture.context, product, launch, DevelopmentReach.LOOPBACK, fixture.folder),
            ):
                pass

    @pytest.mark.skipif(os.name == "nt", reason="Windows ends a process that a signal reaches, so a command there has no signal to stop with.")
    def test_a_run_that_a_signal_ends_stops_the_app_it_follows(self, tmp_path, monkeypatch):
        fixture = DevelopmentFixture(tmp_path, monkeypatch)
        product = fixture.product("Release")
        previous = {name: signal.getsignal(getattr(signal, name)) for name in Terminated.SIGNALS}
        Terminated.install()

        # The app prints a line and runs on, and the signal comes as a bound on the time of the run sends it.
        def line(text: str) -> str:
            os.kill(os.getpid(), signal.SIGALRM)
            return text

        stop = mock.Mock()
        try:
            with pytest.raises(Terminated) as ended, AppRun(fixture.context, product, DevelopmentFixture.launch(development=False), DevelopmentReach.LOOPBACK, fixture.folder) as run:
                run.show("This Mac", stop)
                stop.side_effect = lambda: run.process.terminate()
                run.follow([sys.executable, "-c", "import time; print('Ready.', flush=True); time.sleep(30)"], line=line)
        finally:
            for name, handler in previous.items():
                signal.signal(getattr(signal, name), handler)

        assert ended.value.number == signal.SIGALRM
        stop.assert_called_once_with()
        assert run.process.poll() is not None, "The app ends with the run."
