from haylen.core.errors.build_failed import BuildFailed
from haylen.core.folders import Folders
from haylen.core.repository import Repository
from haylen.core.tooling_context import ToolingContext
from haylen.engine.engine_tests import EngineTests
from haylen.engine.engine_tree import EngineTree
from haylen.toolchains.build_tools import BuildTools
from haylen.toolchains.llvm import Llvm


class Coverage:
    """Measures the code coverage of the engine with the source-based coverage of LLVM: it builds the engine tests instrumented in a tree of their own, runs them and writes a report to the terminal and as HTML. The report leaves out the dependencies, the tests and the platform folders that the tests of this machine cannot reach."""

    IGNORED = r"(_deps|\.cache|engine/tests|engine/src/platform/(apple|android|web|windows|linux)|generated)"

    def __init__(self, context: ToolingContext) -> None:
        self.context = context

    def run(self, sanitizers: str | None) -> dict[str, object]:
        tree = EngineTree(self.context, "coverage")
        options = [*EngineTree.host_options(sanitizers), "-DHAYLEN_ENABLE_COVERAGE=ON", "-DHAYLEN_BUILD_PLAYER=OFF"]
        tree.ensure(tree.command("Debug", *options))
        tree.build(list(EngineTests.TARGETS), "Debug")

        profiles = tree.folder / "coverage"
        Folders.remove(profiles)
        self.context.terminal.step("Running the engine tests")
        self.context.process.run([BuildTools.CTEST.require(), "--test-dir", tree.folder, "--output-on-failure", "--parallel", str(self.context.jobs)], stream=True)

        raw = sorted(profiles.glob("*.profraw"))
        if not raw:
            raise BuildFailed(f"The test run wrote no coverage profiles into `{Repository.shown(profiles)}`. Measure coverage with Clang, which the guide of the build describes.")
        self.context.terminal.step("Writing the coverage report")
        merged = profiles / "haylen.profdata"
        self.context.process.run([Llvm.tool("llvm-profdata", self.context), "merge", "-sparse", *raw, "-o", merged])

        # The report sums both test programs: the first one is the binary of the report and the other one joins it as an object.
        binaries = [tree.binary(name) for name in EngineTests.TARGETS]
        programs = [binaries[0], *[argument for binary in binaries[1:] for argument in ("-object", binary)]]
        coverage = Llvm.tool("llvm-cov", self.context)
        filters = [f"-instr-profile={merged}", f"-ignore-filename-regex={Coverage.IGNORED}"]
        self.context.process.run([coverage, "report", *programs, *filters], stream=True)
        html = profiles / "html"
        self.context.process.run([coverage, "show", *programs, *filters, "-format=html", f"-output-dir={html}"])
        self.context.terminal.success(f"The coverage report is in `{Repository.shown(html / 'index.html')}`.")
        return {"tree": str(tree.folder), "report": str(html / "index.html")}
