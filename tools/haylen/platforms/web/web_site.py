import json
from pathlib import Path

from haylen.content.protected_release import ProtectedRelease
from haylen.core.tooling_context import ToolingContext
from haylen.platforms.generated_tree import GeneratedTree
from haylen.platforms.preparation_state import PreparationState
from haylen.platforms.web.web_project import WebProject
from haylen.toolchains.emscripten import Emscripten


class WebSite:
    """The site that a web build makes in the products of its folder, which a server serves as it is: the page files of the project, the runtime of the page with the processor of its audio output, the worker of its content store and the page side and the worker of the Worker mode, which a page whose address holds `hosting=worker` runs the app in, the protected release of a release in `app/`, and `config.json` with the size of every download, so the loader shows progress when the server sends no length. The scripts of the page of a release are minified, while the runtime that Emscripten linked is already."""

    RUNTIME = "haylen.wasm"
    # The files that a build of the runtime writes next to its script for the default page of C++ apps, which the site replaces with its own page.
    SHELL_FILES = frozenset({"haylen.html", "haylen.shell.html", "haylen-logo.svg"})
    MODULE_SUFFIXES = frozenset({".js", ".mjs"})

    def __init__(self, context: ToolingContext, project: Path, folder: Path) -> None:
        self.context = context
        self.project = project
        self.folder = folder
        self.tree = GeneratedTree(folder)

    def assemble(self, runtime: Path, release: ProtectedRelease | None, release_files: tuple[str, ...]) -> None:
        """Writes the site from the page files of the project, the files of the runtime in `runtime` and the part of a release that ships with the page, and deletes the files of an earlier build that it no longer holds."""
        for path in sorted(self.project.rglob("*")):
            relative = path.relative_to(self.project).as_posix()
            if path.is_file() and relative.split("/")[0] != WebProject.RUNTIME and relative not in (WebProject.CONFIG, PreparationState.FILE):
                self.tree.copy(relative, path)
        for path in sorted(runtime.iterdir()):
            if path.is_file() and path.name not in WebSite.SHELL_FILES:
                self.tree.copy(path.name, path)
        if release is not None:
            for path in sorted(release.shipped.iterdir()):
                self.tree.copy(f"app/{path.name}", path)

        config = json.loads((self.project / WebProject.CONFIG).read_text(encoding="utf-8"))
        downloads = [f"app/{name}" for name in release_files] if release is not None else [WebProject.ARCHIVE]
        config["sizes"] = {path: (self.folder / path).stat().st_size for path in (*downloads, WebSite.RUNTIME)}
        if release is not None:
            config["release"] = {"files": list(release_files)}
        self.tree.write_bytes(WebProject.CONFIG, (json.dumps(config, indent=4) + "\n").encode("utf-8"))
        self.tree.finish()

    def minify(self) -> None:
        """Minifies the page scripts of a release with the terser of the pinned Emscripten: the loader and the page scripts of the app keep their top-level names, which they share, and the web modules of the plugins minify as modules."""
        terser = Emscripten.ROOT / "upstream" / "emscripten" / "node_modules" / "terser" / "bin" / "terser"
        node = Emscripten.node()
        scripts = [self.folder / WebProject.LOADER, *sorted((self.folder / WebProject.SCRIPTS).rglob("*.js")), *sorted((self.folder / WebProject.PLUGINS).rglob("*"))]
        for script in scripts:
            if not script.is_file() or script.suffix not in WebSite.MODULE_SUFFIXES:
                continue
            module = ["--module"] if WebProject.PLUGINS in script.relative_to(self.folder).parts else []
            self.context.process.run([node, terser, script, "--compress", "--mangle", *module, "--output", script])
