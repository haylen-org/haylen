import re

import pytest

from haylen.content.key_folder import KeyFolder
from haylen.core.errors.missing_keys import MissingKeys
from haylen.core.repository import Repository
from tests.content.content_fixture import ContentFixture


class KeyFolderTest:
    def test_keys_live_outside_every_project_unless_continuous_integration_names_their_folder(self, tmp_path, monkeypatch):
        fixture = ContentFixture(tmp_path, monkeypatch)

        own = KeyFolder.of("com.example.game", fixture.context.environment)
        received = KeyFolder.of("com.example.game", {KeyFolder.VARIABLE: str(tmp_path / "secrets")})

        assert own.folder == KeyFolder.user_config_folder(fixture.context.environment) / "keys" / "com.example.game"
        assert not own.folder.is_relative_to(Repository.ROOT)
        assert not own.received
        assert received.folder == tmp_path / "secrets" / "com.example.game"
        assert received.received

    def test_continuous_integration_receives_keys_and_never_creates_them(self, tmp_path, monkeypatch):
        fixture = ContentFixture(tmp_path, monkeypatch)
        keys = KeyFolder.of("com.example.game", {KeyFolder.VARIABLE: str(tmp_path / "secrets")})

        with pytest.raises(MissingKeys, match=re.escape('holds no keys of "com.example.game"')):
            keys.ensure(fixture.tool)
        keys.folder.mkdir(parents=True)
        (keys.folder / KeyFolder.INDEX).write_text("{}", encoding="utf-8")

        assert keys.ensure(fixture.tool) == keys.folder
        assert fixture.tool.commands == []

    def test_the_machine_of_the_developer_creates_the_keys_once_and_says_where_they_live(self, tmp_path, monkeypatch):
        fixture = ContentFixture(tmp_path, monkeypatch)
        keys = KeyFolder.of("com.example.game", fixture.context.environment)

        keys.ensure(fixture.tool)
        keys.ensure(fixture.tool)

        assert fixture.tool.commands == [["keys", "create", str(keys.folder), "--identifier", "com.example.game"]]
        assert "Back up this folder" in fixture.output

    def test_the_secrets_are_every_content_key_and_the_signing_key(self, tmp_path, monkeypatch):
        keys = KeyFolder.of("com.example.game", {KeyFolder.VARIABLE: str(tmp_path)})
        keys.folder.mkdir()
        (keys.folder / KeyFolder.INDEX).write_text('{"contentKeys": ["1234", "5678"]}', encoding="utf-8")
        (keys.folder / "content-1234.key").write_bytes(bytes(range(32)))
        (keys.folder / "content-5678.key").write_bytes(bytes(range(32, 64)))
        (keys.folder / KeyFolder.SIGNING_KEY).write_bytes(bytes(range(64, 96)))

        assert keys.secrets() == {
            'the content key "1234"': bytes(range(32)),
            'the content key "5678"': bytes(range(32, 64)),
            "the signing key": bytes(range(64, 96)),
        }
        assert KeyFolder.of("com.example.other", {KeyFolder.VARIABLE: str(tmp_path)}).secrets() == {}
