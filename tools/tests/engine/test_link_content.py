import os
import shutil
import subprocess
from pathlib import Path

import pytest

from haylen.core.repository import Repository


@pytest.mark.skipif(os.name == "nt" or shutil.which("cmake") is None, reason="The script makes a junction on Windows, and it runs through CMake.")
class LinkContentTest:
    """The CMake script that links the folder of the content of an app to its staged package."""

    SCRIPT = Repository.ENGINE / "cmake" / "haylen-link-content.cmake"

    @staticmethod
    def link(source: Path, destination: Path) -> None:
        subprocess.run(["cmake", f"-DSOURCE={source}", f"-DDESTINATION={destination}", "-P", LinkContentTest.SCRIPT], check=True, capture_output=True)

    def test_the_destination_becomes_a_link_to_the_package_in_place_of_a_plain_folder(self, tmp_path):
        package = tmp_path / "package"
        package.mkdir()
        (package / "haylen.json").write_text("{}\n", encoding="utf-8")
        content = tmp_path / "app" / "content"
        content.mkdir(parents=True)
        (content / "stale.lua").write_text("return {}\n", encoding="utf-8")

        LinkContentTest.link(package, content)

        assert content.is_symlink()
        assert content.readlink() == package
        assert sorted(path.name for path in content.iterdir()) == ["haylen.json"]

    @pytest.mark.skipif(os.name != "nt" and os.geteuid() == 0, reason="Only a user who is not root keeps the entries of a read-only folder.")
    def test_linking_again_keeps_the_link_that_is_there(self, tmp_path):
        package = tmp_path / "package"
        package.mkdir()
        app = tmp_path / "app"
        app.mkdir()
        content = app / "content"
        LinkContentTest.link(package, content)

        # A folder whose entries cannot change fails a script that would make the link again.
        app.chmod(0o500)
        try:
            LinkContentTest.link(package, content)
        finally:
            app.chmod(0o700)

        assert content.is_symlink()
        assert content.readlink() == package
