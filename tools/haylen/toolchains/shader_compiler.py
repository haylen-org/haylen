import tempfile
from pathlib import Path

from haylen.core.errors.missing_toolchain import MissingToolchain
from haylen.core.repository import Repository
from haylen.core.tooling_context import ToolingContext
from haylen.platforms.tool_status import ToolStatus


class ShaderCompiler:
    """The Haylen shader compiler, a host tool: the build that `HAYLEN_SHADER_COMPILER` names, or the release that `engine/cmake/haylen-shader-compiler.json` pins, which the script of the engine downloads into `.tools` where the release has a binary for this machine and builds from its commit elsewhere."""

    VARIABLE = "HAYLEN_SHADER_COMPILER"
    SCRIPT = Repository.ENGINE / "cmake" / "haylen-shader-compiler.cmake"
    PIN = Repository.ENGINE / "cmake" / "haylen-shader-compiler.json"
    # The compiler that this command found, which the script of the engine looks up once per command.
    found: dict[str, Path] = {}

    @staticmethod
    def path(context: ToolingContext) -> Path:
        if "compiler" not in ShaderCompiler.found:
            ShaderCompiler.found["compiler"] = ShaderCompiler.locate(context)
        return ShaderCompiler.found["compiler"]

    @staticmethod
    def locate(context: ToolingContext) -> Path:
        named = context.environment.get(ShaderCompiler.VARIABLE)
        if named:
            compiler = Path(named)
            if not compiler.is_file():
                raise MissingToolchain(
                    f'The variable "{ShaderCompiler.VARIABLE}" names `{named}`, which is not a build of the Haylen shader compiler. Point it at the executable "haylen-shaderc" or unset it.'
                )
            return compiler.resolve()
        with tempfile.TemporaryDirectory() as folder:
            result = Path(folder) / "compiler"
            cache_folder = context.environment.get("CPM_SOURCE_CACHE", str(Repository.ROOT / ".cache" / "cpm"))
            context.process.run(
                [
                    "cmake",
                    f"-DHAYLEN_SHADERC_CACHE={Repository.TOOLS.as_posix()}",
                    f"-DHAYLEN_SHADERC_RESULT={result.as_posix()}",
                    f"-DCPM_SOURCE_CACHE={Path(cache_folder).as_posix()}",
                    "-P",
                    ShaderCompiler.SCRIPT,
                ]
            )
            return Path(result.read_text(encoding="utf-8"))

    @staticmethod
    def status() -> ToolStatus:
        """Tells whether the pinned compiler is in `.tools` without fetching it, for `doctor`."""
        found = sorted(Repository.TOOLS.glob("haylen-shaderc*/**/haylen-shaderc*"))
        executable = next((path for path in found if path.is_file()), None)
        problem = None if executable else 'The Haylen shader compiler is not in ".tools" yet. The first build that compiles shaders downloads the pinned release.'
        return ToolStatus("haylen-shaderc", ToolStatus.Role.HOST, None, executable, problem)
