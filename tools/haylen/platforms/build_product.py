from dataclasses import dataclass
from pathlib import Path

from haylen.platforms.prepared_project import PreparedProject
from haylen.platforms.product_component import ProductComponent


@dataclass(frozen=True)
class BuildProduct:
    """What a build made: the path of the product, its kind, such as `app`, `apk`, `executable` or `site`, the prepared project it came from, its components per target and the files that go with it, such as symbol files, which never ship. The verification examines these files, never only the exit status of the build."""

    prepared: PreparedProject
    path: Path
    kind: str
    components: tuple[ProductComponent, ...]
    auxiliary: tuple[Path, ...]

    def describe(self) -> dict[str, object]:
        return {
            "identity": self.prepared.product.identity,
            "path": str(self.path),
            "kind": self.kind,
            "components": [component.describe() for component in self.components],
            "auxiliary": [str(path) for path in self.auxiliary],
        }
