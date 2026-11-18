import functools
import tarfile
from pathlib import Path

import pytest

from haylen.core.host import Host
from haylen.core.repository import Repository
from haylen.platforms.desktop.appimage_tools import AppImageTools
from haylen.toolchains.download import Download
from tests.support.tooling_fixture import ToolingFixture


class AppImageToolsTest:
    """The tools of the AppImage generator of CPack live under names with their versions, so a new pin of one of them downloads its own release."""

    @staticmethod
    def download(urls: list[str], folder: Path, url: str, target: Path, sha256: str, terminal: object) -> None:
        """A download of the tests that records what it fetched and writes a pinned tool, with patchelf in the archive of its release."""
        urls.append(url)
        target.parent.mkdir(parents=True, exist_ok=True)
        if "patchelf" not in url:
            target.write_text("pinned", encoding="utf-8")
            return
        binary = folder / "patchelf"
        binary.write_text("pinned", encoding="utf-8")
        with tarfile.open(target, "w:gz") as package:
            package.add(binary, "./bin/patchelf")

    @staticmethod
    def extract(command: list, cwd: Path) -> str:
        """Unpacks appimagetool from its own AppImage, as its option "--appimage-extract" does."""
        (cwd / "squashfs-root").mkdir()
        (cwd / "squashfs-root" / "AppRun").write_text("pinned", encoding="utf-8")
        return ""

    def test_a_new_pin_of_patchelf_downloads_its_own_release(self, tmp_path: Path, monkeypatch: pytest.MonkeyPatch):
        monkeypatch.setattr(Repository, "TOOLS", tmp_path / "tools")
        monkeypatch.setattr(Host, "system", staticmethod(lambda: "linux"))
        monkeypatch.setattr(Host, "architecture", staticmethod(lambda: "x86_64"))
        folder = AppImageTools.folder()
        for path in (folder / "appimagetool" / "AppRun", folder / f"runtime-{AppImageTools.RUNTIME_VERSION}-x86_64", folder / "patchelf-0.0.1" / "bin" / "patchelf"):
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("older", encoding="utf-8")

        urls: list[str] = []
        fixture = ToolingFixture(tmp_path)
        monkeypatch.setattr(Download, "file", staticmethod(functools.partial(AppImageToolsTest.download, urls, tmp_path)))
        monkeypatch.setattr(fixture.context.process, "capture", AppImageToolsTest.extract)
        tools = AppImageTools.ensure(fixture.context)

        assert tools.patchelf == folder / f"patchelf-{AppImageTools.PATCHELF_VERSION}" / "bin" / "patchelf"
        assert tools.patchelf.read_text(encoding="utf-8") == "pinned"
        assert any(f"/{AppImageTools.PATCHELF_VERSION}/" in url for url in urls)
        assert tools.appimagetool.read_text(encoding="utf-8") == "pinned"
