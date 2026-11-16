import http.client
import socket
import threading

import pytest

from haylen.cli.application import Application
from haylen.core.errors.development_failed import DevelopmentFailed
from haylen.development.site_server import SiteServer


class SiteServerTest:
    """The server of web sites serves the shards of a protected release by byte range with a strong validator, as a content delivery network does, and every connection that a page opens at once."""

    @pytest.fixture
    def site(self, tmp_path):
        (tmp_path / "app").mkdir()
        (tmp_path / "app" / "shard.hpak").write_bytes(bytes(range(256)) * 4)
        server = SiteServer(tmp_path, "127.0.0.1", 0)
        threading.Thread(target=server.serve, daemon=True).start()
        yield server
        server.shutdown()
        server.close()

    @staticmethod
    def get(server: SiteServer, headers: dict[str, str]) -> tuple[http.client.HTTPResponse, bytes]:
        connection = http.client.HTTPConnection("127.0.0.1", server.port, timeout=5)
        connection.request("GET", "/app/shard.hpak", headers=headers)
        response = connection.getresponse()
        body = response.read()
        connection.close()
        return response, body

    def test_serves_every_connection_that_a_page_opens_while_the_server_is_busy(self, tmp_path):
        (tmp_path / "app.zip").write_bytes(b"z" * 200_000)
        server = SiteServer(tmp_path, "127.0.0.1", 0)

        # The page opens its connections while the server is too busy to accept them, as on a machine under load, and the listen queue holds every one until the server takes it.
        statuses = []
        connections = []
        for _ in range(48):
            try:
                connection = socket.create_connection(("127.0.0.1", server.port), timeout=10)
                connection.sendall(b"GET /app.zip HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n")
                connections.append(connection)
            except OSError as error:
                statuses.append(type(error).__name__.encode())
        threading.Thread(target=server.serve, daemon=True).start()

        try:
            for connection in connections:
                try:
                    response = b""
                    while chunk := connection.recv(1 << 16):
                        response += chunk
                    statuses.append(response.split(b"\r\n", 1)[0] if len(response) > 200_000 else b"short")
                except OSError as error:
                    statuses.append(type(error).__name__.encode())
                connection.close()
        finally:
            server.shutdown()
            server.close()
        assert statuses == [b"HTTP/1.0 200 OK"] * 48

    def test_serves_ranges_and_the_whole_file_when_the_file_changed(self, site):
        whole, body = SiteServerTest.get(site, {})
        assert whole.status == 200
        assert len(body) == 1024
        validator = whole.getheader("ETag")
        assert validator.startswith('"')

        part, body = SiteServerTest.get(site, {"Range": "bytes=10-19"})
        assert part.status == 206
        assert part.getheader("Content-Range") == "bytes 10-19/1024"
        assert body == bytes(range(10, 20))

        resumed, body = SiteServerTest.get(site, {"Range": "bytes=1000-", "If-Range": validator})
        assert (resumed.status, len(body)) == (206, 24)
        changed, body = SiteServerTest.get(site, {"Range": "bytes=1000-", "If-Range": '"other"'})
        assert (changed.status, len(body)) == (200, 1024)
        beyond, _ = SiteServerTest.get(site, {"Range": "bytes=5000-"})
        assert beyond.status == 416

    def test_sends_no_isolation_policy_that_blocks_the_scripts_of_plugins(self, site):
        response, _ = SiteServerTest.get(site, {})
        assert response.getheader("Cross-Origin-Embedder-Policy") is None
        assert response.getheader("Cross-Origin-Opener-Policy") is None

        parser, _ = Application.parser()
        assert parser.parse_args(["serve", "site"]).coep == "off"

    def test_a_port_that_something_else_uses_fails_with_what_to_do(self, site, tmp_path):
        with pytest.raises(DevelopmentFailed, match='pick another one with "--port"'):
            SiteServer(tmp_path, "127.0.0.1", site.port)
