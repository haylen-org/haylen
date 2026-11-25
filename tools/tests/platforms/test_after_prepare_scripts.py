import re
from pathlib import Path

import pytest

from haylen.core.errors.build_failed import BuildFailed
from haylen.core.errors.invalid_manifest import InvalidManifest
from haylen.platforms.desktop.desktop_backend import DesktopBackend
from tests.platforms.desktop.desktop_fixture import DesktopFixture


class AfterPrepareScriptsTest:
    """The `afterPrepare` scripts of the app, which change the generated project after every preparation that writes it, keep the files they write, join the inputs of the preparation and stop it with the script named when they fail."""

    SCRIPT = "\n".join(
        [
            "import os",
            "import sys",
            "from pathlib import Path",
            "",
            "project = Path(sys.argv[1])",
            "assert Path(os.environ['HAYLEN_PROJECT']) == project",
            "(project / 'extra.txt').write_text(os.environ['HAYLEN_TARGET'] + ' ' + os.environ['HAYLEN_PLATFORM'])",
            "values = project / 'app.cmake'",
            "values.write_text(values.read_text() + '# Changed by the app.\\n')",
            "",
        ]
    )

    @pytest.fixture
    def fixture(self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> DesktopFixture:
        return DesktopFixture(tmp_path, monkeypatch)

    @staticmethod
    def app(fixture: DesktopFixture, script: str, name: str = "garden") -> Path:
        app = fixture.app({"platforms": {"linux": {"afterPrepare": "native/after-prepare.py"}}}, name)
        (app / "native").mkdir()
        (app / "native" / "after-prepare.py").write_text(script, encoding="utf-8")
        return app

    def test_a_script_changes_the_project_and_a_preparation_of_the_same_inputs_keeps_it(self, fixture):
        app = AfterPrepareScriptsTest.app(fixture, AfterPrepareScriptsTest.SCRIPT)
        product = fixture.resolve(app, "linux")
        backend = DesktopBackend(fixture.context)

        first = backend.prepare(product, DesktopFixture.folders(product))
        assert first.changed
        assert (first.project / "extra.txt").read_text(encoding="utf-8") == "linux linux"
        assert (first.project / "app.cmake").read_text(encoding="utf-8").endswith("# Changed by the app.\n")

        again = backend.prepare(product, DesktopFixture.folders(product))
        assert not again.changed
        assert (again.project / "app.cmake").read_text(encoding="utf-8").count("# Changed by the app.") == 1

        (app / "native" / "after-prepare.py").write_text(AfterPrepareScriptsTest.SCRIPT.replace("extra.txt", "other.txt"), encoding="utf-8")
        changed = fixture.resolve(app, "linux")
        rewritten = backend.prepare(changed, DesktopFixture.folders(changed))
        assert rewritten.changed
        assert (rewritten.project / "other.txt").is_file()
        assert not (rewritten.project / "extra.txt").exists(), "A file that the script no longer writes leaves the project."

        origin = changed.primary.setting_origins()["linux"]["afterPrepare"]
        assert origin == {"value": "native/after-prepare.py", "origin": f"{changed.app.origin} at platforms.linux.afterPrepare"}

    def test_a_script_that_fails_stops_the_preparation_with_its_name(self, fixture):
        app = AfterPrepareScriptsTest.app(fixture, "raise SystemExit(3)\n")
        product = fixture.resolve(app, "linux")
        with pytest.raises(BuildFailed, match=re.escape('of "platforms.linux.afterPrepare" in `') + r".+" + re.escape('` failed on the project of "linux", so the preparation stops.')):
            DesktopBackend(fixture.context).prepare(product, DesktopFixture.folders(product))

    def test_the_key_names_a_python_script_of_the_app_folder(self, fixture):
        app = fixture.app({"platforms": {"linux": {"afterPrepare": "missing.py"}}})
        with pytest.raises(InvalidManifest, match=re.escape('The key "platforms.linux.afterPrepare" must name a Python script of the app folder that exists')):
            fixture.resolve(app, "linux")
