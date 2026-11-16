import shutil

from haylen.core.repository import Repository
from haylen.plugins.plugin_manifest import PluginManifest


class RepositoryPluginsTest:
    """Every `plugin.json` of the repository follows the plugin schema, and the one of the plugin template does once `plugin new` replaced its tokens."""

    FOLDERS = (Repository.ROOT / "samples", Repository.ENGINE / "tests" / "data")
    TEMPLATE = Repository.TEMPLATES / "plugin"
    TOKENS = {"{{ID}}": "echo-kit", "{{TITLE}}": "Echo Kit", "{{NAME}}": "EchoKit", "{{PACKAGE}}": "echokit", "{{SYMBOL}}": "echo_kit", "{{MACRO}}": "ECHO_KIT"}

    def test_every_plugin_of_the_repository_loads(self):
        manifests = sorted(path for folder in RepositoryPluginsTest.FOLDERS for path in folder.rglob(PluginManifest.FILE))

        assert manifests
        for path in manifests:
            manifest = PluginManifest.load(path.parent)
            assert manifest.id == path.parent.name

    def test_the_plugin_template_loads_once_its_tokens_are_replaced(self, tmp_path):
        folder = tmp_path / RepositoryPluginsTest.TOKENS["{{ID}}"]
        shutil.copytree(RepositoryPluginsTest.TEMPLATE, folder)
        text = (folder / PluginManifest.FILE).read_text(encoding="utf-8")
        for token, value in RepositoryPluginsTest.TOKENS.items():
            text = text.replace(token, value)
        (folder / PluginManifest.FILE).write_text(text, encoding="utf-8")

        manifest = PluginManifest.load(folder)

        assert manifest.id == RepositoryPluginsTest.TOKENS["{{ID}}"]
        assert {contribution.type for contribution in manifest.contributions} >= {"appleClass", "gradleModule", "webModule", "nativeLibrary"}
