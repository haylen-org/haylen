from haylen.core.repository import Repository
from haylen.core.tooling_context import ToolingContext
from haylen.engine.engine_tree import EngineTree


class DesktopGpuTests:
    """Builds and runs the GPU tests on this machine: the conformance suite of the GPU devices and the images of the 2D renderer, on every device that the tree compiled and that starts here, or on the one that a graphics API names."""

    TARGET = "haylen-gpu-tests"
    # The variable through which the GPU tests take the one device to run on.
    API_VARIABLE = "HAYLEN_TEST_GRAPHICS_API"

    def __init__(self, context: ToolingContext) -> None:
        self.context = context

    def run(self, configuration: str, sanitizers: str | None, api: str | None) -> dict[str, object]:
        tree = EngineTree(self.context, EngineTree.host_name(configuration, sanitizers))
        tree.ensure(tree.command(configuration, *EngineTree.host_options(sanitizers)))
        tree.build([DesktopGpuTests.TARGET], configuration)

        env = dict(self.context.environment)
        if api:
            env[DesktopGpuTests.API_VARIABLE] = api
        self.context.terminal.step(f'Running the GPU tests on "{api}"' if api else "Running the GPU tests on every device of the build")
        self.context.process.run([tree.binary(DesktopGpuTests.TARGET)], cwd=Repository.ENGINE / "tests", env=env, stream=True)
        self.context.terminal.success("The GPU tests passed.")
        return {"tree": str(tree.folder), "graphicsApi": api}
