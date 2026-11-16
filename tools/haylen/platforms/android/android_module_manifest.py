import re
import xml.etree.ElementTree
from pathlib import Path


class AndroidModuleManifest:
    """The manifest of the Android library module of a plugin, read for what it brings to the merged manifest of the app: its permissions, its components and its meta-data, with the names that start with a dot completed by the namespace of the module."""

    ANDROID = "{http://schemas.android.com/apk/res/android}"
    TOOLS = "{http://schemas.android.com/tools}"
    COMPONENTS = ("activity", "activity-alias", "service", "receiver", "provider")
    NAMESPACE = re.compile(r'namespace\s*=\s*"([^"]+)"')

    @staticmethod
    def needs(module: Path) -> list[tuple[str, str]]:
        """The tags and names that the module adds, leaving out the names that read a placeholder and the entries that it removes."""
        root = xml.etree.ElementTree.parse(module / "src" / "main" / "AndroidManifest.xml").getroot()
        script = module / "build.gradle.kts"
        found = AndroidModuleManifest.NAMESPACE.search(script.read_text(encoding="utf-8")) if script.is_file() else None
        needs = []
        for element in root.iter():
            name = element.get(f"{AndroidModuleManifest.ANDROID}name", "")
            if element.tag not in ("uses-permission", "meta-data", *AndroidModuleManifest.COMPONENTS) or not name or "${" in name:
                continue
            if element.get(f"{AndroidModuleManifest.TOOLS}node") == "remove":
                continue
            if name.startswith(".") and found:
                name = found[1] + name
            needs.append((element.tag, name))
        return needs
