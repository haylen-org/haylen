import re
from pathlib import Path

from haylen.core.errors.build_failed import BuildFailed
from haylen.core.tooling_context import ToolingContext


class AppleSymbolCheck:
    """Checks that the merged library of a slice of the Apple SDK keeps the symbols of OpenSSL and Poco hidden, so an app that links another copy of them, such as the SDK of a plugin, exports neither. The routines that OpenSSL writes in assembly mark themselves global and stay visible, and the check tells their objects by their Mach-O header, since the compiler marks every object that it compiles from C or C++ with `SUBSECTIONS_VIA_SYMBOLS` and the assembly of OpenSSL does not."""

    OPENSSL_MEMBERS = ("libcrypto-lib-", "libssl-lib-")
    COMPILED_FLAG = "SUBSECTIONS_VIA_SYMBOLS"
    # A line of `nm -A -m` names the archive and the member before the value, the section, the visibility and the symbol.
    SYMBOL = re.compile(r"^.*?:(?P<member>[^:]+\.o): \S+ \([^)]*\) (?P<kind>.*?) (?P<name>\S+)$")
    # A line of `otool -hv` that opens the header of a member of an archive.
    MEMBER = re.compile(r"\((?P<member>[^()]+\.o)\)( \(architecture \S+\))?:$")
    SHOWN = 5

    def __init__(self, context: ToolingContext) -> None:
        self.context = context

    def check(self, library: Path, sdk: str) -> None:
        """Fails with some of the symbols of OpenSSL or Poco that the library exports beyond the routines of OpenSSL in assembly."""
        exported: list[str] = []
        for architecture in self.context.process.capture(["xcrun", "lipo", "-archs", library]).split():
            compiled = self.compiled_members(library, architecture)
            exported += [name for member, name in self.visible(library, architecture) if AppleSymbolCheck.is_hidden_kind(member, name, compiled)]
        if exported:
            names = ", ".join(f'"{name}"' for name in sorted(set(exported))[: AppleSymbolCheck.SHOWN])
            raise BuildFailed(
                f'The library `{library}` of the slice of the SDK "{sdk}" exports {len(set(exported))} symbols of OpenSSL or Poco, such as {names}, so an app that links another copy of them would clash with the engine. They build with hidden symbols on Apple platforms, as the build guide describes.'
            )

    @staticmethod
    def is_hidden_kind(member: str, name: str, compiled: set[str]) -> bool:
        """Whether a visible symbol belongs to the code of OpenSSL or Poco that builds with hidden symbols: an object of OpenSSL that the compiler made, or anything of the namespace of Poco."""
        if member.startswith(AppleSymbolCheck.OPENSSL_MEMBERS):
            return member in compiled
        return name.startswith("__Z") and "4Poco" in name

    def visible(self, library: Path, architecture: str) -> list[tuple[str, str]]:
        """The member and the name of every symbol that an object of the library defines and that an executable which links it would export."""
        found = []
        for line in self.context.process.capture(["xcrun", "nm", "-A", "-m", "-U", "-arch", architecture, library]).splitlines():
            match = AppleSymbolCheck.SYMBOL.match(line)
            if match is None:
                continue
            kind = match["kind"].split()
            if "external" in kind and "private" not in kind and "non-external" not in kind and "hidden" not in kind:
                found.append((match["member"], match["name"]))
        return found

    def compiled_members(self, library: Path, architecture: str) -> set[str]:
        """The members of the library whose Mach-O header carries the flag that the compiler gives every object it makes."""
        compiled = set()
        member = ""
        for line in self.context.process.capture(["xcrun", "otool", "-hv", "-arch", architecture, library]).splitlines():
            match = AppleSymbolCheck.MEMBER.search(line)
            if match is not None:
                member = match["member"]
            elif member and AppleSymbolCheck.COMPILED_FLAG in line.split():
                compiled.add(member)
        return compiled
