from dataclasses import dataclass
from pathlib import Path

from haylen.core.target_id import TargetId


@dataclass(frozen=True)
class ProductComponent:
    """A part of a built product that serves one target, such as the native library of one Android ABI or the slice of one architecture, which the verification checks against its target."""

    target: TargetId
    path: Path
    kind: str

    def describe(self) -> dict[str, object]:
        return {"target": self.target.describe(), "path": str(self.path), "kind": self.kind}
