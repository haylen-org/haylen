import contextlib
import json
import re
import subprocess
from pathlib import Path

from haylen.core.errors.build_failed import BuildFailed
from haylen.core.host import Host
from haylen.core.repository import Repository
from haylen.core.tooling_context import ToolingContext
from haylen.sdk.selected_sdk import SelectedSdk
from haylen.toolchains.shader_compiler import ShaderCompiler


class AppShaders:
    """Compiles the shaders under `content/shaders` of an app into `.shader` packages with every program the engine draws with, for every shader language of the engine, whenever a package is older than its source, the sources it may include, the shader library or the pin of the compiler. The compiler runs on the host, never on the target. When the SDK of the target is the SDK of a port with a shader tool, the host tool `port-shaders` of this machine, the tool runs on every package after it compiles and writes the code of the device of the port into its `port` target, and a package without that target compiles again."""

    LIBRARY = Repository.ENGINE / "shaders" / "include"
    # Every material compiles once per kind of draw and once more for lit canvases, and the engine picks the program of a draw by these names.
    PROGRAMS = {
        "sprite": [],
        "sprite_lit": ["HAYLEN_LIT"],
        "batch": ["HAYLEN_BATCH"],
        "batch_lit": ["HAYLEN_BATCH", "HAYLEN_LIT"],
        "text": ["HAYLEN_TEXT"],
        "text_lit": ["HAYLEN_TEXT", "HAYLEN_LIT"],
        "mesh": ["HAYLEN_MESH"],
        "mesh_lit": ["HAYLEN_MESH", "HAYLEN_LIT"],
        "shape": ["HAYLEN_SHAPE"],
        "shape_lit": ["HAYLEN_SHAPE", "HAYLEN_LIT"],
    }
    PROGRAM = re.compile(r"^\s*@program\s+(\w+)\s+(\w+)\s+(\w+)", re.MULTILINE)
    # The name of the shader tool among the host tools of the SDK of a port, whatever the suffix of its file, and the target of shader packages that it writes.
    PORT_TOOL = "port-shaders"
    PORT_TARGET = "port"

    def __init__(self, context: ToolingContext, sdk: SelectedSdk | None = None) -> None:
        self.context = context
        self.sdk = sdk

    @staticmethod
    def sources(app: Path) -> list[Path]:
        """Lists the sources that declare a program, which compile, while the other sources are files they include."""
        sources = []
        for path in sorted((app / "content" / "shaders").rglob("*.glsl")):
            with contextlib.suppress(FileNotFoundError):
                if re.search(r"^\s*@program\b", path.read_text(encoding="utf-8"), re.MULTILINE):
                    sources.append(path)
        return sources

    @staticmethod
    def newest_input(app: Path) -> float:
        newest = ShaderCompiler.PIN.stat().st_mtime
        for path in [*(app / "content" / "shaders").rglob("*.glsl"), *AppShaders.LIBRARY.rglob("*.glsl")]:
            with contextlib.suppress(FileNotFoundError):
                newest = max(newest, path.stat().st_mtime)
        return newest

    def port_tool(self) -> Path | None:
        """The shader tool of the SDK of the target for this machine, or nothing when the SDK is no port SDK with one."""
        if self.sdk is None:
            return None
        for tool in self.sdk.manifest.host_tools:
            if Path(tool.name).stem == AppShaders.PORT_TOOL and tool.system == Host.system() and tool.architecture == Host.architecture():
                return self.sdk.manifest.path(tool.path)
        return None

    @staticmethod
    def has_port_target(package: Path) -> bool:
        """Tells whether every program of a shader package holds the target that the shader tool of a port writes."""
        try:
            programs = json.loads(package.read_text(encoding="utf-8"))["programs"]
        except OSError, ValueError, KeyError:
            return False
        return all(AppShaders.PORT_TARGET in program.get("targets", {}) for program in programs.values())

    def update(self, app: Path, force: bool = False) -> list[Path]:
        """Compiles the shaders of an app that are out of date, or every one with `force`, and returns the compiled sources."""
        if not (app / "content" / "shaders").is_dir():
            return []
        newest = AppShaders.newest_input(app)
        port_tool = self.port_tool()
        compiled = []
        for source in AppShaders.sources(app):
            output = source.with_suffix(".shader")
            current = output.is_file() and output.stat().st_mtime >= newest
            if port_tool is not None and current:
                current = AppShaders.has_port_target(output)
            if force or not current:
                self.compile(source, output, port_tool)
                compiled.append(source)
        return compiled

    def compile(self, source: Path, output: Path, port_tool: Path | None) -> None:
        try:
            text = source.read_text(encoding="utf-8")
        except FileNotFoundError:
            return
        declared = AppShaders.PROGRAM.findall(text)
        if len(declared) != 1 or declared[0][1] != "haylen_vs":
            raise BuildFailed(f'The shader `{Repository.shown(source)}` must declare exactly one "@program" whose vertex shader is "haylen_vs" from "haylen/material.glsl".')
        variants = [part for program, defines in AppShaders.PROGRAMS.items() for part in ("--variant", f"{program}={','.join(defines)}")]
        command = [str(ShaderCompiler.path(self.context)), str(source.resolve()), "--output", str(output.resolve()), "--include", str(AppShaders.LIBRARY), *variants]
        compiled = subprocess.run(command, capture_output=True, text=True, encoding="utf-8", errors="replace", check=False)
        if compiled.returncode != 0:
            raise BuildFailed(f"The shader `{Repository.shown(source)}` does not compile, and the Haylen shader compiler names the lines below.", details=f"{compiled.stdout}{compiled.stderr}".strip())
        if port_tool is not None:
            self.context.process.run(AppShaders.port_command(port_tool, output.resolve()))
        self.context.terminal.success(f"Compiled `{Repository.shown(source)}` into `{Repository.shown(output)}`.")

    @staticmethod
    def port_command(tool: Path, package: Path) -> list[object]:
        """The command that runs the shader tool of a port on a package, with the path of the package after its last argument, as the build of the engine runs it: a CMake script through `cmake -P`, and any other tool as it is."""
        if tool.suffix == ".cmake":
            return ["cmake", "-P", tool, package]
        return [tool, package]
