from types import MappingProxyType

import pytest

from haylen.cli.option_values import OptionValues
from haylen.content.protected_release import ProtectedRelease
from haylen.content.steam_depots import SteamDepots
from haylen.core.errors.invalid_option import InvalidOption
from tests.content.content_fixture import ContentFixture


class SteamDepotsTest:
    def test_writes_a_build_script_per_depot(self, tmp_path):
        options = SteamDepots.Options.parse({"appId": "1000", "depots": ["macos=1001", "forest=1002"], "branch": "beta"})

        SteamDepots.write_scripts(tmp_path, options.app_id, "Island 1.2.0", options.depots, options.branch)

        build = (tmp_path / "scripts" / "app-build.vdf").read_text(encoding="utf-8")
        forest = (tmp_path / "scripts" / "depot-build-1002.vdf").read_text(encoding="utf-8")
        assert '"AppID" "1000"' in build
        assert '"SetLive" "beta"' in build
        assert '"1002" "depot-build-1002.vdf"' in build
        assert '"ContentRoot" "../content/forest/"' in forest

    def test_escapes_every_value_that_comes_from_the_app_or_the_command(self, tmp_path):
        SteamDepots.write_scripts(tmp_path, "1000", 'Island "Deluxe" C:\\saves\nsecond line\tend', {"forest": "1002"}, 'beta"\n"AppID" "1')

        build = (tmp_path / "scripts" / "app-build.vdf").read_text(encoding="utf-8")
        assert '\t"Desc" "Island \\"Deluxe\\" C:\\\\saves\\nsecond line\\tend"\n' in build
        assert '\t"SetLive" "beta\\"\\n\\"AppID\\" \\"1"\n' in build
        assert build.count('"AppID"') == 1

    def test_one_depot_of_the_command_line_is_one_entry(self):
        options = SteamDepots.Options.parse(OptionValues.parse(["appId=480", "depots=windows=481"]))
        assert dict(options.depots) == {"windows": "481"}

        options = SteamDepots.Options.parse(OptionValues.parse(["appId=480", "depots=windows=481", "depots=forest=482"]))
        assert dict(options.depots) == {"windows": "481", "forest": "482"}

    def test_refuses_options_without_numeric_ids(self):
        with pytest.raises(InvalidOption, match='"forest=1001"'):
            SteamDepots.Options.parse({"appId": "1000", "depots": ["macos=1001", "forest=x"]})
        with pytest.raises(InvalidOption, match='"appId"'):
            SteamDepots.Options.parse({"depots": {"macos": "1001"}})
        with pytest.raises(InvalidOption, match='"output"'):
            SteamDepots.Options.parse({"appId": "1000", "output": "steam"})

    def test_stages_the_depot_of_the_platform_and_one_depot_per_pack(self, tmp_path, monkeypatch):
        fixture = ContentFixture(tmp_path, monkeypatch)
        source = fixture.source({"name": "Island", "version": "1.2.0"})
        installed = tmp_path / "installed"
        (installed / "lib" / "island" / "app").mkdir(parents=True)
        (installed / "bin").mkdir()
        (installed / "bin" / "island").write_bytes(b"executable")
        (tmp_path / "packs" / "forest").mkdir(parents=True)
        (tmp_path / "packs" / "forest" / "forest.hpak").write_bytes(b"shard")
        release = ProtectedRelease("linux", tmp_path / "release", tmp_path / "release", MappingProxyType({"forest": tmp_path / "packs" / "forest"}))
        options = SteamDepots.Options.parse({"appId": "1000", "depots": {"linux": "1001", "forest": "1002"}})

        script = SteamDepots(fixture.context).stage(source, release, "linux", installed, bundle=False, packs_place="lib/island", options=options, output=tmp_path / "steam")

        assert script == tmp_path / "steam" / "scripts" / "app-build.vdf"
        assert (tmp_path / "steam" / "content" / "linux" / "bin" / "island").is_file()
        assert (tmp_path / "steam" / "content" / "forest" / "lib" / "island" / "packs" / "forest.hpak").is_file()
        assert '"Desc" "Island 1.2.0"' in script.read_text(encoding="utf-8")

    def test_names_the_depots_without_an_id(self, tmp_path, monkeypatch):
        fixture = ContentFixture(tmp_path, monkeypatch)
        release = ProtectedRelease("macos", tmp_path, tmp_path, MappingProxyType({"forest": tmp_path}))
        options = SteamDepots.Options.parse({"appId": "1000", "depots": {"macos": "1001"}})

        with pytest.raises(InvalidOption, match='"forest"'):
            SteamDepots(fixture.context).stage(fixture.source(), release, "macos", tmp_path / "Island.app", bundle=True, packs_place="", options=options, output=tmp_path / "steam")
