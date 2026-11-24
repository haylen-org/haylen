import json
import re
import subprocess
from pathlib import Path

import pytest

from haylen.core.app_manifest import AppManifest
from haylen.core.configuration import Configuration
from haylen.core.errors.invalid_manifest import InvalidManifest
from haylen.core.repository import Repository
from haylen.plugins.app_plugins import AppPlugins
from haylen.plugins.plugin_lock import PluginLock
from haylen.plugins.plugin_manifest import PluginManifest
from haylen.plugins.plugin_source import PluginSource
from tests.support.tooling_fixture import ToolingFixture


class PluginRepositoriesTest:
    """The plugins that an app takes from a git repository with `git`, `ref` and `path`, pinned in `haylen.lock`, as `haylen plugin add --reference`, `plugin update`, `plugin list` and the builds take them, from local repositories that the tests commit to."""

    @pytest.fixture(autouse=True)
    def isolated_git(self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch):
        """Every test keeps its checkouts in its own folder and runs git without the configuration of the machine."""
        monkeypatch.setattr(Repository, "PLUGIN_CHECKOUTS", tmp_path / "checkouts")
        configuration = tmp_path / "gitconfig"
        configuration.write_text("", encoding="utf-8")
        monkeypatch.setenv("GIT_CONFIG_GLOBAL", str(configuration))
        monkeypatch.setenv("GIT_CONFIG_NOSYSTEM", "1")
        for variable in ("GIT_AUTHOR_NAME", "GIT_COMMITTER_NAME"):
            monkeypatch.setenv(variable, "Haylen Tests")
        for variable in ("GIT_AUTHOR_EMAIL", "GIT_COMMITTER_EMAIL"):
            monkeypatch.setenv(variable, "tests@haylen.invalid")

    @staticmethod
    def git(repository: Path, *arguments: str) -> str:
        completed = subprocess.run(["git", "-C", str(repository), *arguments], check=True, capture_output=True, text=True)
        return completed.stdout.strip()

    @staticmethod
    def repository(fixture: ToolingFixture) -> Path:
        """A local repository whose branch `main` holds the plugin `kit` in `plugins/kit` at version 1.0.0. Its name ends with `.git`, which is how the tooling tells a repository from a plugin folder."""
        repository = fixture.folder / "repositories" / "kit.git"
        repository.mkdir(parents=True)
        PluginRepositoriesTest.git(repository, "init", "--quiet", "--initial-branch", "main")
        fixture.plugin(repository, "kit", {})
        PluginRepositoriesTest.commit(repository, "Add the plugin.")
        return repository

    @staticmethod
    def commit(repository: Path, message: str) -> str:
        PluginRepositoriesTest.git(repository, "add", "--all")
        PluginRepositoriesTest.git(repository, "commit", "--quiet", "--message", message)
        return PluginRepositoriesTest.git(repository, "rev-parse", "HEAD")

    @staticmethod
    def release(repository: Path, version: str) -> str:
        """Commits a new version of the plugin on the branch `main` and returns the commit."""
        manifest = repository / "plugins" / "kit" / PluginManifest.FILE
        document = json.loads(manifest.read_text(encoding="utf-8"))
        document["version"] = version
        manifest.write_text(json.dumps(document, indent=4), encoding="utf-8")
        return PluginRepositoriesTest.commit(repository, f"Release {version}.")

    @staticmethod
    def source(app: Path) -> PluginSource:
        fixture = ToolingFixture(app.parent)
        return PluginSource(app, PluginLock.load(app), fixture.context.process, fixture.context.terminal)

    @staticmethod
    def version(app: Path, configuration: Configuration) -> str:
        """Returns the version of the plugin `kit` that a build of the app in that configuration takes."""
        reference = next(entry for entry in AppManifest.load(app).plugins if entry.id == "kit")
        folder = PluginRepositoriesTest.source(app).folder(reference, configuration)
        return PluginManifest.load(folder).version.text

    @staticmethod
    def lock(app: Path) -> dict:
        return json.loads((app / PluginLock.FILE).read_text(encoding="utf-8"))

    def test_a_referenced_repository_is_pinned_and_builds_take_the_pinned_commit_while_its_branch_moves_on(self, tmp_path):
        fixture = ToolingFixture(tmp_path)
        app = fixture.app({})
        repository = PluginRepositoriesTest.repository(fixture)
        pinned = PluginRepositoriesTest.git(repository, "rev-parse", "HEAD")
        AppPlugins(fixture.context, app).add(str(repository), "main", "plugins/kit", reference=True)

        assert not (app / "plugins").exists()
        assert json.loads((app / "haylen.json").read_text(encoding="utf-8"))["plugins"] == {"kit": {"git": "../repositories/kit.git", "ref": "main", "path": "plugins/kit"}}
        assert PluginRepositoriesTest.lock(app) == {"version": PluginLock.VERSION, "plugins": {"kit": {"git": "../repositories/kit.git", "ref": "main", "commit": pinned}}}

        PluginRepositoriesTest.release(repository, "1.1.0")
        assert PluginRepositoriesTest.version(app, Configuration.DEBUG) == "1.0.0"
        assert PluginRepositoriesTest.version(app, Configuration.RELEASE) == "1.0.0"

    def test_updating_a_plugin_pins_the_commit_that_its_branch_names_now(self, tmp_path):
        fixture = ToolingFixture(tmp_path)
        app = fixture.app({})
        repository = PluginRepositoriesTest.repository(fixture)
        AppPlugins(fixture.context, app).add(str(repository), "main", "plugins/kit", reference=True)
        newer = PluginRepositoriesTest.release(repository, "1.1.0")

        assert AppPlugins(fixture.context, app).update("kit") == newer
        assert PluginRepositoriesTest.lock(app)["plugins"]["kit"]["commit"] == newer
        assert PluginRepositoriesTest.version(app, Configuration.RELEASE) == "1.1.0"

    def test_a_release_build_refuses_a_plugin_that_the_lock_does_not_pin_while_a_debug_build_takes_its_branch(self, tmp_path):
        fixture = ToolingFixture(tmp_path)
        repository = PluginRepositoriesTest.repository(fixture)
        PluginRepositoriesTest.release(repository, "1.1.0")
        app = fixture.app({"plugins": {"kit": {"git": "../repositories/kit.git", "ref": "main", "path": "plugins/kit"}}})

        with pytest.raises(InvalidManifest, match=re.escape('which "haylen.lock" does not pin to a commit')):
            PluginRepositoriesTest.version(app, Configuration.RELEASE)
        assert PluginRepositoriesTest.version(app, Configuration.DEBUG) == "1.1.0"

    def test_a_lock_that_names_another_ref_pins_nothing(self, tmp_path):
        fixture = ToolingFixture(tmp_path)
        app = fixture.app({})
        repository = PluginRepositoriesTest.repository(fixture)
        AppPlugins(fixture.context, app).add(str(repository), "main", "plugins/kit", reference=True)
        PluginRepositoriesTest.git(repository, "branch", "next")
        manifest = json.loads((app / "haylen.json").read_text(encoding="utf-8"))
        manifest["plugins"]["kit"]["ref"] = "next"
        (app / "haylen.json").write_text(json.dumps(manifest, indent=4), encoding="utf-8")

        with pytest.raises(InvalidManifest, match=re.escape('at "next", which "haylen.lock" does not pin')):
            PluginRepositoriesTest.version(app, Configuration.RELEASE)

    @pytest.mark.parametrize("path", ["../outside", "/plugins/kit", "plugins/../../outside", "escape"])
    def test_a_path_that_leads_outside_the_checkout_is_refused(self, tmp_path, path):
        fixture = ToolingFixture(tmp_path)
        repository = PluginRepositoriesTest.repository(fixture)
        outside = fixture.plugin(tmp_path / "outside", "kit", {})
        (repository / "escape").symlink_to(outside)
        pinned = PluginRepositoriesTest.commit(repository, "Add a link that leads outside the repository.")
        app = fixture.app({"plugins": {"kit": {"git": "../repositories/kit.git", "ref": pinned, "path": path}}})

        with pytest.raises(InvalidManifest, match=re.escape(f'The key "plugins.kit.path" names the folder "{path}" of a git repository, which leads outside it')):
            PluginRepositoriesTest.version(app, Configuration.RELEASE)

    def test_adding_a_repository_with_a_path_that_leads_outside_it_changes_nothing(self, tmp_path):
        fixture = ToolingFixture(tmp_path)
        app = fixture.app({})
        repository = PluginRepositoriesTest.repository(fixture)

        with pytest.raises(InvalidManifest, match=re.escape('The option "--path" names the folder "../.." of a git repository')):
            AppPlugins(fixture.context, app).add(str(repository), "main", "../..", reference=True)
        assert "plugins" not in json.loads((app / "haylen.json").read_text(encoding="utf-8"))
        assert not (app / PluginLock.FILE).exists()

    def test_listing_the_plugins_shows_where_each_one_comes_from(self, tmp_path):
        fixture = ToolingFixture(tmp_path)
        repository = PluginRepositoriesTest.repository(fixture)
        shared = fixture.plugin(tmp_path / "shared", "shared", {})
        app = fixture.app({})
        fixture.plugin(app, "local", {})
        AppPlugins(fixture.context, app).add(str(repository), "main", "plugins/kit", reference=True)
        AppPlugins(fixture.context, app).add(str(shared), None, None, reference=True)
        manifest = json.loads((app / "haylen.json").read_text(encoding="utf-8"))
        manifest["plugins"]["local"] = {}
        (app / "haylen.json").write_text(json.dumps(manifest, indent=4), encoding="utf-8")

        statuses = {status.id: status.describe() for status in AppPlugins(fixture.context, app).statuses()}
        assert {identifier: (status["source"], status["version"], status["problems"]) for identifier, status in statuses.items()} == {
            "kit": ("../repositories/kit.git", "1.0.0", []),
            "shared": ("../shared/plugins/shared", "1.0.0", []),
            "local": ("plugins/local", "1.0.0", []),
        }
