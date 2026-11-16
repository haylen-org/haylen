import shutil
from collections.abc import Mapping
from pathlib import Path

from haylen.core.errors.missing_toolchain import MissingToolchain
from haylen.core.host import Host
from haylen.core.tooling_context import ToolingContext
from haylen.platforms.tool_status import ToolStatus


class Llvm:
    """The LLVM tools of the fuzzers and of the coverage report: a Clang that links libFuzzer, which the Clang of Xcode does not ship, from the folder that `--llvm` or `HAYLEN_LLVM` names or from a package manager, and the profile tools, which Xcode ships on macOS and the path holds elsewhere."""

    VARIABLE = "HAYLEN_LLVM"
    PACKAGE_FOLDERS = ("/opt/homebrew/opt/llvm/bin", "/usr/local/opt/llvm/bin", "/usr/lib/llvm-21/bin", "/usr/lib/llvm-20/bin", "/usr/lib/llvm-19/bin", "/usr/lib/llvm-18/bin")
    INSTALL = 'Install the LLVM of a package manager, such as "brew install llvm" or "apt install clang libclang-rt-dev", or name its "bin" folder with "--llvm" or "HAYLEN_LLVM".'

    @staticmethod
    def folders(requested: str | None, environment: Mapping[str, str]) -> list[Path]:
        named = [Path(folder) for folder in (requested, environment.get(Llvm.VARIABLE)) if folder]
        return named or [Path(folder) for folder in Llvm.PACKAGE_FOLDERS]

    @staticmethod
    def fuzzing_compilers(requested: str | None, environment: Mapping[str, str]) -> tuple[Path, Path]:
        """The Clang and Clang++ that link libFuzzer."""
        for folder in Llvm.folders(requested, environment):
            clang = folder / Host.executable("clang")
            clangxx = folder / Host.executable("clang++")
            if clang.is_file() and clangxx.is_file():
                return clang, clangxx
        raise MissingToolchain(f"No Clang with libFuzzer was found. {Llvm.INSTALL}")

    @staticmethod
    def tool(name: str, context: ToolingContext) -> Path:
        """A profile tool, such as `llvm-profdata` or `llvm-cov`."""
        if Host.system() == "macos":
            return Path(context.process.capture(["xcrun", "--find", name]).strip())
        found = shutil.which(name)
        if found is None:
            raise MissingToolchain(f'The LLVM tool "{name}" was not found. Install LLVM and add its folder to "PATH" to measure coverage.')
        return Path(found)

    @staticmethod
    def status(environment: Mapping[str, str]) -> ToolStatus:
        try:
            _, clangxx = Llvm.fuzzing_compilers(None, environment)
        except MissingToolchain as error:
            return ToolStatus("Clang with libFuzzer", ToolStatus.Role.HOST, None, None, error.message)
        return ToolStatus("Clang with libFuzzer", ToolStatus.Role.HOST, None, clangxx, None)
