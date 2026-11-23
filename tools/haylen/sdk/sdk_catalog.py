import os
from collections.abc import Mapping, Sequence
from dataclasses import dataclass
from pathlib import Path

from haylen.core.errors.incompatible_sdk import IncompatibleSdk
from haylen.core.repository import Repository
from haylen.sdk.sdk_manifest import SdkManifest


class SdkCatalog:
    """The engine SDKs installed on this machine: the folders under `build/sdk` that `haylen sdk build` writes, and the folders that the variable `HAYLEN_SDK_PATH` lists, each with its `haylen-sdk.json`. An SDK whose manifest this tooling cannot use, such as one that an older version of Haylen built or a damaged one, holds no slice that a build takes, like an SDK of other engine sources, so no command stops on it and the commands that build build its targets again."""

    VARIABLE = "HAYLEN_SDK_PATH"

    @dataclass(frozen=True)
    class Unusable:
        """An installed SDK whose manifest this tooling cannot use: whether the file reads at all, the sentence that says what happened in plain words, the reasons of the schema for the log of the command, and the systems and environments that its slices name, which tell the targets it was built for."""

        folder: Path
        readable: bool
        problem: str
        reasons: tuple[str, ...]
        slices: tuple[tuple[str, str], ...]

        @property
        def state(self) -> str:
            return "Built by an older Haylen" if self.readable else "Unreadable"

        def sentence(self, targets: Sequence[str]) -> str:
            """What happened to the SDK in plain words, with the targets it was built for and the one command that builds it again."""
            happened = "was built by an older version of Haylen" if self.readable else f'cannot be read, since its file "{SdkManifest.FILE}" is damaged'
            names = ", ".join(f'"{target}"' for target in targets)
            subject = f"The SDK of {names} in `{Repository.shown(self.folder)}`" if targets else f"The SDK in `{Repository.shown(self.folder)}`"
            options = " ".join(f"--target {target}" for target in targets) or "--target <target>"
            return f'{subject} {happened}. Build it again with "haylen sdk build {options}".'

    @dataclass(frozen=True)
    class Survey:
        """Every installed SDK: the ones whose manifests this tooling uses and the ones it cannot use."""

        manifests: tuple[SdkManifest, ...]
        unusable: tuple[SdkCatalog.Unusable, ...]

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
        """The manifests of the installed SDKs that this tooling uses."""
        return list(self.survey().manifests)

    def survey(self) -> SdkCatalog.Survey:
        manifests = []
        unusable = []
        for folder in self.folders():
            try:
                document = SdkManifest.document(folder)
            except IncompatibleSdk as error:
                unusable.append(SdkCatalog.Unusable(folder.resolve(), False, error.message, (), ()))
                continue
            reasons = SdkManifest.problems(document)
            if reasons:
                unusable.append(SdkCatalog.Unusable(folder.resolve(), True, SdkManifest.older(folder), tuple(reasons), SdkCatalog.named_slices(document)))
                continue
            manifests.append(SdkManifest.parse(folder, document))
        return SdkCatalog.Survey(tuple(manifests), tuple(unusable))

    @staticmethod
    def named_slices(document: object) -> tuple[tuple[str, str], ...]:
        """The system and environment that each slice of a manifest names, which tell the targets of an SDK that this tooling cannot use otherwise."""
        slices = document.get("slices") if isinstance(document, dict) else None
        if not isinstance(slices, list):
            return ()
        named = [(entry.get("system"), entry.get("environment")) for entry in slices if isinstance(entry, dict)]
        return tuple((system, environment) for system, environment in named if isinstance(system, str) and isinstance(environment, str))
