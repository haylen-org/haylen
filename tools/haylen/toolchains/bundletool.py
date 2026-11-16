import os
from pathlib import Path

from haylen.core.repository import Repository
from haylen.core.tooling_context import ToolingContext
from haylen.platforms.tool_status import ToolStatus
from haylen.toolchains.download import Download


class Bundletool:
    """The pinned bundletool of Google, a jar named after its version in `.tools`, which turns the bundle of an Android app into the APKs of one device and installs them in the local testing mode of Google Play, where the asset packs that Play delivers after the install wait on the device. The hash is the one of the `bundletool-all` jar of the release."""

    VERSION = "1.18.3"
    SHA256 = "a099cfa1543f55593bc2ed16a70a7c67fe54b1747bb7301f37fdfd6d91028e29"
    JAR = Repository.TOOLS / f"bundletool-{VERSION}.jar"
    URL = f"https://github.com/google/bundletool/releases/download/{VERSION}/bundletool-all-{VERSION}.jar"

    @staticmethod
    def path(context: ToolingContext) -> Path:
        """The jar, which the first call downloads and moves into place only once its hash matches."""
        if Bundletool.JAR.exists():
            return Bundletool.JAR
        partial = Bundletool.JAR.with_name(f"{Bundletool.JAR.name}.{os.getpid()}.part")
        Download.file(Bundletool.URL, partial, Bundletool.SHA256, context.terminal)
        partial.replace(Bundletool.JAR)
        return Bundletool.JAR

    @staticmethod
    def status() -> ToolStatus:
        """Tells whether the pinned bundletool is in `.tools` without downloading it. A missing jar is no problem, since the first install of a bundle downloads it."""
        return ToolStatus("bundletool", ToolStatus.Role.HOST, Bundletool.VERSION, Bundletool.JAR if Bundletool.JAR.exists() else None, None)
