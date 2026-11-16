import os
from collections.abc import Mapping
from pathlib import Path

from haylen.core.errors.incompatible_sdk import IncompatibleSdk
from haylen.core.repository import Repository
from haylen.sdk.sdk_manifest import SdkManifest


class SdkCatalog:
    """The engine SDKs installed on this machine: the folders under `build/sdk` that `haylen sdk build` writes, and the folders that the variable `HAYLEN_SDK_PATH` lists, each with its `haylen-sdk.json`."""

    VARIABLE = "HAYLEN_SDK_PATH"

    def __init__(self, environment: Mapping[str, str]) -> None:
        self.environment = environment

    def folders(self) -> list[Path]:
        roots = [Repository.SDKS] if Repository.SDKS.is_dir() else []
        folders = [folder for root in roots for folder in sorted(root.iterdir()) if (folder / SdkManifest.FILE).is_file()]
        for entry in self.environment.get(SdkCatalog.VARIABLE, "").split(os.pathsep):
            if not entry:
                continue
            folder = Path(entry).expanduser().resolve()
            if not (folder / SdkManifest.FILE).is_file():
                raise IncompatibleSdk(f'The folder `{folder}` of "{SdkCatalog.VARIABLE}" holds no "{SdkManifest.FILE}". Name the folder of an installed SDK.')
            folders.append(folder)
        return folders

    def manifests(self) -> list[SdkManifest]:
        return [SdkManifest.load(folder) for folder in self.folders()]
