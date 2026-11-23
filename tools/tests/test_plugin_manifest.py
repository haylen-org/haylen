import pytest

from haylen.core.errors.invalid_manifest import InvalidManifest
from haylen.plugins.implementation import Implementation
from haylen.plugins.plugin_manifest import PluginManifest
from tests.support.tooling_fixture import ToolingFixture


class PluginManifestTest:
    def test_reads_implementations_parameters_and_contributions(self, tmp_path):
        fixture = ToolingFixture(tmp_path)
        folder = fixture.plugin(
            tmp_path,
            "camera",
            {
                "implementations": {"fake": "native", "web": "lua"},
                "parameters": {"usage": {"type": "string", "default": "Scans.", "description": "The usage."}},
                "contributions": [{"type": "package", "platforms": ["fake"], "url": "https://example.com/kit", "version": "${usage}"}],
            },
        )

        manifest = PluginManifest.load(folder)

        assert manifest.implementation("fake") is Implementation.NATIVE
        assert manifest.implementation("web") is Implementation.LUA
        assert manifest.implementation("android") is None
        assert manifest.contributions[0].fields["url"] == "https://example.com/kit"
        assert manifest.contributions[0].origin.endswith("plugin.json")

    def test_names_every_problem_the_schema_cannot_see(self, tmp_path):
        folder = ToolingFixture(tmp_path).plugin(
            tmp_path,
            "camera",
            {
                "implementations": {"fake": "native", "web": "lua"},
                "dependencies": {"camera": "1.0"},
                "parameters": {"size": {"type": "integer", "default": "big", "description": "A size.", "platforms": ["android"]}},
                "contributions": [{"type": "package", "platforms": ["web"], "url": "${missing}"}],
            },
        )

        with pytest.raises(InvalidManifest) as raised:
            PluginManifest.load(folder)

        message = raised.value.message
        assert 'The parameter "size" applies to "android", which "implementations" does not list.' in message
        assert 'The default of the parameter "size" must be a whole number.' in message
        assert 'names "web", where "implementations" does not give the plugin a native part.' in message
        assert 'names the parameter "missing" in "${missing}", which "parameters" does not declare.' in message
        assert 'The platform "fake" has a native part in "implementations", but no contribution brings it.' in message
        assert "The plugin depends on itself" in message

    def test_follows_the_schema(self, tmp_path):
        folder = ToolingFixture(tmp_path).plugin(tmp_path, "camera", {"interface": 2, "platforms": ["ios"]})
        with pytest.raises(InvalidManifest) as raised:
            PluginManifest.load(folder)
        assert 'The key "interface" must be 1, not 2.' in raised.value.message
        assert 'The file has the unknown keys "platforms".' in raised.value.message
