from pathlib import Path

from haylen.core.errors.missing_toolchain import MissingToolchain
from haylen.core.host import Host
from haylen.core.repository import Repository
from haylen.core.tooling_context import ToolingContext
from haylen.toolchains.download import Download


class InnoSetup:
    """The pinned Inno Setup, whose compiler the installer generator of CPack runs to make the installer of a Windows app. Its own installer puts it into `.tools` in portable mode, without an uninstaller or entries in the registry. The hash is the one of the x64 installer of the release."""

    VERSION = "7.1.1"
    SHA256 = "8a5a6711c3766a6e5fb74f280ba0e3551e719ce94538d901fdbd1f8f9920b3f0"

    @staticmethod
    def compiler_path() -> Path:
        return Repository.TOOLS / f"inno-setup-{InnoSetup.VERSION}" / "ISCC.exe"

    @staticmethod
    def compiler(context: ToolingContext) -> Path:
        """Returns the compiler of Inno Setup, installing the pinned release on the first package of a Windows app."""
        target = InnoSetup.compiler_path()
        if target.exists():
            return target
        if Host.system() != "windows":
            raise MissingToolchain(f'Inno Setup makes the installers of Windows apps on Windows alone, and this machine runs "{Host.system()}".')

        installer = Repository.TOOLS / f"innosetup-{InnoSetup.VERSION}-x64.exe"
        tag = InnoSetup.VERSION.replace(".", "_")
        Download.file(f"https://github.com/jrsoftware/issrc/releases/download/is-{tag}/innosetup-{InnoSetup.VERSION}-x64.exe", installer, InnoSetup.SHA256, context.terminal)
        context.process.run([installer, "/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART", "/CURRENTUSER", "/PORTABLE=1", f"/DIR={target.parent}"])
        installer.unlink()
        return target
