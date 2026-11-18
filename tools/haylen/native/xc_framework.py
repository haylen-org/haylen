import plistlib
from pathlib import Path


class XcFramework:
    """A prebuilt xcframework of a native library, whose `Info.plist` lists a library for every platform and variant it holds, such as the variant `simulator` of `ios`."""

    @staticmethod
    def library(framework: Path, platform: str, variant: str | None) -> Path | None:
        """The library of a platform and variant, or nothing when the xcframework holds none."""
        for entry in plistlib.loads((framework / "Info.plist").read_bytes())["AvailableLibraries"]:
            if entry["SupportedPlatform"] == platform and entry.get("SupportedPlatformVariant") == variant:
                return framework / entry["LibraryIdentifier"] / entry["LibraryPath"]
        return None
