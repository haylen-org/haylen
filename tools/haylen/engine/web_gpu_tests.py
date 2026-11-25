import json
from urllib.parse import urlencode

from haylen.core.errors.build_failed import BuildFailed
from haylen.core.repository import Repository
from haylen.core.tooling_context import ToolingContext
from haylen.engine.desktop_gpu_tests import DesktopGpuTests
from haylen.engine.engine_tree import EngineTree
from haylen.engine.headless_browser import HeadlessBrowser
from haylen.engine.served_site import ServedSite
from haylen.toolchains.emscripten import Emscripten


class WebGpuTests:
    """Builds the GPU tests for the browser in a tree of their own and runs them in a page of a headless browser, whose WebGPU and WebGL2 draw through SwiftShader on a machine without a GPU for them, on every device of the build or on the one that a graphics API names. The run fails when the tests fail or do not finish in time, and when the page stops or throws at any moment, even after the tests finished."""

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
        report = tree.folder / "gpu-test-report.json"
        report.unlink(missing_ok=True)
        with ServedSite(tree.folder / "bin") as site, HeadlessBrowser(self.context, HeadlessBrowser.SOFTWARE_GPU, "the GPU tests of the web") as debugging:
            self.context.terminal.step(f'Running the GPU tests of the web on "{api}"' if api else "Running the GPU tests of the web on every device of the build")
            page = site.page("?" + urlencode({"api": api or "", "output": output.as_posix()}), "gpu-tests.html")
            self.context.process.run([Emscripten.node(), WebGpuTests.DRIVER, str(debugging), page, str(WebGpuTests.SECONDS), output, report], stream=True)
        WebGpuTests.check(json.loads(report.read_text(encoding="utf-8")))
        self.context.terminal.success("The GPU tests of the web passed.")
        return {"tree": str(tree.folder), "graphicsApi": api, "output": str(output)}

    @staticmethod
    def check(report: dict) -> None:
        """Fails on the report of a page that stopped or threw, whether before or after the tests finished, or whose tests failed or did not finish in time."""
        failures = report["failures"]
        status = report["status"]
        if failures:
            raise BuildFailed(f'The page of the GPU tests of the web stopped with "{failures[0]}", and its output above shows where.')
        if status is None:
            raise BuildFailed(f"The GPU tests of the web did not finish within {WebGpuTests.SECONDS} seconds, and their output above shows the last test that ran.")
        if status != 0:
            raise BuildFailed(f"The GPU tests of the web failed with the status {status}, and their output above shows which tests failed.")
