import base64
import zipfile

from haylen.content.release_entry import ReleaseEntry
from haylen.content.release_inspection import ReleaseInspection
from haylen.content.release_package import ReleasePackage
from haylen.core.app_manifest import AppManifest
from haylen.core.package_layout import PackageLayout
from haylen.platforms.verification_check import VerificationCheck
from tests.support.tooling_fixture import ToolingFixture


class ReleaseInspectionTest:
    KEY = bytes(range(32))

    @staticmethod
    def inspection(tmp_path) -> ReleaseInspection:
        app = ToolingFixture(tmp_path).app({"splash": {"logo": "images/logo.png"}})
        (app / "source" / "main.lua").write_text("print('the main module of the app')", encoding="utf-8")
        (app / "content" / "images").mkdir()
        (app / "content" / "images" / "hero.png").write_bytes(b"a hero image that only the package holds")
        (app / "content" / "images" / "logo.png").write_bytes(b"the splash logo that platforms show")
        source = ReleasePackage(AppManifest.load(app), PackageLayout.of(app, {}))
        return ReleaseInspection(source, {'the content key "1234"': ReleaseInspectionTest.KEY})

    @staticmethod
    def entry(path: str, data: bytes) -> ReleaseEntry:
        return ReleaseEntry(path, len(data), lambda: data)

    def test_a_protected_release_passes(self, tmp_path):
        inspection = ReleaseInspectionTest.inspection(tmp_path)
        logo = (tmp_path / "app" / "content" / "images" / "logo.png").read_bytes()
        entries = [
            ReleaseInspectionTest.entry("app/app.hmanifest", b"manifest"),
            ReleaseInspectionTest.entry("app/content.hmanifest", b"manifest"),
            ReleaseInspectionTest.entry("app/" + "ab" * 32 + ".hpak", b"\x00" * 64),
            ReleaseInspectionTest.entry("garden", b"code of the app"),
            ReleaseInspectionTest.entry("res/splash.png", logo),
        ]

        checks = inspection.checks(entries, "app")

        assert [check.outcome for check in checks] == [VerificationCheck.Outcome.PASSED]
        assert checks[0].source == "content"

    def test_raw_files_lua_text_indexes_and_symbols_fail(self, tmp_path):
        inspection = ReleaseInspectionTest.inspection(tmp_path)
        hero = (tmp_path / "app" / "content" / "images" / "hero.png").read_bytes()
        entries = [
            ReleaseInspectionTest.entry("app/app.hmanifest", b"manifest"),
            ReleaseInspectionTest.entry("app/source/main.lua", b"print('x')"),
            ReleaseInspectionTest.entry("assets/renamed.bin", hero),
            ReleaseInspectionTest.entry("app/haylen-package-index.json", b"[]"),
            ReleaseInspectionTest.entry("garden.pdb", b"symbols of the app"),
        ]

        checks = inspection.checks(entries, "app")
        problems = [check.detail for check in checks]

        assert all(check.outcome is VerificationCheck.Outcome.FAILED for check in checks)
        assert 'The release holds the Lua text "app/source/main.lua".' in problems
        assert 'The release holds "app/source/main.lua", while its folder "app" holds only manifests and shards.' in problems
        assert 'The release holds "assets/renamed.bin", which is the file "content/images/hero.png" of the package as it is.' in problems
        assert 'The release holds the index of a development package, "app/haylen-package-index.json".' in problems
        assert 'The release holds the symbols "garden.pdb", which stay with the developer.' in problems

    def test_keys_in_any_form_and_a_missing_release_fail(self, tmp_path):
        inspection = ReleaseInspectionTest.inspection(tmp_path)
        key = ReleaseInspectionTest.KEY
        for form in (key, key.hex().encode(), key.hex().upper().encode(), base64.b64encode(key)):
            problems = inspection.problems([ReleaseInspectionTest.entry("garden", b"code " + form + b" code")], "app")

            assert 'The release holds the content key "1234" of the app in "garden".' in problems
            assert 'The release has no protected release in "app".' in problems

    def test_the_entries_of_a_folder_and_an_archive_name_their_paths_inside_the_product(self, tmp_path):
        (tmp_path / "bundle" / "app").mkdir(parents=True)
        (tmp_path / "bundle" / "app" / "app.hmanifest").write_bytes(b"manifest")
        with zipfile.ZipFile(tmp_path / "app.apk", "w") as archive:
            archive.writestr("assets/app/app.hmanifest", b"manifest")
            archive.writestr("lib/arm64-v8a/", b"")

        folder = ReleaseEntry.of_folder(tmp_path / "bundle")
        packed = ReleaseEntry.of_archive(tmp_path / "app.apk")

        assert [(entry.path, entry.size, entry.read()) for entry in folder] == [("app/app.hmanifest", 8, b"manifest")]
        assert [(entry.path, entry.size, entry.read()) for entry in packed] == [("assets/app/app.hmanifest", 8, b"manifest")]
