import tarfile
from dataclasses import dataclass
from pathlib import Path

from haylen.core.errors.missing_toolchain import MissingToolchain
from haylen.core.folders import Folders
from haylen.core.host import Host
from haylen.core.repository import Repository
from haylen.core.tooling_context import ToolingContext
from haylen.toolchains.download import Download


@dataclass(frozen=True)
class AppImageTools:
    """The pinned tools that the AppImage generator of CPack runs to pack a Linux app: appimagetool, unpacked from its own AppImage so it runs without FUSE, the AppImage runtime that starts the image, and patchelf, which points the libraries that the image bundles at it. Each tool lives under a name with its version, so a new pin of any of them downloads its own release. The hashes are the ones of the release assets of each architecture."""

    APPIMAGETOOL_VERSION = "1.9.1"
    RUNTIME_VERSION = "20251108"
    PATCHELF_VERSION = "0.19.3"
    RELEASES = "https://github.com"
    # The release assets name the architectures as Linux does.
    ARCHITECTURES = {"x86_64": "x86_64", "arm64": "aarch64"}
    SHA256 = {
        "x86_64": {
            "appimagetool": "ed4ce84f0d9caff66f50bcca6ff6f35aae54ce8135408b3fa33abfc3cb384eb0",
            "runtime": "2fca8b443c92510f1483a883f60061ad09b46b978b2631c807cd873a47ec260d",
            "patchelf": "38861b171079e440b25ac86483284f690d0ee7921dba98b0c8fe04a9b0a11667",
        },
        "aarch64": {
            "appimagetool": "f0837e7448a0c1e4e650a93bb3e85802546e60654ef287576f46c71c126a9158",
            "runtime": "00cbdfcf917cc6c0ff6d3347d59e0ca1f7f45a6df1a428a0d6d8a78664d87444",
            "patchelf": "2281dd071995b449b9ce7c53c71d6d56ec3a280ae1cc66b779442abf624f7c10",
        },
    }

    appimagetool: Path
    runtime: Path
    patchelf: Path

    @staticmethod
    def folder() -> Path:
        return Repository.TOOLS / f"appimage-{AppImageTools.APPIMAGETOOL_VERSION}"

    @staticmethod
    def of(architecture: str) -> AppImageTools:
        folder = AppImageTools.folder()
        return AppImageTools(
            appimagetool=folder / "appimagetool" / "AppRun",
            runtime=folder / f"runtime-{AppImageTools.RUNTIME_VERSION}-{architecture}",
            patchelf=folder / f"patchelf-{AppImageTools.PATCHELF_VERSION}" / "bin" / "patchelf",
        )

    @staticmethod
    def ensure(context: ToolingContext) -> AppImageTools:
        """Returns the tools for the architecture of this machine, downloading the pinned releases again when one of them is missing."""
        architecture = AppImageTools.ARCHITECTURES.get(Host.architecture())
        if architecture is None:
            raise MissingToolchain(f'The AppImage tools exist for "x86_64" and "arm64", and this machine is "{Host.architecture()}".')
        tools = AppImageTools.of(architecture)
        if all(path.exists() for path in (tools.appimagetool, tools.runtime, tools.patchelf)):
            return tools
        if Host.system() != "linux":
            raise MissingToolchain(f'The AppImage tools pack Linux apps on Linux alone, and this machine runs "{Host.system()}".')

        hashes = AppImageTools.SHA256[architecture]
        folder = AppImageTools.folder()
        Folders.remove(folder)
        image = folder / f"appimagetool-{architecture}.AppImage"
        Download.file(
            f"{AppImageTools.RELEASES}/AppImage/appimagetool/releases/download/{AppImageTools.APPIMAGETOOL_VERSION}/appimagetool-{architecture}.AppImage",
            image,
            hashes["appimagetool"],
            context.terminal,
        )
        image.chmod(0o755)
        context.process.capture([image, "--appimage-extract"], cwd=folder)
        (folder / "squashfs-root").rename(tools.appimagetool.parent)
        image.unlink()

        Download.file(f"{AppImageTools.RELEASES}/AppImage/type2-runtime/releases/download/{AppImageTools.RUNTIME_VERSION}/runtime-{architecture}", tools.runtime, hashes["runtime"], context.terminal)
        archive = folder / "patchelf.tar.gz"
        patchelf = f"{AppImageTools.RELEASES}/NixOS/patchelf/releases/download/{AppImageTools.PATCHELF_VERSION}/patchelf-{AppImageTools.PATCHELF_VERSION}-{architecture}.tar.gz"
        Download.file(patchelf, archive, hashes["patchelf"], context.terminal)
        with tarfile.open(archive) as package:
            package.extract("./bin/patchelf", tools.patchelf.parent.parent, filter="data")
        archive.unlink()
        return tools

    def cpack_options(self) -> list[str]:
        return [
            f"-DCPACK_APPIMAGE_TOOL_EXECUTABLE={self.appimagetool}",
            f"-DCPACK_APPIMAGE_PATCHELF_EXECUTABLE={self.patchelf}",
            f"-DCPACK_APPIMAGE_RUNTIME_FILE={self.runtime}",
        ]
