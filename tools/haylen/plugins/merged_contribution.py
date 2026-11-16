from collections.abc import Mapping
from dataclasses import dataclass
from pathlib import Path

from haylen.core.frozen import Frozen


@dataclass(frozen=True)
class MergedContribution:
    """The contributions of one type and identity for a target after their owners were merged: the fields with the parameters of each owner substituted, every owner with the origin of each and the folder its paths are relative to, so a backend writes one entry and a message names all of them."""

    type: str
    fields: Mapping[str, object]
    owners: tuple[str, ...]
    origins: tuple[str, ...]
    folders: tuple[Path, ...]

    def field(self, name: str, default: object = None) -> object:
        return self.fields.get(name, default)

    def describe(self) -> dict[str, object]:
        return {"type": self.type, "fields": Frozen.thaw(self.fields), "owners": list(self.owners), "origins": list(self.origins)}
