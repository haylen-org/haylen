import hashlib
import json
from pathlib import Path

from haylen.core.frozen import Frozen
from haylen.core.resolved_product import ResolvedProduct


class ProductInputs:
    """Hashes everything a preparation reads from the app and its plugins: the resolved product, the files of the package, the folders of the plugins, the C++ sources of the app and the files and the `afterPrepare` scripts that the sections of `platforms` name. Two preparations with the same digest, the same tooling, backend, template and SDK write the same project."""

    IGNORED = frozenset({".DS_Store", ".git", "build", ".gradle", ".cxx", ".kotlin"})

    @staticmethod
    def digest(product: ResolvedProduct) -> str:
        hashed = hashlib.sha256()
        hashed.update(json.dumps(product.describe(), sort_keys=True, default=str).encode())
        files = dict(product.package.files())
        for plugin in product.plugins:
            files |= ProductInputs.folder(f"plugin:{plugin.id}", plugin.folder)
        if product.app.cpp is not None:
            files |= {f"cpp:{path}": path for path in (*product.app.cpp.files(), *product.app.cpp.headers())}
        for section in product.primary.settings.values():
            for destination, source in Frozen.thaw(section.get("files", {})).items():
                files |= ProductInputs.folder(f"files:{destination}", product.app.folder / source)
            if "afterPrepare" in section:
                files |= ProductInputs.folder(f"afterPrepare:{section['afterPrepare']}", product.app.folder / str(section["afterPrepare"]))
        for library in product.primary.native_libraries:
            if library.cmake is not None:
                files |= ProductInputs.folder(f"native:{library.name}", library.cmake)
        for name in sorted(files):
            hashed.update(name.encode() + b"\0" + files[name].read_bytes())
        return hashed.hexdigest()

    @staticmethod
    def folder(prefix: str, root: Path) -> dict[str, Path]:
        if root.is_file():
            return {prefix: root}
        found = {}
        for path in sorted(root.rglob("*")):
            if path.is_file() and not ProductInputs.IGNORED.intersection(path.relative_to(root).parts):
                found[f"{prefix}/{path.relative_to(root).as_posix()}"] = path
        return found
