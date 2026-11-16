from pathlib import Path

import pytest

from haylen.core.app_manifest import AppManifest
from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.platforms.desktop.desktop_app import DesktopApp
from haylen.platforms.desktop.desktop_backend import DesktopBackend
from haylen.platforms.desktop.desktop_links import DesktopLinks
from tests.platforms.desktop.desktop_fixture import DesktopFixture


class DesktopLinkTest:
    """The URL schemes whose links open a Windows or Linux app, which plugins declare as `urlScheme` contributions, reach the installer of Windows and the desktop entry of Linux."""

    @pytest.fixture
    def fixture(self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> DesktopFixture:
        return DesktopFixture(tmp_path, monkeypatch)

    def app(self, fixture: DesktopFixture) -> Path:
        app = fixture.app({"name": 'Tiny "Island"', "identifier": "com.haylen.tinyisland", "plugins": {"links": {}}}, "tiny-island")
        contributions = [{"type": "urlScheme", "platforms": ["windows", "linux"], "scheme": "${scheme}"}, {"type": "urlScheme", "platforms": ["windows", "linux"], "scheme": "island"}]
        parameters = {"scheme": {"type": "string", "default": "island-links", "description": "The scheme."}}
        fixture.plugin(app, "links", {"windows": "native", "linux": "native"}, contributions, parameters=parameters)
        return app

    def test_plugins_declare_the_url_schemes_of_their_links_on_the_desktops(self, fixture: DesktopFixture):
        product = fixture.resolve(self.app(fixture), "linux")
        assert DesktopLinks.schemes(product.primary) == ("island-links", "island")

    def test_a_url_scheme_has_the_form_of_a_scheme(self, fixture: DesktopFixture):
        app = fixture.app({"plugins": {"links": {}}}, "island")
        fixture.plugin(app, "links", {"linux": "native"}, [{"type": "urlScheme", "platforms": ["linux"], "scheme": "1island"}])
        with pytest.raises(UnsupportedTarget, match='The key "scheme" has the value "1island", which does not have the form the key takes'):
            fixture.resolve(app, "linux")

    def test_the_installer_of_windows_registers_each_scheme_with_the_executable_and_the_link(self, fixture: DesktopFixture):
        app = DesktopApp.of(AppManifest.load(self.app(fixture)))
        script = DesktopLinks.installer_script(app, ("island",))
        assert 'Root: HKA; Subkey: "Software\\Classes\\island"; ValueType: string; ValueName: ""; ValueData: "URL:Tiny ""Island"""; Flags: uninsdeletekey' in script
        assert 'Subkey: "Software\\Classes\\island\\shell\\open\\command"; ValueType: string; ValueName: ""; ValueData: """{app}\\tiny-island.exe"" --link ""%1"""' in script

        product = fixture.resolve(self.app(fixture), "windows")
        project = DesktopBackend(fixture.context).prepare(product, DesktopFixture.folders(product)).project
        assert (project / DesktopLinks.SCRIPT).read_text(encoding="utf-8").count("shell\\open\\command") == 2
        assert 'set(CPACK_INNOSETUP_EXTRA_SCRIPTS "${CMAKE_CURRENT_LIST_DIR}/links.iss")' in (project / "app.cmake").read_text(encoding="utf-8")

    def test_the_desktop_entry_of_linux_takes_the_links_of_its_schemes(self, fixture: DesktopFixture):
        product = fixture.resolve(self.app(fixture), "linux")
        project = DesktopBackend(fixture.context).prepare(product, DesktopFixture.folders(product)).project
        entry = (project / "resources" / "app.desktop").read_text(encoding="utf-8").splitlines()
        assert "Exec=tiny-island --link %u" in entry
        assert "MimeType=x-scheme-handler/island-links;x-scheme-handler/island;" in entry
        assert not (project / DesktopLinks.SCRIPT).exists()
        assert "CPACK_INNOSETUP" not in (project / "app.cmake").read_text(encoding="utf-8")
