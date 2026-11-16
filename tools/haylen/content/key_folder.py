import json
from collections.abc import Mapping
from dataclasses import dataclass
from pathlib import Path

from haylen.content.content_tool import ContentTool
from haylen.core.errors.missing_keys import MissingKeys
from haylen.core.host import Host
from haylen.core.repository import Repository


@dataclass(frozen=True)
class KeyFolder:
    """The key folder of an app, which holds its content keys and the seed of its signing key: under the folder that `HAYLEN_KEYS_DIR` names, as continuous integration gives it, or else under the configuration folder of the user, never inside a repository, a project or a build folder. The tooling reads the keys only to check that no release holds them, and never prints them."""

    VARIABLE = "HAYLEN_KEYS_DIR"
    INDEX = "keys.json"
    SIGNING_KEY = "signing.key"

    identifier: str
    folder: Path
    received: bool

    @staticmethod
    def of(identifier: str, environment: Mapping[str, str]) -> KeyFolder:
        root = environment.get(KeyFolder.VARIABLE)
        if root:
            return KeyFolder(identifier, Path(root).expanduser() / identifier, True)
        return KeyFolder(identifier, KeyFolder.user_config_folder(environment) / "keys" / identifier, False)

    @staticmethod
    def user_config_folder(environment: Mapping[str, str]) -> Path:
        """The folder where the tooling keeps what belongs to the user rather than to a project, such as the keys of apps."""
        system = Host.system()
        if system == "macos":
            return Path.home() / "Library" / "Application Support" / "Haylen"
        if system == "windows":
            return Path(environment["APPDATA"]) / "Haylen"
        configuration = environment.get("XDG_CONFIG_HOME")
        return (Path(configuration) if configuration else Path.home() / ".config") / "haylen"

    @property
    def has_keys(self) -> bool:
        return (self.folder / KeyFolder.INDEX).is_file()

    def ensure(self, tool: ContentTool) -> Path:
        """Returns the key folder, and creates the keys of the app on the machine of the developer when it has none yet. A folder that `HAYLEN_KEYS_DIR` names is received and never filled, since every release of an app needs the same keys."""
        if self.has_keys:
            return self.folder
        shown = f"`{Repository.shown(self.folder)}`"
        if self.received:
            raise MissingKeys(
                f'The key folder {shown} holds no keys of "{self.identifier}". Give "{KeyFolder.VARIABLE}" a copy of the key folders made where the releases of the app were built first.'
            )

        terminal = tool.context.terminal
        terminal.step(f'Creating the content keys of "{self.identifier}"')
        tool.run(["keys", "create", self.folder, "--identifier", self.identifier])
        terminal.warning(
            f"Created the keys of the protected releases of the app in {shown}.",
            f'Back up this folder, since every release of the app needs its keys, and give continuous integration a copy through "{KeyFolder.VARIABLE}".',
        )
        return self.folder

    def secrets(self) -> dict[str, bytes]:
        """Reads the content keys and the seed of the signing key by the words that name them, for the check that no release holds them in any form, or nothing when the app has no keys yet."""
        if not self.has_keys:
            return {}
        index = json.loads((self.folder / KeyFolder.INDEX).read_text(encoding="utf-8"))
        secrets = {f'the content key "{identifier}"': (self.folder / f"content-{identifier}.key").read_bytes() for identifier in index["contentKeys"]}
        secrets["the signing key"] = (self.folder / KeyFolder.SIGNING_KEY).read_bytes()
        return secrets
