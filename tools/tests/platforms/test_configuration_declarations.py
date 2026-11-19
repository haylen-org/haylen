import json
import re
from pathlib import Path

import pytest

from haylen.content.protected_releases import ProtectedReleases
from haylen.core.configuration import Configuration
from haylen.platforms.android.android_backend import AndroidBackend
from haylen.platforms.desktop.desktop_backend import DesktopBackend
from haylen.platforms.product_folders import ProductFolders
from haylen.platforms.web.web_backend import WebBackend
from tests.content.content_fixture import ContentFixture
from tests.platforms.android.android_fixture import AndroidFixture
from tests.platforms.apple.apple_fixture import AppleFixture
from tests.platforms.desktop.desktop_fixture import DesktopFixture


class ConfigurationDeclarationsTest:
    """A Debug and a Release build of an app declare and integrate the same: the permissions, the keys of the property lists, the entitlements, the privacy entries, the entries of the Android manifest, the desktop entries and the page of the web. The two differ only in what development is, so these tests prepare one app with a plugin in both configurations and compare what each project declares."""

    # The activity of an Android release loads the library that compiles the bootstrap of its protected release, while a Debug build loads the player of the SDK, so that one value names the library of each configuration.
    ANDROID_LIBRARY = re.compile(r'(android:name="android\.app\.lib_name"\s+android:value=")[^"]*(")')

    @staticmethod
    def prepare_both(prepare) -> tuple[dict[str, bytes], dict[str, bytes]]:
        return prepare(Configuration.DEBUG), prepare(Configuration.RELEASE)

    @staticmethod
    def read(folder: Path, names: tuple[str, ...]) -> dict[str, bytes]:
        return {name: (folder / name).read_bytes() for name in names if (folder / name).is_file()}

    def test_apple_projects_declare_the_same_in_both_configurations(self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch):
        fixture = AppleFixture(tmp_path / "apple", monkeypatch)
        content = ContentFixture(tmp_path / "content", monkeypatch)
        monkeypatch.setattr(ProtectedReleases, "locate", staticmethod(lambda context, registry: content.releases))
        app = fixture.app({"plugins": {"scanner": {}}, "platforms": {"apple": {"team": "ABCDE12345"}}})
        contributions = [
            {"type": "infoPlist", "platforms": ["ios"], "values": {"NSCameraUsageDescription": "The app scans codes."}},
            {"type": "entitlements", "platforms": ["ios"], "values": {"com.apple.developer.associated-domains": ["applinks:haylen.dev"]}},
        ]
        fixture.plugin(app, "scanner", {"implementations": {"ios": "native"}, "contributions": contributions})
        files = ("Info.plist", "App.entitlements", "PrivacyInfo.xcprivacy")

        def prepare(configuration: Configuration) -> dict[str, bytes]:
            product = fixture.resolve(app, "ios", configuration.value)
            folders = ProductFolders(tmp_path / f"apple-{configuration.value}")
            prepared = fixture.backend.prepare(product, folders)
            spec = AppleFixture.spec(prepared)
            declared = ConfigurationDeclarationsTest.read(prepared.folders.project, files)
            declared["project.yml"] = json.dumps(
                {"targets": {name: {key: value for key, value in target.items() if key in ("info", "entitlements", "dependencies")} for name, target in spec["targets"].items()}}, sort_keys=True
            ).encode()
            return declared

        debug, release = ConfigurationDeclarationsTest.prepare_both(prepare)
        assert set(debug) >= {"Info.plist", "project.yml"}
        assert debug == release

    def test_android_projects_declare_the_same_in_both_configurations(self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch):
        fixture = AndroidFixture(tmp_path / "android")
        content = ContentFixture(tmp_path / "content", monkeypatch)
        monkeypatch.setattr(AndroidBackend, "releases", lambda backend: content.releases)
        app = fixture.app({"plugins": {"scanner": {}}, "platforms": {"android": {"manifest": {"permissions": ["android.permission.CAMERA"]}}}})
        fixture.plugin(app, "scanner", {})

        def prepare(configuration: Configuration) -> dict[str, bytes]:
            prepared = fixture.prepare(fixture.resolve(app, configuration.value), f"android-{configuration.value}")
            manifest = (prepared.project / "app" / "src" / "main" / "AndroidManifest.xml").read_text(encoding="utf-8")
            return {"AndroidManifest.xml": ConfigurationDeclarationsTest.ANDROID_LIBRARY.sub(r"\1library\2", manifest).encode()}

        debug, release = ConfigurationDeclarationsTest.prepare_both(prepare)
        assert b"android.permission.CAMERA" in debug["AndroidManifest.xml"]
        assert debug == release

    @pytest.mark.parametrize(
        ("target", "files"),
        [
            ("windows", ("resources/app.manifest", "resources/app.rc", "links.iss")),
            ("linux", ("resources/app.desktop", "resources/app.metainfo.xml")),
            ("web", ("index.html", "loader.css", "config.json")),
        ],
    )
    def test_desktop_and_web_projects_declare_the_same_in_both_configurations(self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch, target: str, files: tuple[str, ...]):
        fixture = DesktopFixture(tmp_path / "desktop", monkeypatch)
        content = ContentFixture(tmp_path / "content", monkeypatch)
        backend_type = WebBackend if target == "web" else DesktopBackend
        monkeypatch.setattr(backend_type, "releases", lambda backend: content.releases)
        app = fixture.app({"plugins": {"garden": {}}}, "garden")
        contributions = [{"type": "urlScheme", "platforms": ["windows", "linux"], "scheme": "garden"}, {"type": "webModule", "platforms": ["web"], "path": "web/garden.js"}]
        fixture.plugin(app, "garden", {"windows": "native", "linux": "native", "web": "native"}, contributions, {"web/garden.js": "export default function load(context) {}\n"})

        def prepare(configuration: Configuration) -> dict[str, bytes]:
            product = fixture.resolve(app, target, configuration)
            folders = ProductFolders(tmp_path / f"{target}-{configuration.value}")
            prepared = backend_type(fixture.context).prepare(product, folders)
            return ConfigurationDeclarationsTest.read(prepared.folders.project, files)

        debug, release = ConfigurationDeclarationsTest.prepare_both(prepare)
        assert debug
        assert debug == release
