import hashlib
import os
from pathlib import Path


class Repository:
    """The folders of the Haylen checkout that the tooling runs from: the engine, the templates, the build folder and the tools that the tooling downloads."""

    ROOT = Path(__file__).resolve().parents[3]
    ENGINE = ROOT / "engine"
    TEMPLATES = ROOT / "templates"
    BUILD = ROOT / "build"
    TOOLS = ROOT / ".tools"
    LOGS = BUILD / "logs"
    APPS = BUILD / "apps"
    SDKS = BUILD / "sdk"
    ENGINE_BUILDS = BUILD / "engine"
    PLUGIN_CHECKOUTS = BUILD / "plugin-repositories"
    # Engine files that never reach an SDK, so editing them keeps the SDKs current.
    REVISION_SKIPPED = frozenset({"tests", "bench", "build", ".cxx", ".gradle", ".kotlin", ".DS_Store"})

    @staticmethod
    def engine_version() -> str:
        return (Repository.ENGINE / "VERSION").read_text(encoding="utf-8").strip()

    @staticmethod
    def engine_revision() -> str:
        """Hashes every engine file that reaches an SDK, which names the sources an SDK was built from."""
        digest = hashlib.sha256()
        for folder, subfolders, files in os.walk(Repository.ENGINE):
            subfolders[:] = sorted(name for name in subfolders if name not in Repository.REVISION_SKIPPED)
            for name in sorted(files):
                if name in Repository.REVISION_SKIPPED:
                    continue
                path = Path(folder) / name
                digest.update(path.relative_to(Repository.ENGINE).as_posix().encode())
                digest.update(b"\0")
                digest.update(path.read_bytes())
        return digest.hexdigest()

    @staticmethod
    def shown(path: Path) -> str:
        """Shows a path relative to the current folder when it lies inside it, and absolute otherwise."""
        resolved = path.resolve()
        try:
            return resolved.relative_to(Path.cwd().resolve()).as_posix() or "."
        except ValueError:
            return resolved.as_posix()
