import re

import pytest

from haylen.content.content_profile import ContentProfile
from haylen.core.errors.invalid_contribution import InvalidContribution
from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.core.frozen import Frozen
from haylen.platforms.android.android_asset_packs import AndroidAssetPacks
from haylen.plugins.merged_contribution import MergedContribution


class AndroidAssetPacksTest:
    @staticmethod
    def delivery(fast: list[str]) -> MergedContribution:
        return MergedContribution("assetPacks", Frozen.freeze({"fastFollow": fast}), ("delivery",), ("plugins/delivery/plugin.json at contributions[0]",), ())

    @staticmethod
    def release(folder, sizes: dict[str, int]):
        folder.mkdir(parents=True)
        for name, size in sizes.items():
            (folder / name).write_bytes(b"s" * size)
        return folder

    def test_a_release_too_large_for_the_base_module_moves_its_content_shards_into_install_time_packs(self, tmp_path, monkeypatch):
        shards = {f"{index:064x}.hpak": 100 for index in range(6)}
        shipped = AndroidAssetPacksTest.release(tmp_path / "release", {"app.hmanifest": 10, "content.hmanifest": 10, **shards})
        kept = ("app.hmanifest", "content.hmanifest", f"{0:064x}.hpak")
        monkeypatch.setattr(AndroidAssetPacks, "BASE_RELEASE_LIMIT", 100)
        monkeypatch.setattr(AndroidAssetPacks, "PACK_LIMIT", 250)

        packs = AndroidAssetPacks.plan(shipped, kept, {}, None, ())

        assert packs.install_time == ("haylencontent1", "haylencontent2", "haylencontent3")
        base = sorted(path.rsplit("/", 1)[1] for path in packs.files if path.startswith(AndroidAssetPacks.RELEASE_ASSETS))
        assert base == sorted(kept), "The manifests and the shards of the app domain stay in the base module."
        counts = [len([path for path in packs.files if path.startswith(f"asset-packs/{name}/src/main/assets/app/")]) for name in packs.install_time]
        assert counts == [2, 2, 1]
        assert b'deliveryType.set("install-time")' in packs.files["asset-packs/haylencontent1/build.gradle.kts"]

    def test_a_release_within_the_base_module_ships_whole_in_it(self, tmp_path):
        shipped = AndroidAssetPacksTest.release(tmp_path / "release", {"app.hmanifest": 10, f"{0:064x}.hpak": 100})

        packs = AndroidAssetPacks.plan(shipped, (), {}, None, ())

        assert packs.deliveries == {}
        assert sorted(packs.files) == [f"{AndroidAssetPacks.RELEASE_ASSETS}/{0:064x}.hpak", f"{AndroidAssetPacks.RELEASE_ASSETS}/app.hmanifest"]

    def test_a_plugin_that_delivers_packs_turns_the_named_packs_into_asset_packs_of_google_play(self, tmp_path):
        shipped = AndroidAssetPacksTest.release(tmp_path / "release", {"app.hmanifest": 10})
        packs = {}
        for pack, count in (("chapter-2", 2), ("intro.music", 1), ("2nd", 1)):
            packs[pack] = AndroidAssetPacksTest.release(tmp_path / "packs" / pack, {f"{pack}-{index}.hpak": 10 for index in range(count)})
        content = ("2nd", "chapter-2", "intro.music")

        planned = AndroidAssetPacks.plan(shipped, (), packs, AndroidAssetPacksTest.delivery(["intro.music"]), content)

        assert planned.delivered == ("pack_2nd", "chapter_2", "intro_music")
        assert b'deliveryType.set("fast-follow")' in planned.files["asset-packs/intro_music/build.gradle.kts"]
        assert b'deliveryType.set("on-demand")' in planned.files["asset-packs/chapter_2/build.gradle.kts"]
        assert sorted(path for path in planned.files if path.startswith("asset-packs/chapter_2/src/")) == [
            "asset-packs/chapter_2/src/main/assets/chapter-2-0.hpak",
            "asset-packs/chapter_2/src/main/assets/chapter-2-1.hpak",
        ], "The shards sit at the root of the assets of each pack."

    def test_the_named_packs_stay_for_the_content_server_without_a_plugin_that_delivers_them(self, tmp_path):
        shipped = AndroidAssetPacksTest.release(tmp_path / "release", {"app.hmanifest": 10})
        packs = {"chapter-2": AndroidAssetPacksTest.release(tmp_path / "packs" / "chapter-2", {"a.hpak": 10})}

        assert AndroidAssetPacks.plan(shipped, (), packs, None, ("chapter-2",)).deliveries == {}

    def test_the_packs_of_a_plugin_are_checked_against_the_app_and_the_store(self, tmp_path, monkeypatch):
        shipped = AndroidAssetPacksTest.release(tmp_path / "release", {"app.hmanifest": 10})
        packs = {"chapter-2": AndroidAssetPacksTest.release(tmp_path / "packs" / "chapter-2", {"a.hpak": 10, "b.hpak": 10})}

        with pytest.raises(InvalidContribution, match=re.escape('the packs "chapter-3", which "content.delivery"')):
            AndroidAssetPacks.plan(shipped, (), packs, AndroidAssetPacksTest.delivery(["chapter-3"]), ("chapter-2",))
        monkeypatch.setattr(AndroidAssetPacks, "STORE_PACK_LIMIT", 15)
        with pytest.raises(UnsupportedTarget, match='The pack "chapter-2" holds 20 bytes'):
            AndroidAssetPacks.plan(shipped, (), packs, AndroidAssetPacksTest.delivery([]), ("chapter-2",))
        monkeypatch.undo()

        packs["chapter.2"] = AndroidAssetPacksTest.release(tmp_path / "packs" / "chapter.2", {"c.hpak": 10})
        with pytest.raises(InvalidContribution, match=re.escape('The packs "chapter-2" and "chapter.2" both become the asset pack "chapter_2"')):
            AndroidAssetPacks.plan(shipped, (), packs, AndroidAssetPacksTest.delivery([]), ("chapter-2", "chapter.2"))

    def test_a_named_pack_never_takes_the_name_of_a_content_pack(self, tmp_path, monkeypatch):
        shipped = AndroidAssetPacksTest.release(tmp_path / "release", {"app.hmanifest": 10, f"{0:064x}.hpak": 100})
        packs = {"haylencontent1": AndroidAssetPacksTest.release(tmp_path / "packs" / "haylencontent1", {"a.hpak": 10})}
        monkeypatch.setattr(AndroidAssetPacks, "BASE_RELEASE_LIMIT", 50)

        with pytest.raises(InvalidContribution, match="which the release already takes for its content shards"):
            AndroidAssetPacks.plan(shipped, ("app.hmanifest",), packs, AndroidAssetPacksTest.delivery([]), ("haylencontent1",))

    def test_android_releases_build_shards_that_fit_an_asset_pack(self):
        target = ContentProfile.of("android").shard_target
        assert target is not None
        assert target * 2 + 5 * 1024**2 <= AndroidAssetPacks.PACK_LIMIT, "A shard holds at most twice its target and one record, so it fits a pack."
