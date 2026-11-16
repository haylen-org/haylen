import re

import pytest

from haylen.core.errors.invalid_option import InvalidOption
from haylen.core.errors.missing_keys import MissingKeys
from haylen.platforms.android.android_signing_key import AndroidSigningKey
from haylen.platforms.android.java_properties import JavaProperties
from haylen.platforms.signing_request import SigningRequest
from tests.platforms.android.android_fixture import AndroidFixture


class AndroidSigningKeyTest:
    def test_properties_name_the_keystore_next_to_them_and_escape_the_values(self):
        lines = AndroidSigningKey.properties("release", "upload", "pa=ss✓").splitlines()

        assert lines[0].startswith("# ")
        assert lines[1:] == ["storeFile=release.jks", "storePassword=pa=ss\\u2713", "keyAlias=upload", "keyPassword=pa=ss\\u2713"]

    def test_values_survive_as_ascii_with_escapes(self):
        assert JavaProperties.escape("Ilha Tropical ✓\\n") == "Ilha Tropical \\u2713\\\\n"
        assert JavaProperties.read(f"# A comment.\nkeyPassword={JavaProperties.escape('pä\\ss😀 ')}\n") == {"keyPassword": "pä\\ss😀 "}, "A value keeps its trailing spaces, as Java reads it."

    def test_the_key_comes_from_the_app_then_the_gradle_home_then_the_environment(self, tmp_path):
        app = tmp_path / "app"
        files = AndroidSigningKey.files(app, "release")
        files["properties"].parent.mkdir(parents=True)
        files["properties"].write_text(AndroidSigningKey.properties("release", "upload", "secret"), encoding="utf-8")
        variables = {"HAYLEN_RELEASE_STORE_FILE": "keys/ci.jks", "HAYLEN_RELEASE_STORE_PASSWORD": "store", "HAYLEN_RELEASE_KEY_ALIAS": "ci", "HAYLEN_RELEASE_KEY_PASSWORD": "key"}

        own = AndroidSigningKey.release(app, {**variables, "HOME": str(tmp_path / "home")})
        assert (own.store_file, own.store_password, own.key_alias) == (files["keystore"], "secret", "upload")

        files["properties"].unlink()
        home = tmp_path / "gradle-home"
        home.mkdir()
        (home / "gradle.properties").write_text("haylen.release.storeFile=keys/home.jks\nhaylen.release.storePassword=home\nhaylen.release.keyAlias=upload\n", encoding="utf-8")
        mixed = AndroidSigningKey.release(app, {**variables, "GRADLE_USER_HOME": str(home)})
        assert (mixed.store_file, mixed.store_password, mixed.key_alias, mixed.key_password) == (app / "keys" / "home.jks", "home", "upload", "key")

        given = AndroidSigningKey.release(app, {**variables, "GRADLE_USER_HOME": str(tmp_path / "empty")})
        assert given.environment() == {**variables, "HAYLEN_RELEASE_STORE_FILE": str(app / "keys" / "ci.jks")}

    def test_a_release_without_a_key_stops_with_the_ways_to_give_one(self, tmp_path):
        with pytest.raises(MissingKeys, match=re.escape('needs the upload key of the app. Create it with "haylen app sign --target android --kind release"')):
            AndroidSigningKey.release(tmp_path / "app", {"GRADLE_USER_HOME": str(tmp_path / "empty")})

    def test_a_short_password_or_an_existing_key_stops_before_anything_changes(self, tmp_path):
        fixture = AndroidFixture(tmp_path)
        app = fixture.app({})

        with pytest.raises(InvalidOption, match="at least 6 characters"):
            fixture.backend.sign(SigningRequest(app, "release", {"password": "short"}))
        assert not (app / "platform").exists()

        keystore = AndroidSigningKey.files(app, "release")["keystore"]
        keystore.parent.mkdir(parents=True)
        keystore.write_bytes(b"key")
        for force in ({}, {"force": "false"}):
            with pytest.raises(InvalidOption, match=r'already has a release key.*"--option force=true"'):
                fixture.backend.sign(SigningRequest(app, "release", force))
        with pytest.raises(InvalidOption, match=re.escape('The option "force" is "true" or "false", not "yes".')):
            fixture.backend.sign(SigningRequest(app, "release", {"force": "yes"}))
        assert keystore.read_bytes() == b"key"
        with pytest.raises(InvalidOption, match='signs with a "release" or a "debug" key'):
            fixture.backend.sign(SigningRequest(app, "store", {}))
