import json
from pathlib import Path

from haylen.core.configuration import Configuration
from haylen.core.errors.build_failed import BuildFailed
from haylen.core.repository import Repository
from haylen.core.resolved_product import ResolvedProduct
from haylen.core.target_environment import TargetEnvironment
from haylen.core.target_id import TargetId
from haylen.platforms.build_product import BuildProduct
from haylen.platforms.preparation_state import PreparationState
from haylen.platforms.prepared_project import PreparedProject
from haylen.platforms.product_component import ProductComponent
from haylen.platforms.product_folders import ProductFolders


class ProductRecord:
    """Keeps what the last build of a product made in `haylen-product.json` of its folder, so `app verify` and `app run` examine and launch that product without building it again."""

    FILE = "haylen-product.json"

    @staticmethod
    def save(product: BuildProduct) -> None:
        document = {**product.describe(), "project": str(product.prepared.project)}
        path = product.prepared.folders.root / ProductRecord.FILE
        path.write_text(json.dumps(document, indent=4) + "\n", encoding="utf-8")

    @staticmethod
    def load(resolved: ResolvedProduct, backend: str) -> BuildProduct:
        folders = ProductFolders.of(resolved.app.folder, resolved.identity)
        path = folders.root / ProductRecord.FILE
        if not path.is_file():
            raise BuildFailed(
                f'The app `{Repository.shown(resolved.app.folder)}` has no build for "{resolved.target_name}" in the "{resolved.configuration.value}" configuration yet. Build it with "haylen app build".'
            )
        outdated = BuildFailed(
            f'The last build of `{Repository.shown(resolved.app.folder)}` for "{resolved.target_name}" is damaged or an older version of Haylen made it. Build the app again with "haylen app build".'
        )
        try:
            document = json.loads(path.read_text(encoding="utf-8"))
            project = Path(document["project"])
            components = tuple(ProductComponent(ProductRecord.target(entry["target"]), Path(entry["path"]), entry["kind"]) for entry in document["components"])
            product = (Path(document["path"]), document["kind"], tuple(Path(entry) for entry in document["auxiliary"]))
        except json.JSONDecodeError, KeyError, TypeError:
            raise outdated from None
        state = PreparationState.read(project)
        if state is None:
            raise BuildFailed(f'The project `{Repository.shown(project)}` of the last build is gone. Build the app again with "haylen app build".')
        prepared = PreparedProject(resolved, folders, project, backend, state, False)
        path, kind, auxiliary = product
        return BuildProduct(prepared, path, kind, components, auxiliary)

    @staticmethod
    def target(entry: dict[str, str]) -> TargetId:
        return TargetId(
            entry["system"],
            entry["architecture"],
            entry["abi"],
            TargetEnvironment(entry["environment"]),
            Configuration(entry["configuration"]),
            entry["toolchain"],
        )
