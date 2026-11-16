import functools
import http.client
import threading

import pytest

from haylen.core.errors.development_failed import DevelopmentFailed
from haylen.development.development_server import DevelopmentServer
from haylen.development.local_server import LocalServer
from haylen.development.package_handler import PackageHandler
from haylen.development.package_server import PackageServer
from tests.development.development_fixture import DevelopmentFixture


class PackageServerTest:
    ADDRESS = "ws://192.168.1.20:8000/haylen/development?token=0123456789ab"

    def test_the_zip_needs_the_token_and_names_the_development_server(self, tmp_path, monkeypatch):
        fixture = DevelopmentFixture(tmp_path, monkeypatch)
        archive = tmp_path / "app.zip"
        fixture.layout.zip(archive)
        development = DevelopmentServer(fixture.context, fixture.layout, DevelopmentServer.archive_manifest(archive))
        handler = functools.partial(PackageHandler, development=development, archive=archive, development_address=PackageServerTest.ADDRESS)
        server = LocalServer(("127.0.0.1", 0), handler)
        threading.Thread(target=server.serve_forever, daemon=True).start()

        def get(path: str) -> tuple[http.client.HTTPResponse, bytes]:
            connection = http.client.HTTPConnection("127.0.0.1", server.server_address[1], timeout=5)
            connection.request("GET", path)
            response = connection.getresponse()
            body = response.read()
            connection.close()
            return response, body

        try:
            response, body = get(f"/haylen/{development.token}/app.zip")
            assert response.status == 200
            assert body == archive.read_bytes()
            assert response.getheader(PackageHandler.DEVELOPMENT_HEADER) == PackageServerTest.ADDRESS
            assert response.getheader("Access-Control-Expose-Headers") == PackageHandler.DEVELOPMENT_HEADER
            assert get("/haylen/0123456789ab/app.zip")[0].status == 403
            assert get(f"/haylen/app.zip?token={development.token}")[0].status == 404
            assert get("/haylen.json")[0].status == 404
        finally:
            server.shutdown()
            server.server_close()
        assert development.manifest == fixture.manifest

    def test_only_the_package_of_a_lua_app_is_served(self, tmp_path, monkeypatch):
        fixture = DevelopmentFixture(tmp_path, monkeypatch)
        cpp = fixture.tooling.app({"kind": "cpp", "cpp": {"sources": ["source"]}}, "cpp")

        with pytest.raises(DevelopmentFailed, match=r"is a C\+\+ app, which has no package to serve"):
            PackageServer(fixture.context, cpp, "127.0.0.1", 0).serve()
