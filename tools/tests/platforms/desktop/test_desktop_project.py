import json
import xml.etree.ElementTree as ElementTree
from pathlib import Path
from types import MappingProxyType

import pytest

from haylen.core.app_category import AppCategory
from haylen.core.app_manifest import AppManifest
from haylen.core.configuration import Configuration
from haylen.core.errors.invalid_manifest import InvalidManifest
from haylen.core.repository import Repository
from haylen.native.cmake_app import CMakeApp
from haylen.platforms.build_product import BuildProduct
from haylen.platforms.desktop.desktop_app import DesktopApp
from haylen.platforms.desktop.desktop_backend import DesktopBackend
from haylen.platforms.desktop.desktop_layout import DesktopLayout
from haylen.platforms.desktop.desktop_verifier import DesktopVerifier
from haylen.platforms.desktop.linux_categories import LinuxCategories
from haylen.platforms.desktop.linux_entries import LinuxEntries
from haylen.platforms.desktop.windows_resources import WindowsResources
from haylen.platforms.package_request import PackageRequest
from haylen.platforms.preparation_state import PreparationState
from haylen.platforms.prepared_project import PreparedProject
from haylen.platforms.verification_check import VerificationCheck
from tests.platforms.desktop.desktop_fixture import DesktopFixture


class DesktopProjectTest:
    """The CMake project of a Windows or Linux app, which the desktop backend generates whole from the template of its platform and the values of `haylen.json`."""

    @pytest.fixture
    def fixture(self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> DesktopFixture:
        return DesktopFixture(tmp_path, monkeypatch)

    def prepare(self, fixture: DesktopFixture, app: Path, target: str) -> Path:
        product = fixture.resolve(app, target)
        return DesktopBackend(fixture.context).prepare(product, DesktopFixture.folders(product)).project

    def test_the_publisher_follows_the_name_and_the_executable_follows_the_folder(self, fixture: DesktopFixture):
        app = DesktopApp.of(AppManifest.load(fixture.app({"name": "Sky Garden"}, "Sky Garden")))
        assert (app.description, app.company, app.copyright, app.website) == ("Sky Garden", "Sky Garden", "Copyright © Sky Garden", "")
        assert app.executable == "sky-garden"

        app = DesktopApp.of(AppManifest.load(fixture.app({"name": "Sky Garden", "company": "Harbor Games", "website": "https://harbor.example"}, "garden")))
        assert (app.company, app.copyright, app.website) == ("Harbor Games", "Copyright © Harbor Games", "https://harbor.example")

    @pytest.mark.parametrize("name", ["app", "Lib"])
    def test_a_linux_executable_takes_no_name_of_the_folders_next_to_it(self, fixture: DesktopFixture, name: str):
        with pytest.raises(InvalidManifest, match=f'gives the executable "{name.lower()}", which is the name of a folder next to the executable of a Linux app'):
            self.prepare(fixture, fixture.app({}, name), "linux")

    def test_values_reach_cmake_and_resource_scripts_as_they_are(self, fixture: DesktopFixture):
        assert CMakeApp.text("Sky Garden") == "[[Sky Garden]]"
        assert CMakeApp.text('A "quoted" ${name}; with ]] inside') == '[=[A "quoted" ${name}; with ]] inside]=]'
        assert WindowsResources.text('The "Garden" \\ Sky') == '"The ""Garden"" \\\\ Sky"'

        project = self.prepare(fixture, fixture.app({"name": "Sky ]] Garden"}, "garden"), "windows")
        values = (project / "app.cmake").read_text(encoding="utf-8")
        assert "set(HAYLEN_APP_NAME [=[Sky ]] Garden]=])" in values
        assert "set(HAYLEN_MSVC_RUNTIME [[MultiThreadedDLL]])" in values
        assert 'haylen_add_app(main LUA CONTENT "${HAYLEN_APP_CONTENT}")' in values

    def test_the_version_resource_takes_the_values_of_haylen_json(self, fixture: DesktopFixture):
        app = DesktopApp.of(AppManifest.load(fixture.app({"name": "Sky Garden", "version": "1.2"}, "sky-garden")))
        header = WindowsResources.header(app, "haylen.json")
        assert '#define HAYLEN_APP_EXECUTABLE "sky-garden.exe"' in header
        assert '#define HAYLEN_APP_COPYRIGHT "Copyright © Sky Garden"' in header
        assert "#define HAYLEN_APP_VERSION_NUMBERS 1, 2, 0, 1" in header
        assert '#define HAYLEN_APP_VERSION "1.2"' in header

        # The fourth number is the build number, and the resource holds numbers of 16 bits alone.
        app = DesktopApp.of(AppManifest.load(fixture.app({"version": "1.2", "build": 37}, "built")))
        assert "#define HAYLEN_APP_VERSION_NUMBERS 1, 2, 0, 37" in WindowsResources.header(app, "haylen.json")
        app = DesktopApp.of(AppManifest.load(fixture.app({"version": "1.2", "build": 65536}, "large")))
        with pytest.raises(InvalidManifest, match=r'The version resource of Windows holds numbers up to 65535, and the "version" "1\.2" or the "build" "65536" of'):
            WindowsResources.header(app, "haylen.json")

    def test_the_desktop_entry_of_the_app_wins_and_gains_what_it_lacks(self):
        entries = LinuxEntries()
        own = "# A comment.\n[Desktop Entry]\nType=Application\nName=Garden\n\n[Desktop Action New]\nName=New\n"
        completed = entries.complete_entry(own, {"Name": "Sky Garden", "Comment": "Tab\tand\nline", "Exec": "sky-garden"}, "app.desktop")
        assert completed == "# A comment.\n[Desktop Entry]\nType=Application\nName=Garden\nComment=Tab\\tand\\nline\nExec=sky-garden\n\n[Desktop Action New]\nName=New\n"
        assert entries.warnings == ['The key "Name" of `app.desktop` keeps its value "Garden", while "haylen.json" gives "Sky Garden".']
        with pytest.raises(InvalidManifest, match='has no "\\[Desktop Entry\\]" group'):
            entries.complete_entry("[Desktop Action New]\n", {}, "app.desktop")

    def test_the_metainfo_of_the_app_wins_and_gains_the_elements_of_haylen_json(self, fixture: DesktopFixture):
        app = DesktopApp.of(AppManifest.load(fixture.app({"name": "Sky Garden", "identifier": "com.haylen.skygarden"}, "garden")))
        entries = LinuxEntries()
        own = '<?xml version="1.0"?>\n<component type="desktop-application"><name>Garden</name><project_license>MIT</project_license></component>'
        completed = entries.complete_metainfo(own, LinuxEntries.elements(app), "app.metainfo.xml")
        component = ElementTree.fromstring(completed.split("\n", 1)[1])
        assert [child.tag for child in component][:5] == ["id", "summary", "description", "developer", "launchable"]
        assert component.findtext("name") == "Garden"
        assert component.find("developer").get("id") == "com.haylen"
        assert component.find("url") is None
        assert component.find("launchable").text == "com.haylen.skygarden.desktop"
        assert entries.warnings == ['The element "name" of `app.metainfo.xml` keeps its value "Garden", while "haylen.json" gives "Sky Garden".']
        with pytest.raises(InvalidManifest, match="is not valid XML"):
            entries.complete_metainfo("<component>", [], "app.metainfo.xml")

    def test_the_category_reaches_the_desktop_entry_the_metainfo_and_the_debian_section(self, fixture: DesktopFixture):
        project = self.prepare(fixture, fixture.app({"category": "music"}, "radio"), "linux")
        entry = (project / "resources" / "app.desktop").read_text(encoding="utf-8").splitlines()
        metainfo = ElementTree.fromstring((project / "resources" / "app.metainfo.xml").read_text(encoding="utf-8").split("\n", 1)[1])
        values = (project / "app.cmake").read_text(encoding="utf-8")

        assert [line for line in entry if line.startswith("Categories=")] == ["Categories=AudioVideo;Audio;"]
        assert [category.text for category in metainfo.find("categories")] == ["AudioVideo", "Audio"]
        assert "set(HAYLEN_APP_DEBIAN_SECTION [[sound]])" in values
        assert set(LinuxCategories.MENU) == set(LinuxCategories.DEBIAN_SECTIONS) == set(AppCategory)

    def test_an_app_without_a_category_lists_none_and_takes_the_debian_section_of_the_rest(self, fixture: DesktopFixture):
        project = self.prepare(fixture, fixture.app({}, "notes"), "linux")
        entry = (project / "resources" / "app.desktop").read_text(encoding="utf-8")
        metainfo = ElementTree.fromstring((project / "resources" / "app.metainfo.xml").read_text(encoding="utf-8").split("\n", 1)[1])

        assert "Categories=" not in entry
        assert not LinuxEntries.has_categories(entry)
        assert metainfo.find("categories") is None
        assert "set(HAYLEN_APP_DEBIAN_SECTION [[misc]])" in (project / "app.cmake").read_text(encoding="utf-8")

    def test_the_linux_installer_stops_an_app_whose_desktop_entry_lists_no_categories(self, fixture: DesktopFixture):
        app = fixture.app({}, "notes")
        product = fixture.resolve(app, "linux", Configuration.RELEASE)
        folders = DesktopFixture.folders(product)
        (folders.project / "resources").mkdir(parents=True)
        (folders.project / LinuxEntries.ENTRY).write_text("[Desktop Entry]\nType=Application\nName=Notes\n", encoding="utf-8")
        state = PreparationState("", DesktopBackend.NAME, "", "", "", "", "", MappingProxyType({}))
        built = BuildProduct(PreparedProject(product, folders, folders.project, DesktopBackend.NAME, state, False), folders.products, "app", (), ())
        request = PackageRequest(DesktopBackend.INSTALLER, fixture.folder / "packages", {})

        with pytest.raises(InvalidManifest) as raised:
            DesktopBackend(fixture.context).package(built, request)
        values = ", ".join(f'"{category.value}"' for category in AppCategory)
        assert raised.value.message == f'The Linux installer holds an AppImage, which needs the category of the app, and `{product.app.origin}` names none. Add "category" to it with one of {values}.'

        (folders.project / LinuxEntries.ENTRY).write_text("[Desktop Entry]\nType=Application\nCategories=Office;\n", encoding="utf-8")
        DesktopBackend.require_categories(built)

    def test_the_files_of_the_section_replace_the_files_of_the_template(self, fixture: DesktopFixture):
        app = fixture.app({"platforms": {"linux": {"files": {"resources/app.desktop": "desktop/garden.desktop", "resources/icons": "desktop/icons"}}}}, "garden")
        (app / "desktop" / "icons").mkdir(parents=True)
        (app / "desktop" / "garden.desktop").write_text("[Desktop Entry]\nType=Application\nCategories=Education;\n", encoding="utf-8")
        (app / "desktop" / "icons" / "512.png").write_bytes(b"own icon")

        project = self.prepare(fixture, app, "linux")
        entry = (project / "resources" / "app.desktop").read_text(encoding="utf-8").splitlines()
        assert "Categories=Education;" in entry
        assert "Exec=garden" in entry
        assert (project / "resources" / "icons" / "512.png").read_bytes() == b"own icon"
        assert (project / "resources" / "icons" / "16.png").is_file()

    def test_a_cpp_app_compiles_its_sources_and_leaves_objective_c_to_apple_platforms(self, fixture: DesktopFixture):
        app = fixture.cpp_app({"cpp": {"sources": ["src"], "includeDirectories": ["src"], "definitions": ["GARDEN=1"]}})
        values = (self.prepare(fixture, app, "linux") / "app.cmake").read_text(encoding="utf-8")
        assert f'haylen_add_app(main CPP SOURCES [[{(app / "src" / "App.cpp").as_posix()}]] CONTENT "${{HAYLEN_APP_CONTENT}}")' in values
        assert "target_compile_definitions(main PRIVATE [[GARDEN=1]])" in values

        manifest = json.loads((app / AppManifest.FILE).read_text(encoding="utf-8"))
        manifest["cpp"]["lua"] = True
        (app / AppManifest.FILE).write_text(json.dumps(manifest), encoding="utf-8")
        values = (self.prepare(fixture, app, "linux") / "app.cmake").read_text(encoding="utf-8")
        assert f'haylen_add_app(main CPP WITH_LUA SOURCES [[{(app / "src" / "App.cpp").as_posix()}]] CONTENT "${{HAYLEN_APP_CONTENT}}")' in values

        (app / "src" / "AppBridge.mm").write_text("", encoding="utf-8")
        with pytest.raises(InvalidManifest, match="which compile only on Apple platforms"):
            self.prepare(fixture, app, "linux")

    @staticmethod
    def executable(manifest: bytes) -> bytes:
        """Builds the smallest PE file whose resource section holds one application manifest."""
        resources = bytearray(0x58)
        resources[14:16] = (1).to_bytes(2, "little")
        resources[16:24] = (24).to_bytes(4, "little") + (0x80000000 | 0x18).to_bytes(4, "little")
        resources[0x18 + 14 : 0x18 + 16] = (1).to_bytes(2, "little")
        resources[0x18 + 16 : 0x18 + 24] = (1).to_bytes(4, "little") + (0x80000000 | 0x30).to_bytes(4, "little")
        resources[0x30 + 14 : 0x30 + 16] = (1).to_bytes(2, "little")
        resources[0x30 + 16 : 0x30 + 24] = (0x409).to_bytes(4, "little") + (0x48).to_bytes(4, "little")
        resources[0x48:0x50] = (0x1058).to_bytes(4, "little") + len(manifest).to_bytes(4, "little")
        resources += manifest
        header = bytearray(0x200)
        header[0:2] = b"MZ"
        header[0x3C:0x40] = (0x40).to_bytes(4, "little")
        header[0x40:0x44] = b"PE\0\0"
        header[0x46:0x48] = (1).to_bytes(2, "little")
        header[0x54:0x56] = (240).to_bytes(2, "little")
        header[0x58:0x5A] = (0x20B).to_bytes(2, "little")
        header[0x58 + 128 : 0x58 + 136] = (0x1000).to_bytes(4, "little") + len(resources).to_bytes(4, "little")
        section = 0x58 + 240
        header[section : section + 8] = b".rsrc\0\0\0"
        header[section + 8 : section + 24] = len(resources).to_bytes(4, "little") + (0x1000).to_bytes(4, "little") + len(resources).to_bytes(4, "little") + (0x200).to_bytes(4, "little")
        return bytes(header + resources)

    def test_the_manifest_of_a_windows_executable_selects_what_the_engine_needs(self, tmp_path: Path):
        manifest = (Repository.ENGINE / "platform" / "windows" / "haylen.manifest").read_bytes()
        assert WindowsResources.read(DesktopProjectTest.executable(manifest), WindowsResources.MANIFEST) == [manifest]
        assert WindowsResources.read(b"not an executable", WindowsResources.MANIFEST) == []

        app = DesktopApp("Sky Garden", "com.haylen.skygarden", "1.0", 1, AppCategory.GAME, "", "", "", "", "sky-garden")
        installed = tmp_path / "installed"
        native = tmp_path / "native"
        (installed / "app").mkdir(parents=True)
        (installed / "app" / "haylen.json").write_text("{}", encoding="utf-8")
        native.mkdir()
        (native / "glue.dll").write_bytes(b"glue")
        (installed / "glue.dll").write_bytes(b"glue")
        (installed / "sky-garden.exe").write_bytes(DesktopProjectTest.executable(manifest))
        layout = DesktopLayout.of("windows", app, installed)
        checks = DesktopVerifier.checks("windows", app, layout, Configuration.DEBUG, native)
        assert [check.outcome for check in checks] == [VerificationCheck.Outcome.PASSED]

        legacy = b"<assembly><application><windowsSettings><activeCodePage>Legacy</activeCodePage></windowsSettings></application></assembly>"
        (installed / "sky-garden.exe").write_bytes(DesktopProjectTest.executable(legacy))
        (installed / "glue.dll").unlink()
        details = [check.detail for check in DesktopVerifier.checks("windows", app, layout, Configuration.DEBUG, native)]
        assert details == [
            'The built app lacks the native library "glue.dll" in ".", where "native.load" finds it.',
            'The application manifest of the executable lacks the Common Controls of version 6, which give the task dialogs and the controls that native libraries show the current look of Windows. Keep the manifest of the engine that "haylen_add_app" adds.',
            'The application manifest of the executable lacks UTF-8 as the code page of the process, which the text of Haylen needs. Keep the manifest of the engine that "haylen_add_app" adds.',
        ]

    def test_a_linux_app_needs_its_desktop_entry_named_after_the_identifier(self, tmp_path: Path):
        app = DesktopApp("Sky Garden", "com.haylen.skygarden", "1.0", 1, AppCategory.GAME, "", "", "", "", "sky-garden")
        installed = tmp_path / "installed"
        (installed / "lib" / "sky-garden" / "app").mkdir(parents=True)
        (installed / "lib" / "sky-garden" / "app" / "haylen.json").write_text("{}", encoding="utf-8")
        (installed / "lib" / "sky-garden" / "sky-garden").write_bytes(b"code")
        (installed / "bin").mkdir()
        (installed / "bin" / "sky-garden").symlink_to("../lib/sky-garden/sky-garden")
        layout = DesktopLayout.of("linux", app, installed)
        details = [check.detail for check in DesktopVerifier.checks("linux", app, layout, Configuration.DEBUG, tmp_path / "native")]
        assert details == ['The built app lacks the desktop entry "share/applications/com.haylen.skygarden.desktop", by which Linux desktops find the name and the icon of the window of the app.']

        (installed / "share" / "applications").mkdir(parents=True)
        (installed / "share" / "applications" / "com.haylen.skygarden.desktop").write_text("[Desktop Entry]\n", encoding="utf-8")
        assert [check.outcome for check in DesktopVerifier.checks("linux", app, layout, Configuration.DEBUG, tmp_path / "native")] == [VerificationCheck.Outcome.PASSED]

    def test_the_project_of_a_windows_app_names_the_engine_runtime_and_the_sdk(self, fixture: DesktopFixture):
        product = fixture.resolve(fixture.app({}, "garden"), "windows")
        project = DesktopBackend(fixture.context).prepare(product, DesktopFixture.folders(product)).project
        values = (project / "app.cmake").read_text(encoding="utf-8")
        windows = fixture.sdks[0].folder / f"windows-{product.primary.target.architecture}" / "lib" / "cmake" / "haylen"
        assert f"set(haylen_DIR [[{windows.as_posix()}]])" in values
        assert (project / "resources" / "HaylenApp.h").is_file()
        assert "CREATEPROCESS_MANIFEST_RESOURCE_ID" not in (project / "resources" / "app.rc").read_text(encoding="utf-8")
