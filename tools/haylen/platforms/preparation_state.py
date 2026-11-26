import json
from collections.abc import Mapping
from dataclasses import dataclass
from pathlib import Path
from types import MappingProxyType

from haylen.platforms.product_folders import ProductFolders


@dataclass(frozen=True)
class PreparationState:
    """What a preparation records in `haylen-preparation.json` of the generated project of a product, `project/` of its folders, whatever folder the native tool opens: the version of the tooling that generated it, the backend and its version, the digest of its template, the identity of the SDK, the digest of the inputs and the digest of the generated files. A preparation whose record matches its inputs again writes nothing."""

    FILE = "haylen-preparation.json"

    generator: str
    backend: str
    backend_version: str
    template: str
    sdk: str
    inputs: str
    output: str
    extra: Mapping[str, str]

    @staticmethod
    def read(folders: ProductFolders) -> PreparationState | None:
        """The record of the last preparation of the project of a product, or nothing when there is none or when it is damaged or an older version of Haylen wrote it, so the next preparation writes the project again."""
        path = PreparationState.path(folders)
        if not path.is_file():
            return None
        try:
            document = json.loads(path.read_text(encoding="utf-8"))
            return PreparationState.parse(document)
        except json.JSONDecodeError, KeyError, TypeError:
            return None

    @staticmethod
    def parse(document: dict) -> PreparationState:
        return PreparationState(
            generator=document["generator"],
            backend=document["backend"],
            backend_version=document["backendVersion"],
            template=document["template"],
            sdk=document["sdk"],
            inputs=document["inputs"],
            output=document["output"],
            extra=MappingProxyType(document.get("extra", {})),
        )

    def describe(self) -> dict[str, object]:
        return {
            "generator": self.generator,
            "backend": self.backend,
            "backendVersion": self.backend_version,
            "template": self.template,
            "sdk": self.sdk,
            "inputs": self.inputs,
            "output": self.output,
            "extra": dict(self.extra),
        }

    @staticmethod
    def path(folders: ProductFolders) -> Path:
        return folders.project / PreparationState.FILE

    def write(self, folders: ProductFolders) -> None:
        path = PreparationState.path(folders)
        text = json.dumps(self.describe(), indent=4, sort_keys=True) + "\n"
        if not path.is_file() or path.read_text(encoding="utf-8") != text:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text, encoding="utf-8")

    def same_inputs(self, other: PreparationState | None) -> bool:
        """Tells whether another record names the same generator, backend, template, SDK and inputs."""
        if other is None:
            return False
        return (self.generator, self.backend, self.backend_version, self.template, self.sdk, self.inputs) == (
            other.generator,
            other.backend,
            other.backend_version,
            other.template,
            other.sdk,
            other.inputs,
        )
