from dataclasses import dataclass
from pathlib import Path

from haylen.core.resolved_product import ResolvedProduct
from haylen.platforms.preparation_state import PreparationState
from haylen.platforms.product_folders import ProductFolders


@dataclass(frozen=True)
class PreparedProject:
    """A generated native project of a product: the folders of the product, the project the native tool opens, the backend that generated it, the record of its preparation and whether this preparation changed any file."""

    product: ResolvedProduct
    folders: ProductFolders
    project: Path
    backend: str
    state: PreparationState
    changed: bool

    def describe(self) -> dict[str, object]:
        return {
            "identity": self.product.identity,
            "backend": self.backend,
            "project": str(self.project),
            "changed": self.changed,
            "state": self.state.describe(),
        }
