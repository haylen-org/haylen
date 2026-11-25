import hashlib
import re
from pathlib import Path


class AppIdentifier:
    """The identifier that the tooling gives an app whose `haylen.json` names none: `local.` and one part of the letters and digits of the folder name with eight hexadecimal digits of the BLAKE2b hash of its resolved path, such as `local.skygarden3fa9c1d2`. The engine follows the same rule for a package on disk that names none, in `core::AppIdentifier`, so a folder has one identifier however it runs. It is unique on this machine, never shared between two app folders, and a valid bundle identifier of Apple and application id of Android, whose parts start with a letter. The domain `local` belongs to no one, so the identifier never claims the domain of a publisher."""

    PREFIX = "local"
    # The part of an app folder whose name holds no letter, the letters of the name of an app without one.
    FALLBACK = "haylenapp"
    MAX_NAME = 40
    DIGEST_SIZE = 4

    @staticmethod
    def generate(folder: Path) -> str:
        resolved = folder.resolve()
        return AppIdentifier.compose(resolved.name, str(resolved))

    @staticmethod
    def compose(name: str, resolved_path: str) -> str:
        """The identifier of a name and a resolved path, which keeps the ASCII letters of the name in lowercase and its digits, without the digits it starts with."""
        letters = re.sub(r"[^A-Za-z0-9]", "", name).lower()
        letters = re.sub(r"^[0-9]+", "", letters)[: AppIdentifier.MAX_NAME] or AppIdentifier.FALLBACK
        digest = hashlib.blake2b(resolved_path.encode(), digest_size=AppIdentifier.DIGEST_SIZE).hexdigest()
        return f"{AppIdentifier.PREFIX}.{letters}{digest}"
