import hashlib
from pathlib import Path


class GeneratedTree:
    """Writes a generated project the same way for the same inputs: every file is written only when its content changes, so the native tools rebuild only what changed, and the files that a preparation no longer writes are deleted once it ends. A file whose content a backend copies keeps its bytes and its permissions."""

    def __init__(self, root: Path) -> None:
        self.root = root
        self.written: set[str] = set()
        self.changed: list[str] = []

    def write_bytes(self, relative: str, content: bytes, mode: int | None = None) -> Path:
        path = self.root / relative
        self.written.add(Path(relative).as_posix())
        if path.is_file() and path.read_bytes() == content and (mode is None or path.stat().st_mode & 0o777 == mode):
            return path
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)
        if mode is not None:
            path.chmod(mode)
        self.changed.append(Path(relative).as_posix())
        return path

    def copy(self, relative: str, source: Path) -> Path:
        return self.write_bytes(relative, source.read_bytes(), source.stat().st_mode & 0o777)

    def copy_folder(self, relative: str, source: Path, ignored: frozenset[str] = frozenset({".DS_Store"})) -> None:
        for path in sorted(source.rglob("*")):
            if path.is_file() and not ignored.intersection(path.relative_to(source).parts):
                self.copy(f"{relative}/{path.relative_to(source).as_posix()}", path)

    def finish(self, kept: frozenset[str] = frozenset()) -> None:
        """Deletes every file and empty folder of the tree that this preparation did not write, except the top folders that `kept` names, such as those the native tools fill."""
        if not self.root.is_dir():
            return
        for path in sorted(self.root.rglob("*"), reverse=True):
            relative = path.relative_to(self.root)
            if relative.parts[0] in kept:
                continue
            if path.is_dir() and not path.is_symlink():
                if not any(path.iterdir()):
                    path.rmdir()
            elif relative.as_posix() not in self.written:
                path.unlink()
                self.changed.append(relative.as_posix())

    def digest(self) -> str:
        """Hashes the paths and contents of every written file, which names the generated project."""
        hashed = hashlib.sha256()
        for relative in sorted(self.written):
            hashed.update(relative.encode() + b"\0" + (self.root / relative).read_bytes())
        return hashed.hexdigest()
