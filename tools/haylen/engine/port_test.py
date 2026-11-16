from haylen.core.host import Host
from haylen.core.repository import Repository
from haylen.core.tooling_context import ToolingContext
from haylen.engine.engine_tree import EngineTree


class PortTest:
    """Builds the player of the engine with the test port of `engine/tests/data/port`, whose platform and GPU device come from outside the engine, in a tree of its own, and plays the app of the port on it. The app quits with a failure when the platform, the operating system or the graphics API is not the one of the port, or when a frame fails."""

    PORT = Repository.ENGINE / "tests" / "data" / "port"
    APP = Repository.ENGINE / "tests" / "data" / "port-app"
    PLAYER = "haylen"

    def __init__(self, context: ToolingContext) -> None:
        self.context = context

    def run(self, configuration: str) -> dict[str, object]:
        tree = EngineTree(self.context, f"{Host.system()}-port-{configuration.lower()}")
        options = [f"-DHAYLEN_PORT_DIR={PortTest.PORT.as_posix()}", "-DHAYLEN_BUILD_TESTS=OFF", "-DHAYLEN_BUILD_BENCHMARKS=OFF", "-DHAYLEN_BUILD_TOOLS=OFF"]
        tree.ensure(tree.command(configuration, *EngineTree.host_options(), *options))
        tree.build([PortTest.PLAYER], configuration)

        self.context.terminal.step("Playing the app of the test port")
        player = tree.folder / "bin" / PortTest.PLAYER / Host.executable(PortTest.PLAYER)
        self.context.process.run([player, PortTest.APP], stream=True)
        self.context.terminal.success("The test port played its app.")
        return {"tree": str(tree.folder), "app": str(PortTest.APP)}
