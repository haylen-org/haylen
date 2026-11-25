import hashlib
import re
from pathlib import Path


class AppIdentifier:
    """The identifier that the tooling gives an app whose `haylen.json` names none: `local.` and one part of the letters and digits of the folder name with eight hexadecimal digits of the hash of its absolute path, such as `local.skygarden3fa9c1d2`. It is unique on this machine, never shared between two app folders, and a valid bundle identifier of Apple and application id of Android, whose parts start with a letter. The domain `local` belongs to no one, so the identifier never claims the domain of a publisher."""

    PREFIX = "local"
    # The part of an app folder whose name holds no letter, the letters of the name of an app without one.
    FALLBACK = "haylenapp"
    MAX_NAME = 40

    @staticmethod
    def generate(folder: Path) -> str:
        name = re.sub(r"[^a-z0-9]", "", folder.name.lower())
        name = re.sub(r"^[0-9]+", "", name)[: AppIdentifier.MAX_NAME] or AppIdentifier.FALLBACK
        digest = hashlib.sha256(str(folder.resolve()).encode()).hexdigest()[:8]
        return f"{AppIdentifier.PREFIX}.{name}{digest}"
