import hashlib
import re
from pathlib import Path


class AppIdentifier:
    """The identifier that an app whose `haylen.json` names none takes from the location of its package: `local.` and one part of the letters and digits of the last part of the location with eight hexadecimal digits of the BLAKE2b hash of the whole location, such as `local.skygarden3fa9c1d2`. The tooling gives an app folder the identifier of its resolved path, and the engine follows the same rule in `core::AppIdentifier` for every package, a folder or a zip file on disk, a launched package by the address it came from, and a package in memory by the location its creator names, so no two apps share an identifier however they run. It is a valid bundle identifier of Apple and application id of Android, whose parts start with a letter, and the domain `local` belongs to no one, so it never claims the domain of a publisher."""

    PREFIX = "local"
    # The part of a location whose name holds no letter, the letters of the name of an app without one.
    FALLBACK = "haylenapp"
    MAX_NAME = 40
    DIGEST_SIZE = 4

    @staticmethod
    def generate(folder: Path) -> str:
        return AppIdentifier.from_location(str(folder.resolve()))

    @staticmethod
    def from_location(location: str) -> str:
        """The identifier of a location, which keeps the ASCII letters of its last part in lowercase and its digits, without the digits it starts with. A location that holds `://` is an address, whose last part leaves out its query and fragment."""
        rest = re.split(r"[?#]", location, maxsplit=1)[0] if "://" in location else location
        name = re.split(r"[/\\]", rest.rstrip("/\\"))[-1]
        letters = re.sub(r"[^A-Za-z0-9]", "", name).lower()
        letters = re.sub(r"^[0-9]+", "", letters)[: AppIdentifier.MAX_NAME] or AppIdentifier.FALLBACK
        digest = hashlib.blake2b(location.encode(), digest_size=AppIdentifier.DIGEST_SIZE).hexdigest()
        return f"{AppIdentifier.PREFIX}.{letters}{digest}"
