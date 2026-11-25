import re
from pathlib import Path

import pytest

from haylen.core.errors.invalid_manifest import InvalidManifest
from haylen.platforms.desktop.desktop_backend import DesktopBackend
from haylen.platforms.desktop.desktop_project import DesktopProject
from tests.platforms.desktop.desktop_fixture import DesktopFixture


class DesktopSettingsTest:
    """The settings of the Windows and Linux projects that the app chooses through `platforms.windows` and `platforms.linux` of `haylen.json`: the name of the executable, the console subsystem, the execution level, the oldest Windows of the installer, and Linux, which takes no oldest system version."""

    @pytest.fixture
    def fixture(self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> DesktopFixture:
        return DesktopFixture(tmp_path, monkeypatch)

    @staticmethod
    def project(fixture: DesktopFixture, app: Path, target: str) -> Path:
        product = fixture.resolve(app, target)
        return DesktopBackend(fixture.context).prepare(product, DesktopFixture.folders(product)).project

    def test_a_windows_app_runs_in_the_windows_subsystem_without_elevation_by_default(self, fixture):
        app = fixture.app({}, "garden")
        minimum = fixture.resolve(app, "windows").primary.minimum_system_version.value
        project = DesktopSettingsTest.project(fixture, app, "windows")
        values = (project / "app.cmake").read_text(encoding="utf-8")
        assert "haylen_add_app(main LUA CONTENT" in values
        assert "set(HAYLEN_APP_EXECUTABLE [[garden]])" in values
        assert f"set(HAYLEN_APP_MINIMUM_SYSTEM [[{minimum}]])" in values
        assert 'level="asInvoker"' in (project / "resources" / "app.manifest").read_text(encoding="utf-8")
        assert 'set(CPACK_INNOSETUP_SETUP_MinVersion "${HAYLEN_APP_MINIMUM_SYSTEM}")' in (project / "CMakeLists.txt").read_text(encoding="utf-8")

    def test_a_windows_app_takes_its_executable_its_console_and_its_execution_level(self, fixture):
        windows = {"executableName": "sky-tool", "console": True, "executionLevel": "requireAdministrator", "minimumSystemVersion": "10.0.22000"}
        project = DesktopSettingsTest.project(fixture, fixture.app({"platforms": {"windows": windows}}, "garden"), "windows")
        values = (project / "app.cmake").read_text(encoding="utf-8")
        assert "haylen_add_app(main LUA CONSOLE CONTENT" in values
        assert "set(HAYLEN_APP_EXECUTABLE [[sky-tool]])" in values
        assert "set(HAYLEN_APP_MINIMUM_SYSTEM [[10.0.22000]])" in values
        assert 'level="requireAdministrator"' in (project / "resources" / "app.manifest").read_text(encoding="utf-8")

    def test_a_manifest_of_the_app_keeps_its_level_unless_the_key_contradicts_it(self, fixture):
        app = fixture.app({"platforms": {"windows": {"files": {"resources/app.manifest": "windows/app.manifest"}}}}, "own")
        (app / "windows").mkdir()
        template = (DesktopProject.template("windows") / "resources" / "app.manifest").read_text(encoding="utf-8")
        (app / "windows" / "app.manifest").write_text(template.replace('level="asInvoker"', 'level="highestAvailable"'), encoding="utf-8")
        assert 'level="highestAvailable"' in (DesktopSettingsTest.project(fixture, app, "windows") / "resources" / "app.manifest").read_text(encoding="utf-8")

        conflicting = fixture.app({"platforms": {"windows": {"executionLevel": "asInvoker", "files": {"resources/app.manifest": "windows/app.manifest"}}}}, "conflicting")
        (conflicting / "windows").mkdir()
        (conflicting / "windows" / "app.manifest").write_text(template.replace('level="asInvoker"', 'level="highestAvailable"'), encoding="utf-8")
        with pytest.raises(InvalidManifest, match=re.escape('asks for the level "highestAvailable", while "platforms.windows.executionLevel" of')):
            DesktopSettingsTest.project(fixture, conflicting, "windows")

    def test_a_linux_app_takes_its_executable_and_no_oldest_system_version(self, fixture):
        project = DesktopSettingsTest.project(fixture, fixture.app({"platforms": {"linux": {"executableName": "sky-tool"}}}, "garden"), "linux")
        assert "Exec=sky-tool" in (project / "resources" / "app.desktop").read_text(encoding="utf-8").splitlines()

        app = fixture.app({"platforms": {"linux": {"minimumSystemVersion": "2.31"}}}, "versioned")
        with pytest.raises(InvalidManifest, match=re.escape('so "platforms.linux.minimumSystemVersion" of')):
            fixture.resolve(app, "linux")
