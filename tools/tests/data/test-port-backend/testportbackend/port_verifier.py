from pathlib import Path

from haylen.core.repository import Repository
from haylen.platforms.build_product import BuildProduct
from haylen.platforms.verification_check import VerificationCheck
from testportbackend.linked_parts import LinkedParts


class PortVerifier:
    """The checks that the backend of the test port adds to the verification of a built app: the executable is a file, the folder `app` next to it holds the staged package of the app file by file, and the executable registers every part of its plugins under the id of its plugin with each of its symbols."""

    SOURCE = "test-port"
    CONTENT = "app"

    @staticmethod
    def checks(product: BuildProduct, executable: Path) -> tuple[VerificationCheck, ...]:
        if not executable.is_file():
            return (VerificationCheck.failed("executable", f"The executable `{Repository.shown(executable)}` does not exist.", PortVerifier.SOURCE),)
        checks = [VerificationCheck.passed("executable", f"The executable `{Repository.shown(executable)}` exists.", PortVerifier.SOURCE)]
        checks.append(PortVerifier.content(product.prepared.folders.package, executable.parent / PortVerifier.CONTENT))
        checks += PortVerifier.parts(LinkedParts.of(product.prepared.product.primary), executable)
        return tuple(checks)

    @staticmethod
    def content(staged: Path, shipped: Path) -> VerificationCheck:
        """Compares the content that ships next to the executable with the staged package, file by file."""
        expected = {path.relative_to(staged).as_posix(): path for path in staged.rglob("*") if path.is_file()}
        found = {path.relative_to(shipped).as_posix(): path for path in shipped.rglob("*") if path.is_file()} if shipped.is_dir() else {}
        missing = sorted(name for name in expected if name not in found or found[name].read_bytes() != expected[name].read_bytes())
        extra = sorted(name for name in found if name not in expected)
        if missing or extra:
            names = ", ".join(f'"{name}"' for name in (*missing, *extra))
            return VerificationCheck.failed("content", f"The content `{Repository.shown(shipped)}` differs from the package of the app in {names}. Build the app again.", PortVerifier.SOURCE)
        return VerificationCheck.passed("content", f"The content `{Repository.shown(shipped)}` holds the {len(expected)} files of the package of the app.", PortVerifier.SOURCE)

    @staticmethod
    def parts(parts: LinkedParts, executable: Path) -> list[VerificationCheck]:
        """Finds the names that the registration of each part writes into the executable, the id of its plugin and its symbols."""
        if not parts.parts:
            return []
        binary = executable.read_bytes()
        checks = []
        for part in parts.parts:
            absent = [name for name in (part.owner, *part.symbols) if b"\0" + name.encode() + b"\0" not in binary]
            if absent:
                names = ", ".join(f'"{name}"' for name in absent)
                checks.append(VerificationCheck.failed("parts", f'The executable does not register {names} of the part of "{part.owner}", so the part was not linked.', PortVerifier.SOURCE))
            else:
                checks.append(VerificationCheck.passed("parts", f'The executable registers the part of "{part.owner}" with {len(part.symbols)} symbols.', PortVerifier.SOURCE))
        return checks
