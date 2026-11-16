import base64
import hashlib
from collections.abc import Iterable, Mapping

from haylen.content.protected_release import ProtectedRelease
from haylen.content.release_entry import ReleaseEntry
from haylen.content.release_package import ReleasePackage
from haylen.platforms.verification_check import VerificationCheck


class ReleaseInspection:
    """Examines a built release product for what a release never ships: Lua text, the index of a development package, a file of the package as it is, files in its release folder besides the manifests and the shards, symbol files and a key of the app in any form, and a missing protected release. A backend adds its checks to the report of the product. The splash logo is the one file of the package that platforms show on their own, so it may ship as it is."""

    NAME = "release"
    SOURCE = "content"
    PACKAGE_INDEX = "haylen-package-index.json"
    # Symbol files stay with the developer, for crash reports.
    SYMBOL_SUFFIXES = (".pdb", ".debug", ".dwo", ".dsym", ".map", ".symbols")
    # A file of the package smaller than this may match an unrelated file of a product by chance, such as an empty file, so the check of raw files leaves it out.
    MATCH_MINIMUM = 16

    def __init__(self, source: ReleasePackage, secrets: Mapping[str, bytes]) -> None:
        self.source = source
        self.secrets = secrets

    @staticmethod
    def forms(secret: bytes) -> tuple[bytes, ...]:
        """The forms in which a key could show in a file: its bytes, its hexadecimal text in either case and its Base64 text."""
        return (secret, secret.hex().encode(), secret.hex().upper().encode(), base64.b64encode(secret), base64.urlsafe_b64encode(secret).rstrip(b"="))

    def checks(self, entries: Iterable[ReleaseEntry], release: str) -> tuple[VerificationCheck, ...]:
        """Returns a failed check for every problem of a product whose protected release lies in the folder `release` of it, or one passed check."""
        problems = self.problems(entries, release)
        if not problems:
            return (VerificationCheck.passed(ReleaseInspection.NAME, f'The product ships the package of the app only inside its protected release in "{release}".', ReleaseInspection.SOURCE),)
        return tuple(VerificationCheck.failed(ReleaseInspection.NAME, problem, ReleaseInspection.SOURCE) for problem in problems)

    def problems(self, entries: Iterable[ReleaseEntry], release: str) -> list[str]:
        package = self.package_files()
        sizes = {size for size, _ in package}
        problems: list[str] = []
        released = False
        for entry in entries:
            name = entry.path.rsplit("/", 1)[-1]
            if entry.path.startswith(f"{release}/"):
                released = released or name == ProtectedRelease.MANIFESTS[0]
                nested = "/" in entry.path[len(release) + 1 :]
                if nested or (name not in ProtectedRelease.MANIFESTS and not name.endswith(ProtectedRelease.SHARD_SUFFIX)):
                    problems.append(f'The release holds "{entry.path}", while its folder "{release}" holds only manifests and shards.')
            if name.endswith(".lua"):
                problems.append(f'The release holds the Lua text "{entry.path}".')
            if name == ReleaseInspection.PACKAGE_INDEX:
                problems.append(f'The release holds the index of a development package, "{entry.path}".')
            if any(part.lower().endswith(ReleaseInspection.SYMBOL_SUFFIXES) for part in entry.path.split("/")):
                problems.append(f'The release holds the symbols "{entry.path}", which stay with the developer.')

            # Shards hold only encrypted records, so neither a file of the package nor a key can show in them.
            if name.endswith(ProtectedRelease.SHARD_SUFFIX) or (entry.size not in sizes and not self.secrets):
                continue
            data = entry.read()
            match = package.get((entry.size, hashlib.sha256(data).hexdigest()))
            if match:
                problems.append(f'The release holds "{entry.path}", which is the file "{match}" of the package as it is.')
            for owner, secret in self.secrets.items():
                if any(form in data for form in ReleaseInspection.forms(secret)):
                    problems.append(f'The release holds {owner} of the app in "{entry.path}".')

        if not released:
            problems.append(f'The release has no protected release in "{release}".')
        return problems

    def package_files(self) -> dict[tuple[int, str], str]:
        """The files of the package that a release never ships as they are, by their size and hash."""
        files: dict[tuple[int, str], str] = {}
        for relative, path in self.source.package.files().items():
            size = path.stat().st_size
            if relative == self.source.splash_logo or size < ReleaseInspection.MATCH_MINIMUM:
                continue
            files[(size, hashlib.sha256(path.read_bytes()).hexdigest())] = relative
        return files
