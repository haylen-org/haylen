import re
import zipfile
from pathlib import Path

from haylen.core.tooling_context import ToolingContext
from haylen.toolchains.android_sdk import AndroidSdk


class AndroidApk:
    """An APK or a bundle that a build made, read for what the verification checks: the names of its entries, the bytes of an entry, the shards that it stores compressed and the elements of its merged manifest, which `aapt2` of the Android SDK reads from the binary XML."""

    ELEMENT = re.compile(r"\s*E: (\S+)")
    NAME = re.compile(r'\s*A: http://schemas.android.com/apk/res/android:name\(0x[0-9a-f]+\)="([^"]*)"')

    def __init__(self, path: Path) -> None:
        self.path = path

    def entries(self) -> set[str]:
        with zipfile.ZipFile(self.path) as archive:
            return set(archive.namelist())

    def read(self, entry: str) -> bytes:
        with zipfile.ZipFile(self.path) as archive:
            return archive.read(entry)

    def compressed(self, suffix: str) -> list[str]:
        """The entries of a suffix that the archive stores compressed."""
        with zipfile.ZipFile(self.path) as archive:
            return sorted(info.filename for info in archive.infolist() if info.filename.endswith(suffix) and info.compress_type != zipfile.ZIP_STORED)

    def manifest(self, context: ToolingContext) -> dict[str, set[str]]:
        """The names of the elements of the merged manifest by tag, such as the permissions, the activities, the providers and the meta-data."""
        aapt = AndroidSdk(context.environment).build_tool("aapt2")
        dump = context.process.capture([aapt, "dump", "xmltree", "--file", "AndroidManifest.xml", self.path])
        elements: dict[str, set[str]] = {}
        tag = ""
        for line in dump.splitlines():
            element = AndroidApk.ELEMENT.match(line)
            if element:
                tag = element[1]
                continue
            name = AndroidApk.NAME.match(line)
            if name:
                elements.setdefault(tag, set()).add(name[1])
        return elements
