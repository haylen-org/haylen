import http.client
import io
import json
import threading
from unittest import mock

from haylen.core.package_layout import PackageLayout
from haylen.development.debug_relay import DebugRelay
from haylen.development.development_server import DevelopmentServer
from haylen.development.site_server import SiteServer
from haylen.development.websocket import WebSocket
from tests.development.development_fixture import DevelopmentFixture


class DevelopmentServerTest:
    @staticmethod
    def answering_app(server: DevelopmentServer, answer: dict, report: dict | None) -> tuple[mock.Mock, list[dict]]:
        """An app that answers the console line of a capture with its result and then with the report of its capture, from another thread as a connection does."""
        sent: list[dict] = []

        def answer_line(message: dict) -> None:
            server.receive(app, {"type": "consoleResult", "id": message["id"], **answer})
            if report is not None:
                server.receive(app, {"type": "report", "gpuCapture": report})

        def send(opcode: int, payload: bytes) -> bool:
            message = json.loads(payload)
            sent.append(message)
            threading.Thread(target=answer_line, args=(message,)).start()
            return True

        app = mock.Mock(send=send)
        return app, sent

    def test_only_files_of_the_package_count(self):
        for path in ("haylen.json", "source/scenes/level.lua", "content/hero.png", "plugins/ads/plugin.json", "plugins/ads/source/init.lua"):
            assert DevelopmentServer.watched(path), path
        for path in (
            "source/.level.lua.swp",
            "source/level.lua~",
            "source/#level.lua#",
            "source/notes.txt",
            "content/hero.png.tmp",
            "content/.cache",
            "plugins/ads/android/build.gradle",
            "native/web/app.js",
            "README.md",
        ):
            assert not DevelopmentServer.watched(path), path

    def test_the_watcher_sends_saved_files_and_leaves_editor_files(self, tmp_path, monkeypatch):
        fixture = DevelopmentFixture(tmp_path, monkeypatch)
        server = fixture.server()

        fixture.write("source/level.lua", "return {edited = true}")
        fixture.write("source/.level.lua.swp", "swap")
        fixture.write("content/new.json", "[]")
        (fixture.folder / "content" / "data.json").unlink()

        assert server.scan() == (["content/new.json", "source/level.lua"], ["content/data.json"])
        assert server.scan() == ([], [])

    def test_a_file_that_goes_away_while_the_watcher_reads_it_keeps_the_watcher_running(self, tmp_path, monkeypatch):
        fixture = DevelopmentFixture(tmp_path, monkeypatch)
        server = fixture.server()
        manifest = fixture.folder / PackageLayout.MANIFEST

        # An editor or a tool deletes its scratch file between the listing of the package and the read of the file.
        listed = {**fixture.layout.files(), "content/scratch.png": fixture.folder / "content" / "scratch.png"}
        with mock.patch.object(PackageLayout, "files", return_value=listed):
            assert server.scan() == ([], [])

        # An editor that saves `haylen.json` may move the file aside before it writes the new one.
        manifest.rename(fixture.folder / "haylen.json~")
        assert server.scan() == ([], ["haylen.json"])
        (fixture.folder / "haylen.json~").rename(manifest)
        assert server.scan() == (["haylen.json"], [])

        fixture.write("content/sprite.png", "sprite")
        assert server.scan() == (["content/sprite.png"], [])
        (fixture.folder / "content" / "sprite.png").unlink()
        message, contents = server.batch(["content/sprite.png"], [])
        assert (message["files"], contents) == ([], [])
        assert server.scan() == ([], ["content/sprite.png"])

    def test_late_apps_catch_up(self, tmp_path, monkeypatch):
        fixture = DevelopmentFixture(tmp_path, monkeypatch)
        server = fixture.server()
        fixture.write("source/level.lua", "return {edited = true}")
        (fixture.folder / "content" / "data.json").unlink()
        server.scan()

        fresh, contents = server.catch_up({"type": "hello", "session": "", "revision": 0})
        assert [file["path"] for file in fresh["files"]] == ["source/level.lua"]
        assert fresh["removed"] == ["content/data.json"]
        assert contents == [b"return {edited = true}"]

        foreign, _ = server.catch_up({"type": "hello", "session": "earlier", "revision": 3})
        assert [file["path"] for file in foreign["files"]] == ["haylen.json", "source/level.lua", "source/main.lua"]
        assert server.catch_up({"type": "hello", "session": server.session, "revision": server.revision}) is None

    def test_the_server_watches_the_plugins_that_the_app_lists_while_it_runs_and_pushes_them_under_their_ids(self, tmp_path, monkeypatch):
        fixture = DevelopmentFixture(tmp_path, monkeypatch)
        far = tmp_path / "far-plugin"
        (far / "source").mkdir(parents=True)
        (far / "source" / "init.lua").write_text("return {}", encoding="utf-8")
        plugin = {"interface": 1, "id": "far", "name": "Far", "version": "1.0.0", "description": "A plugin of the tests.", "implementations": {"fake": "lua"}}
        (far / "plugin.json").write_text(json.dumps(plugin), encoding="utf-8")
        server = fixture.server()

        document = json.loads((fixture.folder / PackageLayout.MANIFEST).read_text(encoding="utf-8"))
        fixture.write(PackageLayout.MANIFEST, json.dumps({**document, "plugins": {"far": {"path": "../far-plugin"}}}))
        assert server.scan() == (["haylen.json", "plugins/far/plugin.json", "plugins/far/source/init.lua"], [])

        (far / "source" / "init.lua").write_text("return 'saved'", encoding="utf-8")
        DevelopmentFixture.later(far / "source" / "init.lua")
        changed, _ = server.scan()
        assert changed == ["plugins/far/source/init.lua"]
        message, contents = server.batch(changed, [])
        assert (message["files"], contents) == ([{"path": "plugins/far/source/init.lua", "size": 14}], [b"return 'saved'"])

        # The debugger shows the files of a plugin where they lie, and the app names them by their package paths.
        relay = DebugRelay(server.layout, 0, fixture.context.terminal)
        relay.close()
        assert relay.to_package(str(far / "source" / "init.lua")) == "plugins/far/source/init.lua"
        assert relay.to_editor("plugins/far/source/init.lua") == str(far.resolve() / "source" / "init.lua")
        assert relay.to_editor("source/main.lua") == str(fixture.folder / "source" / "main.lua")

    def test_a_problem_of_the_plugins_shows_once_and_the_server_keeps_the_layout_before_it(self, tmp_path, monkeypatch):
        fixture = DevelopmentFixture(tmp_path, monkeypatch)
        server = fixture.server()
        document = json.loads((fixture.folder / PackageLayout.MANIFEST).read_text(encoding="utf-8"))

        fixture.write(PackageLayout.MANIFEST, json.dumps({**document, "plugins": {"missing": {"path": "../missing"}}}))
        server.scan()
        fixture.write(PackageLayout.MANIFEST, json.dumps({**document, "plugins": {"missing": {"path": "../missing"}}, "build": 2}))
        server.scan()

        assert fixture.output.getvalue().count("holds no") == 1
        assert server.layout is fixture.layout

    def test_the_gpu_capture_reports_of_the_apps_print(self, tmp_path, monkeypatch):
        fixture = DevelopmentFixture(tmp_path, monkeypatch)
        server = fixture.server()
        client = mock.Mock()

        server.receive(client, {"type": "report", "gpuCapture": {"ok": True, "path": "/captures/one.gputrace", "tool": "metalFrameCapture", "frames": 1}})
        server.receive(client, {"type": "report", "gpuCapture": {"ok": False, "error": "The capture tool wrote no capture."}})

        assert 'The app wrote the GPU capture of 1 frame with "metalFrameCapture" to /captures/one.gputrace.' in fixture.output.getvalue()
        server.receive(client, {"type": "report", "gpuCapture": {"ok": False}})

        assert "The GPU capture of the app failed. The capture tool wrote no capture." in fixture.output.getvalue()
        assert "The GPU capture of the app failed, and the app gave no reason." in fixture.output.getvalue()

    def test_the_files_whose_code_runs_again_print_with_whether_the_app_resumed(self, tmp_path, monkeypatch):
        fixture = DevelopmentFixture(tmp_path, monkeypatch)
        server = fixture.server()
        client = mock.Mock()

        server.receive(client, {"type": "report", "restored": ["source/main.lua", "source/level.lua"], "resumed": True})
        server.receive(client, {"type": "report", "restored": ["source/menu.lua"]})

        assert 'The files "source/main.lua", "source/level.lua" hold the code that runs again, and the app resumed.' in fixture.output.getvalue()
        assert 'The files "source/menu.lua" hold the code that runs again.' in fixture.output.getvalue()

    def test_a_gpu_capture_waits_for_every_app_and_returns_what_each_one_wrote(self, tmp_path, monkeypatch):
        fixture = DevelopmentFixture(tmp_path, monkeypatch)
        server = fixture.server()
        assert server.capture_gpu(1) == []

        written = {"ok": True, "path": "/captures/two.rdc", "tool": "renderDoc", "frames": 2}
        capturing, sent = DevelopmentServerTest.answering_app(server, {"ok": True, "output": "Capturing the next 2 frames."}, written)
        refusing, _ = DevelopmentServerTest.answering_app(server, {"ok": False, "output": "The capture tool is not loaded into the app."}, None)
        server.clients += [capturing, refusing]
        results = server.capture_gpu(2)

        assert sent == [{"type": "console", "id": 2, "line": "gpuCapture 2"}]
        assert sorted(results, key=json.dumps) == sorted([written, {"ok": False, "error": "The capture tool is not loaded into the app."}], key=json.dumps)
        assert server.awaited == {}
        assert server.gpu_reports is None

    def test_the_capture_path_needs_the_token_and_a_number_of_frames(self, tmp_path, monkeypatch):
        fixture = DevelopmentFixture(tmp_path, monkeypatch)
        server = fixture.server()
        site = SiteServer(tmp_path, "127.0.0.1", 0, development=server)
        threading.Thread(target=site.serve, daemon=True).start()

        try:
            for token, body, status in (("wrong", b"{}", 403), (server.token, b'{"frames": 0}', 400), (server.token, b'{"frames": 1}', 200)):
                connection = http.client.HTTPConnection("127.0.0.1", site.port, timeout=5)
                connection.request("POST", f"{DevelopmentServer.GPU_CAPTURE_PATH}?token={token}", body=body)
                assert connection.getresponse().status == status
                connection.close()
        finally:
            site.shutdown()
            site.close()

    def test_an_app_whose_connection_resets_leaves_and_the_server_goes_on(self, tmp_path, monkeypatch):
        fixture = DevelopmentFixture(tmp_path, monkeypatch)
        server = fixture.server()
        hello = DevelopmentFixture.hello(server.session, server.revision)

        class Resetting(io.BytesIO):
            def read(self, size: int | None = -1) -> bytes:
                data = super().read(size)
                if not data:
                    raise ConnectionResetError(54, "Connection reset by peer")
                return data

        headers = {"Sec-WebSocket-Key": DevelopmentFixture.HELLO_KEY, "Upgrade": "websocket"}
        handler = mock.Mock(path=f"{DevelopmentServer.PATH}?token={server.token}", headers=headers, wfile=io.BytesIO(), rfile=Resetting(bytes([0x81, len(hello)]) + hello))
        server.accept(handler)

        assert server.clients == []
        assert "An app left the development server." in fixture.output.getvalue()

    def test_connections_need_the_token_and_receive_every_batch(self, tmp_path, monkeypatch):
        fixture = DevelopmentFixture(tmp_path, monkeypatch)
        server = fixture.server()
        site = SiteServer(tmp_path, "127.0.0.1", 0, development=server)
        threading.Thread(target=site.serve, daemon=True).start()

        try:
            refused, _, status = DevelopmentFixture.connect(site.port, "wrong")
            refused.close()
            assert "403" in status

            connection, stream, status = DevelopmentFixture.connect(site.port, server.token)
            assert "101" in status
            connection.sendall(DevelopmentFixture.masked(WebSocket.TEXT, DevelopmentFixture.hello(server.session, server.revision)))
            DevelopmentFixture.wait(lambda: bool(server.clients))
            fixture.write("source/level.lua", "return {pushed = true}")
            server.broadcast(*server.scan())

            final, opcode, message = WebSocket.read_frame(stream)
            assert (final, opcode) == (True, WebSocket.TEXT)
            assert json.loads(message)["files"] == [{"path": "source/level.lua", "size": 22}]
            assert WebSocket.read_frame(stream) == (True, WebSocket.BINARY, b"return {pushed = true}")
            stream.close()
            connection.close()
        finally:
            site.shutdown()
            site.close()

    def test_a_message_that_is_not_json_is_reported_and_the_next_one_arrives(self, tmp_path, monkeypatch):
        fixture = DevelopmentFixture(tmp_path, monkeypatch)
        server = fixture.server()
        received = []
        monkeypatch.setattr(server, "receive", lambda client, message: received.append(message))
        stream = io.BytesIO(WebSocket.frame(WebSocket.TEXT, b"{broken") + WebSocket.frame(WebSocket.TEXT, b'{"type": "report"}'))

        server.serve(mock.Mock(), stream)

        assert received == [{"type": "report"}]
        assert "An app sent the development server a message that is not JSON, which the server skips." in fixture.output.getvalue()
