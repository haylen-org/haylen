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
        """Reads the manifest of an SDK, and fails with a plain sentence when the file is damaged or this tooling cannot use it, such as one that an older version of Haylen wrote."""
        document = SdkManifest.document(folder)
        if not SdkManifest.matches(document):
            raise IncompatibleSdk(SdkManifest.older(folder), origin=Repository.shown(folder / SdkManifest.FILE))
        return SdkManifest.parse(folder, document)

    @staticmethod
    def usable(folder: Path) -> SdkManifest | None:
        """The manifest of the SDK in a folder, or nothing when the folder holds none or one that this tooling cannot use, whose files a new build of the SDK replaces."""
        if not (folder / SdkManifest.FILE).is_file():
            return None
        try:
            return SdkManifest.load(folder)
        except IncompatibleSdk:
            return None

    @staticmethod
    def document(folder: Path) -> object:
        """The JSON document of the manifest of an SDK, failing with a plain sentence when the file cannot be read."""
        path = folder / SdkManifest.FILE
        try:
            return json.loads(path.read_text(encoding="utf-8"))
        except OSError, json.JSONDecodeError:
            raise IncompatibleSdk(SdkManifest.unreadable(folder), origin=Repository.shown(path)) from None

    @staticmethod
    def matches(document: object) -> bool:
        """Tells whether a manifest has the layout that this tooling writes, which one of an older version of Haylen lacks."""
        return not SchemaValidator.of("sdk.schema.json").problems(document)

    @staticmethod
    def older(folder: Path) -> str:
        return f'The SDK in `{Repository.shown(folder)}` was built by an older version of Haylen, so this version cannot use it. Build it again with "haylen sdk build".'

    @staticmethod
    def unreadable(folder: Path) -> str:
        return f'The SDK in `{Repository.shown(folder)}` cannot be read, since its file "{SdkManifest.FILE}" is damaged. Build it again with "haylen sdk build".'

    @staticmethod
    def parse(folder: Path, document: dict) -> SdkManifest:
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
            raise IncompatibleSdk(f'The SDK in `{Repository.shown(self.folder)}` lacks a part that this version of Haylen needs, so it is incomplete. Build it again with "haylen sdk build".')
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
