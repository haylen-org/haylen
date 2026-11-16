import re
import sys
from pathlib import Path

from haylen.core.errors.build_failed import BuildFailed
from haylen.core.repository import Repository
from haylen.core.tooling_context import ToolingContext
from haylen.toolchains.build_tools import BuildTools


class SourceFormat:
    """Formats the sources of the repository, or checks them with `check`: clang-format on the C, C++, Objective-C and Objective-C++ files of the engine, the samples, the app scaffolds and the templates of the backends, the rule that multi-line lambdas stand between `clang-format off` and `on` markers, since clang-format cannot lay them out, and Ruff on the Python of `tools`."""

    EXTENSIONS = frozenset({".h", ".hpp", ".c", ".cpp", ".m", ".mm"})
    ROOTS = ("engine/include", "engine/src", "engine/tests", "engine/tools", "samples", "templates", "tools/haylen")
    # Build outputs inside those roots, such as the native tree that Gradle keeps in each Android project, hold generated and third-party code.
    SKIPPED = frozenset({".cxx", ".gradle", "build", "_deps"})
    PYTHON = Repository.ROOT / "tools"
    # The files go to clang-format in groups, which keeps every command line within the limits of the system.
    GROUP = 200
    # The opening line of a lambda whose body goes on below, which the brackets of the array forms of "operator new" and "operator delete" never are.
    LAMBDA_START = re.compile(r"(?<!operator new\[)(?<!operator delete\[)\]\s*(\([^)]*\))?\s*(mutable\s*)?(noexcept\s*)?(->\s*[\w:<>]+\s*)?\{\s*$")

    def __init__(self, context: ToolingContext) -> None:
        self.context = context

    @staticmethod
    def sources() -> list[Path]:
        """The C-family sources to format, without the files of the plugin template whose names hold a token such as `{{NAME}}`, which become code only once "haylen plugin new" replaces their tokens."""
        files: list[Path] = []
        for root in SourceFormat.ROOTS:
            folder = Repository.ROOT / root
            if not folder.exists():
                continue
            for path in folder.rglob("*"):
                if path.suffix in SourceFormat.EXTENSIONS and path.is_file() and "{{" not in path.name and not SourceFormat.SKIPPED.intersection(path.relative_to(folder).parts):
                    files.append(path)
        return sorted(files)

    @staticmethod
    def unguarded_lambdas(files: list[Path]) -> list[str]:
        """Lists the multi-line lambdas outside `clang-format off` regions."""
        findings: list[str] = []
        for path in files:
            guarded = False
            for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
                stripped = line.strip()
                if stripped == "// clang-format off":
                    guarded = True
                elif stripped == "// clang-format on":
                    guarded = False
                elif not guarded and SourceFormat.LAMBDA_START.search(stripped) and not stripped.startswith(("//", "[[", "#")):
                    findings.append(f'The file `{Repository.shown(path)}:{number}` has a multi-line lambda outside "clang-format off".')
        return findings

    def run(self, check: bool) -> dict[str, object]:
        files = SourceFormat.sources()
        clang_failed = self.clang_format(files, check)
        findings = SourceFormat.unguarded_lambdas(files)
        for finding in findings:
            self.context.terminal.warning(finding)
        python_failed = self.ruff(check)

        if clang_failed and check:
            raise BuildFailed('Some C-family files are not formatted, as the output of "clang-format" above shows. Format them with "haylen engine format".')
        if clang_failed:
            raise BuildFailed('The command "clang-format" could not format some files, and its output above shows why.')
        if findings and check:
            raise BuildFailed(f'The {len(findings)} multi-line lambdas above need "// clang-format off" and "// clang-format on" markers around them.')
        if python_failed:
            verb = "check" if check else "fix"
            raise BuildFailed(f'Ruff found problems in the Python of `tools` that it cannot {verb}, as its output above shows. Format with "haylen engine format" and fix what stays.')
        self.context.terminal.success(f"The {len(files)} C-family files and the Python of `tools` are formatted." if check else f"Formatted {len(files)} C-family files and the Python of `tools`.")
        return {"files": len(files), "checked": check, "lambdas": findings}

    def clang_format(self, files: list[Path], check: bool) -> bool:
        """Runs clang-format on the files and tells whether it failed on any of them."""
        command = [BuildTools.CLANG_FORMAT.require(), "--style=file", *(["--dry-run", "--Werror"] if check else ["-i"])]
        self.context.terminal.step(f'{"Checking the format of" if check else "Formatting"} {len(files)} files with "clang-format"')
        failed = False
        for start in range(0, len(files), SourceFormat.GROUP):
            try:
                self.context.process.run([*command, *files[start : start + SourceFormat.GROUP]], stream=True)
            except BuildFailed:
                failed = True
        return failed

    def ruff(self, check: bool) -> bool:
        """Runs Ruff on the Python of `tools` and tells whether it found problems. Ruff comes with the development group of the tooling, so it runs with the Python of the tooling."""
        ruff = [sys.executable, "-m", "ruff"]
        steps = [["format", "--check"], ["check"]] if check else [["check", "--fix"], ["format"]]
        self.context.terminal.step(f'{"Checking" if check else "Formatting"} the Python of `{Repository.shown(SourceFormat.PYTHON)}` with "ruff"')
        failed = False
        for step in steps:
            try:
                self.context.process.run([*ruff, *step, SourceFormat.PYTHON], stream=True)
            except BuildFailed:
                failed = True
        return failed
