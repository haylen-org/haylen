from pathlib import Path

from haylen.core.host import Host
from haylen.core.repository import Repository
from haylen.core.tooling_context import ToolingContext
from haylen.platforms.tool_status import ToolStatus
from haylen.toolchains.download import Download


class Gradle:
    """The pinned Gradle, which builds the Android libraries of the engine and the Android projects of apps. Its distribution unpacks into `.tools/gradle-<version>`, so a new pin downloads its own. The hash is the one that Gradle publishes for the distribution in `https://services.gradle.org/distributions/gradle-<version>-bin.zip.sha256`, which the wrapper of the generated projects checks too."""

    VERSION = "9.8.2"
    SHA256 = "21bd5da2de49b642904077f38c0efdc7778e92b3ed0889392dca61b97e389607"
    FOLDER = Repository.TOOLS / f"gradle-{VERSION}"
    URL = f"https://services.gradle.org/distributions/gradle-{VERSION}-bin.zip"

    @staticmethod
    def executable() -> Path:
        return Gradle.FOLDER / "bin" / ("gradle.bat" if Host.system() == "windows" else "gradle")

    @staticmethod
    def path(context: ToolingContext) -> Path:
        """The executable, which the first call downloads."""
        executable = Gradle.executable()
        if executable.exists():
            return executable
        Download.archive(Gradle.URL, Gradle.SHA256, Gradle.FOLDER, executable, f"gradle-{Gradle.VERSION}", context.terminal)
        return executable

    @staticmethod
    def status() -> ToolStatus:
        """Tells whether the pinned Gradle is in `.tools` without downloading it. A missing Gradle is no problem, since the first build downloads it."""
        executable = Gradle.executable()
        return ToolStatus("Gradle", ToolStatus.Role.HOST, Gradle.VERSION, executable if executable.exists() else None, None)
