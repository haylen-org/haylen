import re
from collections.abc import Mapping
from pathlib import Path

from haylen.core.errors.build_failed import BuildFailed


class AndroidTemplate:
    """The template of the Android backend: the Gradle project under `project/`, the module of an asset pack and the CMake project of the native code. Its text files hold tokens such as `{{IDENTIFIER}}`, which a preparation replaces with values it writes in the syntax of the file, and quotes the values of each language itself, so no value of an app breaks a script."""

    FOLDER = Path(__file__).resolve().parent / "template"
    PROJECT = FOLDER / "project"
    ASSET_PACK = FOLDER / "asset-pack" / "build.gradle.kts"
    NATIVE = FOLDER / "native" / "CMakeLists.txt"
    TOKEN = re.compile(r"\{\{([A-Z0-9_]+)\}\}")

    @staticmethod
    def project_files() -> list[str]:
        """The files of the Gradle project of the template, relative to it."""
        return sorted(path.relative_to(AndroidTemplate.PROJECT).as_posix() for path in AndroidTemplate.PROJECT.rglob("*") if path.is_file() and path.name != ".DS_Store")

    @staticmethod
    def render(path: Path, tokens: Mapping[str, str]) -> str:
        """Returns a file of the template with its tokens replaced, and fails on a token that the preparation does not give, which only a template and its backend that disagree cause. A token alone on its line whose value is empty, such as a block that the app does not need, leaves the line out."""
        text = path.read_text(encoding="utf-8")
        unknown = sorted(set(AndroidTemplate.TOKEN.findall(text)) - set(tokens))
        if unknown:
            raise BuildFailed(f'The template file `{path}` holds the token "{unknown[0]}", which the Android backend does not give. Install a tooling whose template and backend match.')
        lines = []
        for line in text.splitlines(keepends=True):
            alone = AndroidTemplate.TOKEN.fullmatch(line.strip())
            if alone is not None and not tokens[alone[1]]:
                continue
            lines.append(AndroidTemplate.TOKEN.sub(lambda match: tokens[match[1]], line))
        return "".join(lines)

    @staticmethod
    def kotlin(value: str) -> str:
        """A string literal of Kotlin, which takes `$` as the start of a template."""
        escaped = value.replace("\\", "\\\\").replace('"', '\\"').replace("$", "\\$").replace("\n", "\\n")
        return f'"{escaped}"'

    @staticmethod
    def xml(value: str) -> str:
        """The value of an XML attribute as it is, which may name a resource, such as `@mipmap/icon`."""
        return value.replace("&", "&amp;").replace('"', "&quot;").replace("<", "&lt;").replace(">", "&gt;")

    @staticmethod
    def text(value: str) -> str:
        """A text in an XML attribute of Android, such as the name of the app, whose leading `@` or `?` would name a resource."""
        escaped = AndroidTemplate.xml(value)
        return f"\\{escaped}" if escaped.startswith(("@", "?")) else escaped
