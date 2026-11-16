from haylen.core.tooling_context import ToolingContext
from haylen.engine.engine_tree import EngineTree
from haylen.toolchains.build_tools import BuildTools


class EngineTests:
    """Builds the engine tests in the tree of this machine for a configuration and a sanitizer, and runs them through CTest, which prints their results."""

    TARGET = "haylen-tests"

    def __init__(self, context: ToolingContext) -> None:
        self.context = context

    def run(self, configuration: str, sanitizers: str | None, target: str) -> dict[str, object]:
        tree = EngineTree(self.context, EngineTree.host_name(configuration, sanitizers))
        tree.ensure(tree.command(configuration, *EngineTree.host_options(sanitizers)))
        tree.build([target], configuration)

        self.context.terminal.step("Running the engine tests")
        command = [BuildTools.CTEST.require(), "--test-dir", tree.folder, "-C", configuration, "--output-on-failure", "--parallel", str(self.context.jobs)]
        self.context.process.run(command, stream=True)
        self.context.terminal.success("The engine tests passed.")
        return {"tree": str(tree.folder), "configuration": configuration, "sanitizers": sanitizers}
