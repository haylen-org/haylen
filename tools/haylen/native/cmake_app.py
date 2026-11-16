from haylen.core.app_kind import AppKind
from haylen.core.app_manifest import AppManifest
from haylen.core.errors.invalid_manifest import InvalidManifest
from haylen.core.repository import Repository


class CMakeApp:
    """Writes the CMake text that builds the binary of an app with `haylen_add_app` in the projects that backends generate: the kind of the app, the C++ sources of a C++ app with its include folders and definitions, the content that the app ships and, for a release, its bootstrap. Values reach CMake as bracket arguments, which keep every character as it is."""

    # Objective-C compiles only on Apple platforms, whose projects take it from the section `apple`.
    OBJECTIVE_C = frozenset({".m", ".mm"})

    @staticmethod
    def text(value: str) -> str:
        """Writes a value as a bracket argument of CMake."""
        level = 0
        while f"]{'=' * level}]" in value:
            level += 1
        return f"[{'=' * level}[{value}]{'=' * level}]"

    @staticmethod
    def lines(app: AppManifest, target: str, content: str, bootstrap: str | None) -> list[str]:
        """The call of `haylen_add_app` for a target, with `content` and `bootstrap` as CMake expressions, and the include folders and definitions of a C++ app."""
        release = f" BOOTSTRAP {bootstrap}" if bootstrap is not None else ""
        if app.kind is AppKind.LUA:
            return [f"haylen_add_app({target} LUA CONTENT {content}{release})"]

        cpp = app.cpp
        files = cpp.files()
        objective = [file for file in files if file.suffix in CMakeApp.OBJECTIVE_C]
        if objective:
            names = ", ".join(f"`{Repository.shown(file)}`" for file in objective)
            raise InvalidManifest(
                f'The "cpp" section of `{app.origin}` names the Objective-C files {names}, which compile only on Apple platforms. Name them in "platforms.apple.sources".', origin=app.origin
            )
        sources = " ".join(CMakeApp.text(file.as_posix()) for file in files)
        lines = [f"haylen_add_app({target} CPP SOURCES {sources} CONTENT {content}{release})"]
        if cpp.include_directories:
            lines.append(f"target_include_directories({target} PRIVATE {' '.join(CMakeApp.text(path.as_posix()) for path in cpp.include_directories)})")
        if cpp.definitions:
            lines.append(f"target_compile_definitions({target} PRIVATE {' '.join(CMakeApp.text(definition) for definition in cpp.definitions)})")
        return lines
