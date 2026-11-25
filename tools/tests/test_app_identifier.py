import json
from pathlib import Path

import pytest

from haylen.core.app_identifier import AppIdentifier
from haylen.core.app_manifest import AppManifest
from haylen.core.errors.invalid_manifest import InvalidManifest
from haylen.core.package_layout import PackageLayout
from haylen.core.repository import Repository
from haylen.platforms.android.android_project import AndroidProject
from tests.support.tooling_fixture import ToolingFixture


class AppIdentifierTest:
    """The identifier that the tooling gives an app whose `haylen.json` names none, which the package carries, so the engine and every local target take it."""

    @pytest.fixture
    def fixture(self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> ToolingFixture:
        monkeypatch.setattr(Repository, "APPS", tmp_path / "apps")
        return ToolingFixture(tmp_path)

    def test_an_app_without_an_identifier_takes_one_of_its_folder_that_no_other_folder_shares(self, fixture: ToolingFixture, tmp_path: Path):
        first = AppManifest.load(fixture.app({"identifier": None}, "Sky Garden"))
        again = AppManifest.load(fixture.app({"identifier": None}, "Sky Garden"))
        elsewhere = AppManifest.load(ToolingFixture(tmp_path / "other").app({"identifier": None}, "Sky Garden"))

        assert first.identifier_generated
        assert first.identifier == again.identifier
        assert first.identifier != elsewhere.identifier
        assert first.identifier.startswith("local.skygarden")
        assert len(first.identifier) == len("local.skygarden") + 8

    @pytest.mark.parametrize("name", ["Sky Garden", "2048 Puzzle!", "123", "new", "日本"])
    def test_every_generated_identifier_is_a_bundle_identifier_and_an_application_id(self, tmp_path: Path, name: str):
        identifier = AppIdentifier.generate(tmp_path / name)
        assert AndroidProject.APPLICATION_ID.fullmatch(identifier) is not None
        assert all(part[0].isalpha() for part in identifier.split("."))
        assert not identifier.startswith("dev.haylen")
        assert AppIdentifier.generate(tmp_path / "123").startswith(f"local.{AppIdentifier.FALLBACK}")

    def test_the_package_carries_the_generated_identifier_with_every_line_in_its_place(self, fixture: ToolingFixture):
        app = AppManifest.load(fixture.app({"identifier": None}, "garden"))
        files = PackageLayout.of_app(app, {}).files()
        carried = files[PackageLayout.MANIFEST]
        original = app.path.read_text(encoding="utf-8")
        text = carried.read_text(encoding="utf-8")

        assert carried != app.path
        assert not carried.is_relative_to(app.folder), "The tooling never writes into the app folder."
        assert json.loads(text) == {"identifier": app.identifier, **json.loads(original)}
        assert text.count("\n") == original.count("\n")

        stamp = carried.stat().st_mtime_ns
        assert app.package_manifest().stat().st_mtime_ns == stamp, "The copy is written only when it changes."
        app.path.write_text(original.replace('"Test App"', '"Garden"'), encoding="utf-8")
        assert json.loads(AppManifest.load(app.folder).package_manifest().read_text(encoding="utf-8"))["name"] == "Garden"

    def test_an_app_with_an_identifier_ships_its_own_manifest(self, fixture: ToolingFixture):
        app = AppManifest.load(fixture.app({"identifier": "com.example.garden"}))
        assert not app.identifier_generated
        assert PackageLayout.of_app(app, {}).files()[PackageLayout.MANIFEST] == app.path
        assert app.require_identifier("Anything") == "com.example.garden"

    def test_a_step_that_needs_an_identifier_of_the_developer_says_why_and_what_to_add(self, fixture: ToolingFixture):
        app = AppManifest.load(fixture.app({"identifier": None}))
        with pytest.raises(InvalidManifest) as raised:
            app.require_identifier("A Release build ships the protected release of the app, whose content keys are kept per identifier")
        assert raised.value.message == (
            f'A Release build ships the protected release of the app, whose content keys are kept per identifier, and `{app.origin}` names no "identifier". '
            'Add "identifier" to it with a reverse domain identifier of yours, such as "com.example.game".'
        )
