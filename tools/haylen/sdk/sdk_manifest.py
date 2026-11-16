import json
from collections.abc import Mapping
from dataclasses import dataclass
from pathlib import Path

from haylen.core.configuration import Configuration
from haylen.core.errors.incompatible_sdk import IncompatibleSdk
from haylen.core.frozen import Frozen
from haylen.core.repository import Repository
from haylen.core.schema_validator import SchemaValidator
from haylen.sdk.host_tool import HostTool
from haylen.sdk.sdk_slice import SdkSlice


@dataclass(frozen=True)
class SdkManifest:
    """The `haylen-sdk.json` of an installed engine SDK, which its engine port writes: the port and its version, the engine version and the revision of the engine sources it was built from, its configuration, its slices, its host tools and its shared files by name, relative to the SDK folder. The identity of an SDK is apart from the version of the backend contract, and its selection checks both."""

    FILE = "haylen-sdk.json"
    FORMAT = 1

    folder: Path
    port: str
    port_version: str
    engine_version: str
    revision: str
    configuration: Configuration
    slices: tuple[SdkSlice, ...]
    host_tools: tuple[HostTool, ...]
    files: Mapping[str, str]

    @staticmethod
    def load(folder: Path) -> SdkManifest:
        path = folder / SdkManifest.FILE
        origin = Repository.shown(path)
        try:
            document = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as error:
            raise IncompatibleSdk(f"The SDK manifest `{origin}` cannot be read: {error}.", origin=origin) from None
        problems = SchemaValidator.of("sdk.schema.json").problems(document)
        if problems:
            lines = "\n".join(f"  {problem}" for problem in problems)
            raise IncompatibleSdk(f'The SDK manifest `{origin}` has these problems, so its SDK is incomplete. Build it again with "haylen sdk build".\n{lines}', origin=origin)
        return SdkManifest(
            folder=folder.resolve(),
            port=document["port"],
            port_version=document["portVersion"],
            engine_version=document["engineVersion"],
            revision=document["revision"],
            configuration=Configuration(document["configuration"]),
            slices=tuple(SdkSlice.parse(entry) for entry in document["slices"]),
            host_tools=tuple(HostTool.parse(entry) for entry in document.get("hostTools", [])),
            files=Frozen.freeze(dict(document.get("files", {}))),
        )

    def save(self) -> None:
        self.folder.mkdir(parents=True, exist_ok=True)
        (self.folder / SdkManifest.FILE).write_text(json.dumps(self.describe(), indent=4) + "\n", encoding="utf-8")

    def path(self, relative: str) -> Path:
        return self.folder / relative

    def file(self, name: str) -> Path:
        """Returns a shared file of the SDK by its name, and fails when the SDK lacks it."""
        if name not in self.files:
            raise IncompatibleSdk(f'The SDK `{Repository.shown(self.folder)}` has no file "{name}", so it is incomplete. Build it again with "haylen sdk build".')
        return self.folder / self.files[name]

    def describe(self) -> dict[str, object]:
        return {
            "format": SdkManifest.FORMAT,
            "port": self.port,
            "portVersion": self.port_version,
            "engineVersion": self.engine_version,
            "revision": self.revision,
            "configuration": self.configuration.value,
            "slices": [entry.describe() for entry in self.slices],
            "hostTools": [tool.describe() for tool in self.host_tools],
            "files": dict(self.files),
        }
