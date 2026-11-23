from haylen.core.tooling_context import ToolingContext
from haylen.engine.engine_tree import EngineTree
from haylen.engine.suite_shard import SuiteShard
from haylen.toolchains.build_tools import BuildTools


class EngineTests:
    """Builds the engine tests in the tree of this machine for a configuration and a sanitizer, the program of the tests of the core and the program of the tests of the Lua layer, and runs every test of both, or one shard of them, through CTest, which prints their results."""

    TARGETS = ("haylen-tests", "haylen-lua-tests")

    def __init__(self, context: ToolingContext) -> None:
        self.context = context

    def run(self, configuration: str, sanitizers: str | None, targets: list[str], shard: SuiteShard | None) -> dict[str, object]:
        tree = EngineTree(self.context, EngineTree.host_name(configuration, sanitizers))
        tree.ensure(tree.command(configuration, *EngineTree.host_options(sanitizers)))
        tree.build(targets, configuration)

        command: list[object] = [BuildTools.CTEST.require(), "--test-dir", tree.folder, "-C", configuration, "--output-on-failure", "--parallel", str(self.context.jobs)]
        if shard is None:
            self.context.terminal.step("Running the engine tests")
        else:
            self.context.terminal.step(f'Running the shard "{shard.describe()}" of the engine tests')
            command += shard.ctest_arguments()
        self.context.process.run(command, stream=True)
        self.context.terminal.success("The engine tests passed.")
        return {"tree": str(tree.folder), "configuration": configuration, "sanitizers": sanitizers, "shard": shard.describe() if shard else None}
