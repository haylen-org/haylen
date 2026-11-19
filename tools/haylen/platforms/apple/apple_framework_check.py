import re
from collections.abc import Iterable
from pathlib import Path

from haylen.core.errors.build_failed import BuildFailed
from haylen.core.tooling_context import ToolingContext


class AppleFrameworkCheck:
    """Checks that every system framework that a slice of the Apple SDK links gives the merged library of the slice at least one of the symbols that it leaves undefined, from the stubs of the framework in the SDK of Xcode, so the engine links no framework that it does not use. A framework that only re-exports others, such as an umbrella framework, gives no symbol of its own and fails the check, since the slice links the frameworks that hold its symbols instead."""

    # The lists of a stub that name the symbols of the framework, where an Objective-C class is a name without its prefix.
    SECTIONS = re.compile(r"^(exports|reexports|undefineds):", re.MULTILINE)
    SYMBOL_LISTS = re.compile(r"\b(symbols|weak-symbols|thread-local-symbols|objc-classes|objc-eh-types|objc-ivars):\s*\[([^\]]*)\]")
    CLASS_PREFIXES = ("_OBJC_CLASS_$_", "_OBJC_METACLASS_$_")
    IVAR_PREFIX = "_OBJC_IVAR_$_"
    EH_TYPE_PREFIX = "_OBJC_EHTYPE_$_"

    def __init__(self, context: ToolingContext) -> None:
        self.context = context

    def check(self, library: Path, sdk: str, frameworks: Iterable[str]) -> None:
        """Fails with the frameworks of the slice that give the library no symbol, named by their names in the SDK, such as `MetalKit.framework`."""
        root = Path(self.context.process.capture(["xcrun", "--sdk", sdk, "--show-sdk-path"]).strip())
        undefined = self.undefined(library)
        unused = [framework for framework in frameworks if not undefined & AppleFrameworkCheck.exports(root, framework)]
        if unused:
            names = ", ".join(f'"{name}"' for name in unused)
            raise BuildFailed(
                f'The engine links {names} on the slice of the SDK "{sdk}", while no symbol of `{library}` comes from them. Remove them from the list of the platform in `engine/cmake/haylen-app.cmake`.'
            )

    def undefined(self, library: Path) -> set[str]:
        """The symbols that the objects of the library use and none of them defines."""
        used: set[str] = set()
        defined: set[str] = set()
        for line in self.context.process.capture(["xcrun", "nm", "-g", "-P", library]).splitlines():
            parts = line.split()
            if len(parts) < 2 or parts[0].endswith(":"):
                continue
            (used if parts[1] == "U" else defined).add(parts[0])
        return used - defined

    @staticmethod
    def exports(root: Path, framework: str) -> set[str]:
        """The symbols that the stub of a framework of the SDK exports itself, without the frameworks that it re-exports, or nothing for a framework that the SDK lacks."""
        name = framework.removesuffix(".framework")
        stub = root / "System" / "Library" / "Frameworks" / f"{name}.framework" / f"{name}.tbd"
        if not stub.is_file():
            return set()
        return AppleFrameworkCheck.symbols(stub.read_text(encoding="utf-8"))

    @staticmethod
    def symbols(stub: str) -> set[str]:
        """The symbol names that the `exports` of the text of a stub of the text-based format of Apple list, with the classes, the instance variables and the exception types of Objective-C under the names that objects refer to them by, and without what it re-exports or uses."""
        parts = AppleFrameworkCheck.SECTIONS.split(stub)
        exported = "".join(text for section, text in zip(parts[1::2], parts[2::2], strict=True) if section == "exports")
        found: set[str] = set()
        for kind, items in AppleFrameworkCheck.SYMBOL_LISTS.findall(exported):
            for item in (part.strip().strip("'\"") for part in items.split(",")):
                if not item:
                    continue
                if kind == "objc-classes":
                    found.update(prefix + item for prefix in AppleFrameworkCheck.CLASS_PREFIXES)
                elif kind == "objc-ivars":
                    found.add(AppleFrameworkCheck.IVAR_PREFIX + item)
                elif kind == "objc-eh-types":
                    found.add(AppleFrameworkCheck.EH_TYPE_PREFIX + item)
                else:
                    found.add(item)
        return found
