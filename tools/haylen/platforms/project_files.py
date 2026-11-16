from pathlib import Path


class ProjectFiles:
    """Reads the files that a backend puts into a generated project by the paths they take there, such as the files of a template or the files of an app that the key `files` of a section of `platforms` names, with their permissions, so a backend lays out the whole project before it writes each file once."""

    IGNORED = frozenset({".DS_Store"})

    @staticmethod
    def of(source: Path, destination: str) -> dict[str, tuple[bytes, int | None]]:
        """Reads a file at `destination`, or every file of a folder under `destination`, which is empty for the root of the project."""
        if source.is_file():
            return {destination: (source.read_bytes(), source.stat().st_mode & 0o777)}
        found = {}
        for path in sorted(source.rglob("*")):
            if path.is_file() and path.name not in ProjectFiles.IGNORED:
                relative = path.relative_to(source).as_posix()
                found[f"{destination}/{relative}" if destination else relative] = (path.read_bytes(), path.stat().st_mode & 0o777)
        return found
