import functools
import os
import zipfile

import pytest

from haylen.toolchains.download import Download
from haylen.toolchains.gradle import Gradle
from tests.platforms.android.android_fixture import AndroidFixture


class GradleTest:
    @staticmethod
    def download(downloads: list[tuple[str, str]], url: str, archive, sha256: str, terminal) -> None:
        """A download of the tests that records what it fetched and writes a distribution of the pinned version."""
        downloads.append((url, sha256))
        with zipfile.ZipFile(archive, "w") as package:
            package.writestr(f"gradle-{Gradle.VERSION}/bin/{Gradle.executable().name}", "pinned")
            package.writestr(f"gradle-{Gradle.VERSION}/lib/gradle.jar", "library")

    @staticmethod
    def full_disk(*_: object) -> None:
        raise OSError("No space left on device")

    def test_the_pinned_distribution_moves_into_place_only_once_it_is_unpacked(self, tmp_path, monkeypatch):
        fixture = AndroidFixture(tmp_path / "fixture")
        tools = tmp_path / "tools"
        tools.mkdir()
        monkeypatch.setattr(Gradle, "FOLDER", tools / f"gradle-{Gradle.VERSION}")
        downloads: list[tuple[str, str]] = []
        monkeypatch.setattr(Download, "file", functools.partial(GradleTest.download, downloads))

        # An unpack that stops halfway leaves no folder whose executable seems installed.
        with monkeypatch.context() as failing:
            failing.setattr(zipfile.ZipFile, "extractall", GradleTest.full_disk)
            with pytest.raises(OSError):
                Gradle.path(fixture.context)
        assert list(tools.iterdir()) == []

        installed = Gradle.path(fixture.context)
        assert installed == Gradle.FOLDER / "bin" / Gradle.executable().name
        assert installed.read_text(encoding="utf-8") == "pinned"
        assert os.access(installed, os.X_OK)
        assert (Gradle.FOLDER / "lib" / "gradle.jar").read_text(encoding="utf-8") == "library"
        assert downloads == [(Gradle.URL, Gradle.SHA256)] * 2
        assert [path.name for path in tools.iterdir()] == [Gradle.FOLDER.name]

    def test_a_folder_that_an_interrupted_unpack_left_is_replaced(self, tmp_path, monkeypatch):
        fixture = AndroidFixture(tmp_path / "fixture")
        monkeypatch.setattr(Gradle, "FOLDER", tmp_path / "tools" / f"gradle-{Gradle.VERSION}")
        (Gradle.FOLDER / "lib").mkdir(parents=True)
        (Gradle.FOLDER / "lib" / "partial.jar").write_text("partial", encoding="utf-8")
        monkeypatch.setattr(Download, "file", functools.partial(GradleTest.download, []))

        assert Gradle.path(fixture.context).read_text(encoding="utf-8") == "pinned"
        assert not (Gradle.FOLDER / "lib" / "partial.jar").exists()
