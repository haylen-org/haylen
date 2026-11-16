from tests.content.content_fixture import ContentFixture


class ReleasePackageTest:
    def test_resolves_the_plugins_of_the_platform_of_a_target_without_an_sdk(self, tmp_path, monkeypatch):
        fixture = ContentFixture(tmp_path, monkeypatch)
        fixture.tooling.plugin(tmp_path / "app", "kit", {})

        source = fixture.source({"plugins": {"kit": {}}})

        assert fixture.tooling.sdks == []
        assert "plugins/kit/source/init.lua" in source.package.files()

    def test_reads_the_delivery_of_the_content_from_the_manifest(self, tmp_path, monkeypatch):
        fixture = ContentFixture(tmp_path, monkeypatch)
        (tmp_path / "app" / "content" / "images").mkdir(parents=True)
        (tmp_path / "app" / "content" / "images" / "logo.png").write_bytes(b"logo")
        delivery = [{"match": "forest/*", "pack": "forest"}, {"match": "caves/*", "pack": "caves"}, {"match": "music/*", "mode": "onDemand"}]

        source = fixture.source({"splash": {"logo": "images/logo.png"}, "content": {"updates": {"baseUrl": "https://example.com/content"}, "delivery": delivery}})
        plain = fixture.source()

        assert source.packs == ("caves", "forest")
        assert source.content_server == "https://example.com/content"
        assert source.splash_logo == "content/images/logo.png"
        assert (plain.packs, plain.content_server, plain.splash_logo) == ((), None, None)
