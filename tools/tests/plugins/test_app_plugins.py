import json
import os
from pathlib import Path

import pytest

from haylen.core.errors.build_failed import BuildFailed
from haylen.plugins.app_plugins import AppPlugins
from tests.support.tooling_fixture import ToolingFixture


class AppPluginsTest:
    """The plugins of an app as `haylen plugin add` and `haylen plugin remove` change them: the copies under `plugins/` of the app and the entries of its `haylen.json`."""

    @staticmethod
    def kit(fixture: ToolingFixture) -> Path:
        """A plugin folder outside the app whose Apple framework keeps its current version as a symbolic link, as frameworks do."""
        folder = fixture.plugin(fixture.folder / "shared", "kit", {})
        versions = folder / "apple" / "Kit.framework" / "Versions"
        (versions / "A").mkdir(parents=True)
        (versions / "A" / "Kit").write_bytes(b"kit")
        (versions / "Current").symlink_to("A")
        return folder

    @staticmethod
    def plugins(app: Path) -> dict | None:
        return json.loads((app / "haylen.json").read_text(encoding="utf-8")).get("plugins")

    def test_adding_a_plugin_folder_copies_it_into_the_app_with_the_symbolic_links_of_its_frameworks(self, tmp_path):
        fixture = ToolingFixture(tmp_path)
        app = fixture.app({})
        AppPlugins(fixture.context, app).add(str(AppPluginsTest.kit(fixture)), None, None, reference=False)

        current = app / "plugins" / "kit" / "apple" / "Kit.framework" / "Versions" / "Current"
        assert current.is_symlink()
        assert current.readlink() == Path("A")
        assert (current / "Kit").read_bytes() == b"kit"
        assert AppPluginsTest.plugins(app) == {"kit": {}}

    def test_adding_a_reference_deletes_the_copy_and_writes_where_the_plugin_comes_from(self, tmp_path):
        fixture = ToolingFixture(tmp_path)
        app = fixture.app({})
        source = AppPluginsTest.kit(fixture)
        AppPlugins(fixture.context, app).add(str(source), None, None, reference=False)
        AppPlugins(fixture.context, app).add(str(source), None, None, reference=True)

        assert not (app / "plugins" / "kit").exists()
        assert AppPluginsTest.plugins(app) == {"kit": {"path": "../shared/plugins/kit"}}

    def test_removing_a_plugin_deletes_its_copy_and_its_entry(self, tmp_path):
        fixture = ToolingFixture(tmp_path)
        app = fixture.app({})
        AppPlugins(fixture.context, app).add(str(AppPluginsTest.kit(fixture)), None, None, reference=False)
        AppPlugins(fixture.context, app).remove("kit")

        assert not (app / "plugins").exists()
        assert AppPluginsTest.plugins(app) is None

    @pytest.mark.skipif(os.name == "nt" or os.geteuid() == 0, reason="Only a POSIX user who is not root keeps the files of a read-only folder.")
    def test_a_copy_that_cannot_be_removed_stops_adding_and_removing_the_plugin(self, tmp_path):
        fixture = ToolingFixture(tmp_path)
        app = fixture.app({})
        source = AppPluginsTest.kit(fixture)
        AppPlugins(fixture.context, app).add(str(source), None, None, reference=False)
        copy = app / "plugins" / "kit"
        copy.chmod(0o500)
        try:
            with pytest.raises(BuildFailed, match="could not be removed"):
                AppPlugins(fixture.context, app).add(str(source), None, None, reference=False)
            with pytest.raises(BuildFailed, match="could not be removed"):
                AppPlugins(fixture.context, app).remove("kit")
        finally:
            copy.chmod(0o700)

        assert AppPluginsTest.plugins(app) == {"kit": {}}
