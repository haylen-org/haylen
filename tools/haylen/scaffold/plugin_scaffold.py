import re
from pathlib import Path

from haylen.core.errors.invalid_option import InvalidOption
from haylen.core.repository import Repository
from haylen.plugins.plugin_manifest import PluginManifest


class PluginScaffold:
    """Creates a plugin from `templates/plugin`, with its id in the names of its files, classes and modules, whose native part answers its method and sends its event on every platform from its first build."""

    TEMPLATE = Repository.TEMPLATES / "plugin"
    ID = re.compile(r"[a-z][a-z0-9]*(-[a-z0-9]+)*")
    IGNORED = frozenset({".DS_Store"})

    @staticmethod
    def tokens(identifier: str) -> dict[str, str]:
        words = identifier.split("-")
        return {
            "{{ID}}": identifier,
            "{{NAME}}": "".join(word.capitalize() for word in words),
            "{{TITLE}}": " ".join(word.capitalize() for word in words),
            "{{PACKAGE}}": "".join(words),
            "{{SYMBOL}}": "_".join(words),
            "{{MACRO}}": "_".join(words).upper(),
        }

    @staticmethod
    def substitute(text: str, tokens: dict[str, str]) -> str:
        for token, value in tokens.items():
            text = text.replace(token, value)
        return text

    @staticmethod
    def create(folder: Path, identifier: str) -> PluginManifest:
        if not PluginScaffold.ID.fullmatch(identifier):
            raise InvalidOption(f'The name "{identifier}" is not a plugin id. Plugin ids are "dash-case", such as "camera-scanner".')
        if identifier != folder.name:
            raise InvalidOption(f'A plugin folder is named after the id of its plugin, so the folder of "{identifier}" is named "{identifier}", not "{folder.name}".')
        if folder.exists() and any(folder.iterdir()):
            raise InvalidOption(f"The folder `{Repository.shown(folder)}` already exists and is not empty. Name a new folder or an empty one.")

        tokens = PluginScaffold.tokens(identifier)
        for source in sorted(PluginScaffold.TEMPLATE.rglob("*")):
            if not source.is_file() or source.name in PluginScaffold.IGNORED:
                continue
            target = folder / PluginScaffold.substitute(source.relative_to(PluginScaffold.TEMPLATE).as_posix(), tokens)
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(PluginScaffold.substitute(source.read_text(encoding="utf-8"), tokens), encoding="utf-8")
        return PluginManifest.load(folder)
