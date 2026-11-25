import json
import re
import shutil
from pathlib import Path

from haylen.core.app_category import AppCategory
from haylen.core.app_kind import AppKind
from haylen.core.app_manifest import AppManifest
from haylen.core.errors.invalid_option import InvalidOption
from haylen.core.repository import Repository


class AppScaffold:
    """Creates a new app from the layers of `templates`: `common` with `haylen.json`, the logo, the bridge handlers of every platform under `native/` and the settings files of a repository, then `lua` or `cpp` with the code of its kind. The app holds no platform project, which every build generates from its backend."""

    COMMON = Repository.TEMPLATES / "common"
    KINDS = {AppKind.LUA: Repository.TEMPLATES / "lua", AppKind.CPP: Repository.TEMPLATES / "cpp"}
    KIND_WORDS = {AppKind.LUA: "Lua", AppKind.CPP: "C++"}
    # An identifier is also the application id of Android, whose parts start with a letter and hold letters, digits and underscores.
    IDENTIFIER = re.compile(r"[A-Za-z][A-Za-z0-9_]*(\.[A-Za-z][A-Za-z0-9_]*)+")
    # The Kotlin package of the application class of an app without an identifier, which the identifier of the app names otherwise.
    PACKAGE = "app"
    IGNORED = frozenset({".DS_Store"})

    @staticmethod
    def files(kind: AppKind) -> dict[str, Path]:
        """The files of the layers of a kind by their path in the app, where a file of the kind replaces a common file of the same path."""
        files: dict[str, Path] = {}
        for layer in (AppScaffold.COMMON, AppScaffold.KINDS[kind]):
            for path in sorted(layer.rglob("*")):
                if path.is_file() and path.name not in AppScaffold.IGNORED:
                    files[path.relative_to(layer).as_posix()] = path
        return files

    @staticmethod
    def create(folder: Path, kind: AppKind, category: AppCategory | None, name: str | None, identifier: str | None, orientation: str) -> AppManifest:
        if folder.exists() and any(folder.iterdir()):
            raise InvalidOption(f"The folder `{Repository.shown(folder)}` already exists and is not empty. Name a new folder or an empty one.")
        slug = re.sub(r"[^a-z0-9]+", "-", folder.name.lower()).strip("-")
        title = name or " ".join(word.capitalize() for word in slug.split("-")) or AppManifest.DEFAULT_NAME
        if identifier is not None and not AppScaffold.IDENTIFIER.fullmatch(identifier):
            raise InvalidOption(
                f'The identifier "{identifier}" is not a reverse domain identifier such as "com.example.game", whose parts start with a letter and hold letters, digits and underscores.'
            )
        package = identifier or AppScaffold.PACKAGE

        for relative, source in AppScaffold.files(kind).items():
            target = folder / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            if relative.endswith(".kt"):
                target.write_text(source.read_text(encoding="utf-8").replace("{{PACKAGE}}", package), encoding="utf-8")
            else:
                shutil.copy2(source, target)

        manifest_path = folder / AppManifest.FILE
        document = json.loads(manifest_path.read_text(encoding="utf-8"))
        document.update({"name": title, "orientation": orientation})
        # The identifier follows the name and the category follows the build number, where the template keeps the identity of the app.
        if identifier is not None:
            document = AppScaffold.insert(document, "name", "identifier", identifier)
        if category is not None:
            document = AppScaffold.insert(document, "build", "category", category.value)
        document["window"]["title"] = title
        document["platforms"]["android"]["applicationClass"] = f"{package}.App"
        if kind is AppKind.CPP:
            document = {**{key: value for key, value in document.items() if key != "platforms"}, "kind": kind.value, "cpp": {"sources": ["src"]}, "platforms": document["platforms"]}
        # The starter app is laid out for landscape, so a portrait app turns its window and design resolution upright.
        if orientation == "portrait":
            for section in (document["window"], document["design"]):
                section["width"], section["height"] = section["height"], section["width"]
        manifest_path.write_text(json.dumps(document, indent=4) + "\n", encoding="utf-8")
        return AppManifest.load(folder)

    @staticmethod
    def insert(document: dict, after: str, key: str, value: object) -> dict:
        entries = list(document.items())
        place = [name for name, _ in entries].index(after) + 1
        return dict([*entries[:place], (key, value), *entries[place:]])
