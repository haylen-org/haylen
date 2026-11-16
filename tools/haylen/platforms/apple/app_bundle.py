import plistlib
import re
import tempfile
from pathlib import Path

from haylen.core.tooling_context import ToolingContext
from haylen.platforms.apple.property_lists import PropertyLists


class AppBundle:
    """Reads what a built Apple app holds: its Info.plist, its executable and the code it loads, the architectures of its code, the system frameworks and libraries it links, the Objective-C classes of its code, the entitlements it is signed with and its privacy manifest. A macOS or Mac Catalyst app keeps its files in `Contents`, and the others at the root of the bundle."""

    def __init__(self, context: ToolingContext, path: Path, mac: bool) -> None:
        self.context = context
        self.path = path
        self.contents = path / "Contents" if mac else path
        self.resources = self.contents / "Resources" if mac else path
        self.info = PropertyLists.read(self.contents / "Info.plist")
        self.executable = self.contents / "MacOS" / self.info["CFBundleExecutable"] if mac else path / self.info["CFBundleExecutable"]

    @property
    def identifier(self) -> str:
        return str(self.info["CFBundleIdentifier"])

    @property
    def package(self) -> Path:
        """The folder of the package or the protected release inside the bundle."""
        return self.resources / "app"

    def code(self) -> list[Path]:
        """The files that hold the code of the app: its executable and, in the Debug builds of Xcode, the `.debug.dylib` next to it, which holds the code while the executable only loads it."""
        debug = self.executable.with_name(self.executable.name + ".debug.dylib")
        return [self.executable, *([debug] if debug.is_file() else [])]

    def architectures(self) -> set[str]:
        return set(self.context.process.capture(["lipo", "-archs", self.executable]).split())

    def linked(self) -> set[str]:
        """The system frameworks and libraries that the code links, such as `Metal.framework` and `libz.tbd`."""
        linked = set()
        for code in self.code():
            for line in self.context.process.capture(["otool", "-L", code]).splitlines()[1:]:
                path = line.strip().split(" (")[0]
                framework = re.search(r"/([^/]+)\.framework/", path)
                library = re.search(r"/lib([^/.]+)[^/]*\.dylib$", path)
                if framework:
                    linked.add(f"{framework[1]}.framework")
                elif library:
                    linked.add(f"lib{library[1]}.tbd")
        return linked

    def classes(self) -> set[str]:
        """The runtime names of the Objective-C classes of the code, from its `__objc_classname` section, which the runtime finds plugin classes by and which stays in release builds, whose symbols are stripped."""
        found = set()
        for code in self.code():
            found |= AppBundle.class_names(self.context.process.capture(["otool", "-v", "-s", "__TEXT", "__objc_classname", code]))
        return found

    @staticmethod
    def class_names(listing: str) -> set[str]:
        return {parts[1] for line in listing.splitlines() if len(parts := line.split()) == 2 and re.fullmatch(r"[0-9a-f]+", parts[0])}

    def entitlements(self) -> dict:
        """The entitlements that the app is signed with. Apps for simulators carry them in a section of their executable instead of their signature, whose entitlements are then empty."""
        _, signed = self.context.process.attempt(["codesign", "-d", "--entitlements", "-", "--xml", self.path])
        if signed.strip():
            entitlements = plistlib.loads(signed)
            if entitlements:
                return entitlements
        with tempfile.TemporaryDirectory() as scratch:
            executable = self.executable
            # The tool reads one architecture, so a universal executable gives it its first slice.
            architectures = sorted(self.architectures())
            if len(architectures) > 1:
                thin = Path(scratch) / "executable"
                self.context.process.run(["lipo", executable, "-thin", architectures[0], "-output", thin])
                executable = thin
            section = Path(scratch) / "entitlements"
            self.context.process.attempt(["segedit", executable, "-extract", "__TEXT", "__entitlements", section])
            return plistlib.loads(section.read_bytes()) if section.is_file() and section.read_bytes().strip() else {}

    def privacy(self) -> dict:
        path = self.resources / "PrivacyInfo.xcprivacy"
        return PropertyLists.read(path) if path.is_file() else {}
