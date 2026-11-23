from haylen.core.package_layout import PackageLayout
from tests.support.tooling_fixture import ToolingFixture


class PackageLayoutTest:
    def test_only_the_package_of_an_app_that_runs_lua_holds_lua_modules(self, tmp_path):
        fixture = ToolingFixture(tmp_path)
        app = fixture.app({})
        plugin = fixture.plugin(app, "kit", {})
        (app / "content" / "data.json").write_text("{}", encoding="utf-8")

        scripted = PackageLayout.of(app, {"kit": plugin}, True).files()
        plain = PackageLayout.of(app, {"kit": plugin}, False).files()

        assert list(scripted) == ["content/data.json", "haylen.json", "plugins/kit/plugin.json", "plugins/kit/source/init.lua", "source/main.lua"]
        assert list(plain) == ["content/data.json", "haylen.json", "plugins/kit/plugin.json"]
