from pathlib import Path

import pytest

from haylen.content.content_profile import ContentProfile
from haylen.content.protected_releases import ProtectedReleases
from haylen.core.errors.missing_release import MissingRelease
from haylen.platforms.product_folders import ProductFolders
from tests.content.content_fixture import ContentFixture
from tests.content.recording_content_tool import RecordingContentTool


class ProtectedReleasesTest:
    def test_a_release_follows_the_release_that_shipped_before_the_one_of_the_build_folder(self, tmp_path, monkeypatch):
        fixture = ContentFixture(tmp_path, monkeypatch)
        source = fixture.source()

        first = fixture.releases.build(source, "fake")
        fixture.releases.build(source, "fake", tmp_path / "shipped")
        fixture.releases.build(source, "fake")

        builds = fixture.commands("build")
        assert "--previous" not in builds[0]
        assert builds[1][builds[1].index("--previous") + 1] == str((tmp_path / "shipped").resolve())
        assert builds[2][builds[2].index("--previous") + 1] == str(first.folder)
        assert first.folder == ProductFolders.app_folder(source.app.folder) / "release" / "fake"
        assert first.shipped == first.folder
        assert not (first.folder.parent / "fake.new").exists()

    def test_a_release_builds_from_the_staged_package_with_the_build_and_the_shard_target_of_its_profile(self, tmp_path, monkeypatch):
        fixture = ContentFixture(tmp_path, monkeypatch)
        monkeypatch.setattr(ContentProfile, "SHARD_TARGETS", {"fake": 1024})
        fixture.tooling.plugin(tmp_path / "app", "kit", {})
        source = fixture.source({"build": 7, "plugins": {"kit": {}}})

        fixture.releases.build(source, "fake")

        command = fixture.commands("build")[0]
        package = Path(command[1])
        assert (package / "plugins" / "kit" / "plugin.json").is_file()
        assert (package / "source" / "main.lua").is_file()
        assert command[command.index("--build") + 1] == "7"
        assert command[command.index("--profile") + 1] == "fake"
        assert command[command.index("--shard-target") + 1] == "1024"

    def test_a_server_keeps_the_later_content_out_of_the_app_and_named_packs_go_to_their_stores(self, tmp_path, monkeypatch):
        fixture = ContentFixture(tmp_path, monkeypatch)
        fixture.tool.packs = ("forest",)
        served = fixture.source({"content": {"updates": {"baseUrl": "https://example.com/content"}}})

        release = fixture.releases.build(served, "fake")
        stage = fixture.commands("stage")[0]
        assert release.shipped == release.folder.parent / "fake.shipped"
        assert "--ship-deferred" not in stage
        assert dict(release.packs) == {"forest": release.folder.parent / "fake.packs" / "forest"}

        packed = fixture.source({"content": {"delivery": [{"match": "forest/*", "pack": "forest"}]}})
        fixture.releases.build(packed, "fake")
        assert "--ship-deferred" in fixture.commands("stage")[1]

        fixture.releases.build(fixture.source(), "fake")
        assert not release.shipped.exists()
        assert not (release.folder.parent / "fake.packs").exists()

    def test_a_release_larger_than_its_store_accepts_warns(self, tmp_path, monkeypatch):
        fixture = ContentFixture(tmp_path, monkeypatch)
        monkeypatch.setattr(ContentProfile, "STORE_LIMITS", {"fake": (4, "the limit of the test")})
        source = fixture.source()

        fixture.releases.build(source, "fake")
        fixture.releases.build(source, "other")

        assert fixture.output.count("more than the limit of the test") == 1

    def test_the_bootstrap_source_comes_from_the_keys_of_the_app_and_its_build(self, tmp_path, monkeypatch):
        fixture = ContentFixture(tmp_path, monkeypatch)
        source = fixture.source({"build": 3})

        text = fixture.releases.bootstrap_source(source.app, "fake")

        command = fixture.commands("bootstrap")[0]
        assert text == RecordingContentTool.BOOTSTRAP
        assert command[command.index("--build") + 1] == "3"
        assert command[command.index("--keys") + 1] == str(fixture.releases.keys(source.app))

    def test_the_app_files_are_what_the_tool_stages_for_the_app_domain(self, tmp_path, monkeypatch):
        fixture = ContentFixture(tmp_path, monkeypatch)
        source = fixture.source()
        release = fixture.releases.build(source, "fake")

        assert fixture.releases.app_files(source, release) == ("app.hmanifest",)
        assert "--app-only" in fixture.commands("stage")[0]
        assert not (release.folder.parent / "app-files").exists()

    def test_the_first_publication_of_a_channel_keeps_the_shards_of_the_release_that_ships(self, tmp_path, monkeypatch):
        fixture = ContentFixture(tmp_path, monkeypatch)
        source = fixture.source()

        fixture.releases.publish(source, "fake", "beta", tmp_path / "tree")
        release = fixture.releases.build(source, "fake")
        fixture.releases.publish(source, "fake", "beta", tmp_path / "tree")

        first, second = fixture.commands("publish")
        assert "--release" not in first
        assert second[second.index("--release") + 1] == str(release.folder)
        assert second[second.index("--channel") + 1] == "beta"

    def test_the_commands_examine_the_last_release_or_name_the_command_that_builds_one(self, tmp_path, monkeypatch):
        fixture = ContentFixture(tmp_path, monkeypatch)
        source = fixture.source()

        with pytest.raises(MissingRelease, match='Build one with "haylen content build"'):
            ProtectedReleases.last(source.app, "fake")
        built = fixture.releases.build(source, "fake")

        assert ProtectedReleases.last(source.app, "fake") == built
        assert ProtectedReleases.last(source.app, "fake", built.folder).folder == built.folder
