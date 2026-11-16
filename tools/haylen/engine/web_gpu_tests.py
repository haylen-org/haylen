from haylen.core.repository import Repository
from haylen.core.tooling_context import ToolingContext
from haylen.engine.desktop_gpu_tests import DesktopGpuTests
from haylen.engine.engine_tree import EngineTree
from haylen.engine.headless_browser import HeadlessBrowser
from haylen.engine.served_site import ServedSite
from haylen.toolchains.emscripten import Emscripten


class WebGpuTests:
    """Builds the GPU tests for the browser in a tree of their own and runs them in a page of a headless browser, whose WebGPU and WebGL2 draw through SwiftShader on a machine without a GPU for them, on every device of the build or on the one that a graphics API names."""

    DRIVER = Repository.ENGINE / "tests" / "web" / "gpu-tests.mjs"
    SECONDS = 1200

    def __init__(self, context: ToolingContext) -> None:
        self.context = context

    def run(self, configuration: str, api: str | None) -> dict[str, object]:
        tree = EngineTree(self.context, f"web-gpu-tests-{configuration.lower()}")
        if not tree.is_current() or Emscripten.is_stale(tree.folder):
            self.context.terminal.step(f"Configuring the GPU tests of the web in `{Repository.shown(tree.folder)}`")
            Emscripten.configure(self.context, tree.tree, tree.command(configuration, "-G", "Ninja", *EngineTree.ONLY_TESTS))
        tree.build([DesktopGpuTests.TARGET], configuration)

        output = tree.folder / "gpu-test-output"
        with ServedSite(tree.folder / "bin") as site, HeadlessBrowser(self.context, HeadlessBrowser.SOFTWARE_GPU, "the GPU tests of the web") as debugging:
            self.context.terminal.step(f'Running the GPU tests of the web on "{api}"' if api else "Running the GPU tests of the web on every device of the build")
            page = site.page(f"?api={api or ''}", "gpu-tests.html")
            self.context.process.run([Emscripten.node(), WebGpuTests.DRIVER, str(debugging), page, str(WebGpuTests.SECONDS), output], stream=True)
        self.context.terminal.success("The GPU tests of the web passed.")
        return {"tree": str(tree.folder), "graphicsApi": api, "output": str(output)}
