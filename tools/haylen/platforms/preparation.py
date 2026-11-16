import hashlib
from importlib import metadata
from pathlib import Path

from haylen.core.product_inputs import ProductInputs
from haylen.core.repository import Repository
from haylen.core.resolved_product import ResolvedProduct
from haylen.platforms.generated_tree import GeneratedTree
from haylen.platforms.preparation_state import PreparationState
from haylen.platforms.prepared_project import PreparedProject
from haylen.platforms.product_folders import ProductFolders


class Preparation:
    """One preparation of the generated project of a product by a backend: the record that its inputs give, whether the project on disk already matches that record, and the end of a preparation, which deletes the files it no longer writes and records the digest of the generated files. A project matches its record only while its files still have the digest that the record names, so a project that lost a file prepares again.

    ```python
    preparation = Preparation(product, folders, "web", "1.0.0", template)
    if preparation.is_current():
        return preparation.unchanged(project)
    tree = GeneratedTree(folders.project)
    ...
    return preparation.finish(tree, project)
    ```
    """

    def __init__(self, product: ResolvedProduct, folders: ProductFolders, backend: str, version: str, template: Path | None) -> None:
        """Takes the product, its folders, the backend with its version and the folder of the template that the backend generates from, or nothing for a backend that generates no files of its own."""
        self.product = product
        self.folders = folders
        self.backend = backend
        self.state = PreparationState(
            generator=Preparation.generator(),
            backend=backend,
            backend_version=version,
            template=Preparation.folder_digest(template) if template is not None else "",
            sdk=Preparation.sdk_identity(product),
            inputs=ProductInputs.digest(product),
            output="",
            extra={},
        )

    @staticmethod
    def generator() -> str:
        """Names the tooling that generates projects: the version of the engine, the version of the tooling package and a digest of its code and schemas, so a project that an older tooling wrote prepares again although no version changed."""
        hashed = hashlib.sha256()
        package = Path(__file__).resolve().parents[1]
        for path in sorted(path for suffix in ("*.py", "*.json") for path in package.rglob(suffix) if "__pycache__" not in path.parts):
            hashed.update(path.relative_to(package).as_posix().encode() + b"\0" + path.read_bytes())
        return f"{Repository.engine_version()}+{metadata.version('haylen')}+{hashed.hexdigest()[:16]}"

    @staticmethod
    def sdk_identity(product: ResolvedProduct) -> str:
        """Names the SDK of every target of a product by its folder and the revision of the engine it was built from."""
        return ";".join(f"{project.sdk.manifest.folder.as_posix()}@{project.sdk.manifest.revision}" for project in product.projects)

    @staticmethod
    def folder_digest(folder: Path) -> str:
        """Hashes the paths and contents of every file of a folder, such as a template, the same way `GeneratedTree.digest` hashes a generated project."""
        hashed = hashlib.sha256()
        files = sorted(
            (path for path in folder.rglob("*") if path.is_file() and path.name not in ProductInputs.IGNORED and path.name != PreparationState.FILE),
            key=lambda path: path.relative_to(folder).as_posix(),
        )
        for path in files:
            hashed.update(path.relative_to(folder).as_posix().encode() + b"\0" + path.read_bytes())
        return hashed.hexdigest()

    def is_current(self) -> bool:
        """Tells whether the project on disk came from the same inputs and still holds the files it was generated with."""
        recorded = PreparationState.read(self.folders.project)
        if not self.state.same_inputs(recorded):
            return False
        return Preparation.folder_digest(self.folders.project) == recorded.output

    def unchanged(self, project: Path) -> PreparedProject:
        recorded = PreparationState.read(self.folders.project)
        return PreparedProject(self.product, self.folders, project, self.backend, recorded, False)

    def finish(self, tree: GeneratedTree, project: Path) -> PreparedProject:
        """Deletes the files of the project that this preparation did not write, records the preparation next to the project and returns the prepared project."""
        tree.finish(frozenset({PreparationState.FILE}))
        state = PreparationState(
            generator=self.state.generator,
            backend=self.state.backend,
            backend_version=self.state.backend_version,
            template=self.state.template,
            sdk=self.state.sdk,
            inputs=self.state.inputs,
            output=tree.digest(),
            extra=self.state.extra,
        )
        recorded = PreparationState.read(self.folders.project)
        state.write(self.folders.project)
        changed = bool(tree.changed) or recorded != state
        return PreparedProject(self.product, self.folders, project, self.backend, state, changed)
