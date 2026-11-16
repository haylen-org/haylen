from haylen.core.tooling_context import ToolingContext
from haylen.engine.engine_tree import EngineTree
from haylen.toolchains.llvm import Llvm


class Fuzzers:
    """Builds the fuzzers of the parsers that read hostile bytes with a Clang that links libFuzzer and runs each one for a while. Each fuzzer keeps the inputs that reached new code in a corpus of its own in the tree, which the next run starts from."""

    TARGETS = (
        "haylen-content-fuzzer",
        "haylen-input-fuzzer",
        "haylen-messagepack-fuzzer",
        "haylen-placeholder-fuzzer",
        "haylen-image-fuzzer",
        "haylen-vector-image-fuzzer",
        "haylen-ogg-fuzzer",
        "haylen-font-fuzzer",
        "haylen-bitmap-font-fuzzer",
    )
    MAXIMUM_LENGTH = 65536

    def __init__(self, context: ToolingContext) -> None:
        self.context = context

    def run(self, seconds: int, llvm: str | None) -> dict[str, object]:
        clang, clangxx = Llvm.fuzzing_compilers(llvm, self.context.environment)
        tree = EngineTree(self.context, "fuzz")
        # The compilers come from the command, so the tree configures on every run, which keeps it on the Clang that this run names.
        compilers = [f"-DCMAKE_C_COMPILER={clang}", f"-DCMAKE_CXX_COMPILER={clangxx}"]
        tree.configure(tree.command("Debug", "-G", "Ninja", *compilers, "-DHAYLEN_BUILD_FUZZERS=ON"))
        tree.build(Fuzzers.TARGETS, "Debug")

        for fuzzer in Fuzzers.TARGETS:
            corpus = tree.folder / "corpus" / fuzzer
            corpus.mkdir(parents=True, exist_ok=True)
            self.context.terminal.step(f'Running "{fuzzer}" for {seconds} seconds')
            self.context.process.run([tree.binary(fuzzer), corpus, f"-max_total_time={seconds}", f"-max_len={Fuzzers.MAXIMUM_LENGTH}", "-print_final_stats=1"], stream=True)
        self.context.terminal.success(f"The {len(Fuzzers.TARGETS)} fuzzers ran without a crash.")
        return {"tree": str(tree.folder), "fuzzers": list(Fuzzers.TARGETS), "seconds": seconds}
