import json
import os
import shutil
import tempfile
from pathlib import Path

from haylen.cli.report_printer import ReportPrinter
from haylen.core.app_pipeline import AppPipeline
from haylen.core.configuration import Configuration
from haylen.core.errors.build_failed import BuildFailed
from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.core.folders import Folders
from haylen.core.product_request import ProductRequest
from haylen.core.repository import Repository
from haylen.core.target_id import TargetId
from haylen.core.tooling_context import ToolingContext
from haylen.engine.headless_browser import HeadlessBrowser
from haylen.engine.served_site import ServedSite
from haylen.engine.suite_shard import SuiteShard
from haylen.sdk.sdk_catalog import SdkCatalog
from haylen.sdk.sdk_selector import SdkSelector
from haylen.toolchains.emscripten import Emscripten


class WebChecks:
    """The checks of the web: apps of `engine/tests/data` that the web backend builds in Release, whose pages a headless browser opens while a driver of `engine/tests/web` follows the steps that each page reports. The error screen check runs on WebGPU and on WebGL2, or on the API that `api` names, and the others draw through the first API that starts, or through the one that `api` names."""

    TARGET = "web"
    DATA = Repository.ENGINE / "tests" / "data"
    DRIVERS = Repository.ENGINE / "tests" / "web"
    GRAPHICS_APIS = ("webGpu", "webGl2")
    # The error screen check: an app that stops with an error at a key, whose page opens in a browser that lets the page write the clipboard, one that refuses it and one without its API, on each graphics API, since a restart makes a new GPU device.
    ERROR_SCREEN_STEPS = {
        "granted": ["ready", "drew on after a canvas of the page", "stopped", "copied", "restarted by the key", "stopped again", "restarted by a click"],
        "refused": ["ready", "stopped", "refused", "restarted by the key"],
        "missing": ["ready", "stopped", "refused", "restarted by the key"],
    }
    ERROR_SCREEN_BROWSERS = {"granted": "lets the page write the clipboard", "refused": "refuses the clipboard", "missing": "has no clipboard API"}
    # A page that names a graphics API that the engine does not know shows the reason instead of waiting for an app that never starts.
    UNKNOWN_API_STEPS = ["reported", "shown"]
    # The wake lock check: an app that keeps the screen on from its start and lets it sleep at a key, in a browser that grants the wake lock, one that refuses it and one without its API, which only warns while the app runs on.
    WAKE_LOCK_STEPS = {
        "granted": ["ready", "taken", "released when hidden", "taken when shown", "released when the app lets the screen sleep"],
        "refused": ["ready", "warned", "runs on"],
        "missing": ["ready", "warned", "runs on"],
    }
    WAKE_LOCK_BROWSERS = {"granted": "grants the wake lock", "refused": "refuses the wake lock", "missing": "has no Screen Wake Lock API"}
    # The fullscreen check: each promise settles once the window reports its mode, and a change that a newer one replaces fails with the code `denied`.
    FULLSCREEN_STEPS = ["ready", "entered", "heard", "left", "replaced", "replacing", "ends windowed"]
    # The orientation check: a phone held in portrait shows the turn screen of the page over an app that takes landscape while the app runs on, the screen goes away once the phone turns, and the lock of the app replaces the orientation of `haylen.json`.
    ORIENTATION_STEPS = ["ready", "shown in portrait", "runs under it", "hidden in landscape", "locked portrait", "shown for the lock", "turns freely", "hidden for any"]
    # The plugin check: a page whose permissions policy governs one feature that its plugin requires, with an error of the page, a request without a prompt and a permission that the plugin never registered.
    PLUGIN_STEPS = [
        "checked",
        "lacks only the governed feature",
        "fails an error of the page as an exception",
        "fails a result that JSON cannot carry as an exception",
        "fails a use without a prompt as denied",
        "fails an unregistered permission as unsupported",
        "kept the newest events before the app started",
    ]
    # The editor check: a page of the web player of the web SDK in development whose editor builds a project file by file, runs it, reloads, rejects and restores a module, applies a failing batch atomically, keeps the budget of the project, stops code that never ends through the execution budget, inspects the app, runs it again in place while its audio output opens, stops it early and disposes of the runtime, without an error of the page.
    EDITOR_PAGE = "editor-check.html"
    EDITOR_STEPS = [
        "ready",
        "ran",
        "reloaded",
        "rejected a syntax error",
        "restored",
        "applied atomically",
        "kept the project budget",
        "stopped endless code",
        "inspected",
        "ran again in place",
        "stopped early",
        "disposed",
    ]
    # The same page in the Worker mode, whose runtime runs in a dedicated worker that the page reaches for audio, input, gamepads, the mirror of screen readers, plugins and videos, and whose editor terminates a worker that stopped answering or that has not started.
    WORKER_HOSTING = "hosting=worker"
    WORKER_STEPS = [
        "ready",
        "refused an audio output",
        "prepared input",
        "took input",
        "read a gamepad",
        "mirrored the interface",
        "answered through a plugin",
        "played a video",
        "ran",
        "reloaded",
        "rejected a syntax error",
        "restored",
        "applied atomically",
        "kept the project budget",
        "stopped endless code",
        "inspected",
        "ran again in place",
        "stopped early",
        "disposed",
        "ready again",
        "terminated a stuck worker",
        "terminated before the worker started",
    ]
    # The memory check: a protected release with a medium and a large file that download on demand, while the memory of WebAssembly and the array buffers of the page stay far below the size of the files. A page without the files of the browser, or whose worker of the content store does not load, keeps the medium file in memory and refuses the large one for want of room.
    MEMORY_FILE_SIZE = 384 * 1024**2
    MEMORY_MEDIUM_SIZE = 16 * 1024**2
    MEMORY_WASM_LIMIT = MEMORY_FILE_SIZE // 2
    MEMORY_BUFFER_LIMIT = 64 * 1024**2
    MEMORY_SECONDS = 600

    def __init__(self, context: ToolingContext, pipeline: AppPipeline) -> None:
        self.context = context
        self.pipeline = pipeline

    @staticmethod
    def query(api: str | None, *parameters: str) -> str:
        """The query of the address of a page, with the graphics API that `api` names and the other parameters, such as `hosting=worker`."""
        joined = [*([f"graphicsApi={api}"] if api else []), *parameters]
        return f"?{'&'.join(joined)}" if joined else ""

    def run(self, api: str | None, shard: SuiteShard | None) -> dict[str, object]:
        """Runs every check, or the checks of a shard, which take their places in the order of the checks."""
        if api is not None and api not in WebChecks.GRAPHICS_APIS:
            raise UnsupportedTarget(f'The checks of the web draw through "{WebChecks.GRAPHICS_APIS[0]}" or "{WebChecks.GRAPHICS_APIS[1]}", not "{api}".')
        checks = {
            "errorScreen": self.error_screen,
            "wakeLock": self.wake_lock,
            "fullscreen": self.fullscreen,
            "orientation": self.orientation,
            "plugins": self.plugins,
            "editor": self.editor,
            "memory": self.memory,
        }
        reports = {}
        for place, (name, check) in enumerate(checks.items()):
            if shard is None or shard.takes(place):
                reports[name] = check(api)
        return {"graphicsApi": api, "memory": reports.get("memory"), "shard": shard.describe() if shard else None}

    def web_player(self) -> Path:
        """The web player of the installed web SDK of the current engine sources, which the target of the web takes for a Debug build of a Lua app."""
        target = self.pipeline.registry.target(WebChecks.TARGET).description
        architecture = target.default_architectures[0]
        target_id = TargetId(target.system, architecture, target.abis[architecture], target.environment, Configuration.RELEASE, target.toolchain)
        selector = SdkSelector(tuple(SdkCatalog(self.context.environment).manifests()), Repository.engine_revision())
        return selector.select(target_id, target.name, Configuration.RELEASE, None, ()).file("player")

    def site(self, app: Path) -> Path:
        """Builds the site of a check app with the web backend in Release and verifies it."""
        request = ProductRequest(app, WebChecks.TARGET, Configuration.RELEASE, Configuration.RELEASE, None)
        product = self.pipeline.build(self.pipeline.prepare(self.pipeline.resolve(request, build_sdk=True)))
        ReportPrinter.verification(self.pipeline.verify(product), self.context.terminal, f"`{Repository.shown(product.path)}`")
        return product.path

    def drive(self, driver: str, browser: tuple[str, ...], purpose: str, *arguments: str) -> dict:
        """Opens a page in a headless browser of its own and returns what the driver reported."""
        with HeadlessBrowser(self.context, browser, purpose) as debugging:
            output = self.context.process.capture([Emscripten.node(), WebChecks.DRIVERS / driver, str(debugging), *arguments])
        return json.loads(output)

    @staticmethod
    def check_steps(report: dict, steps: list[str], page: str) -> None:
        """Fails on a page that reported an error or missed one of the steps of its mode, with the last lines of its log."""
        reached = [entry["name"] for entry in report["steps"] if entry["reached"]]
        if not report["failure"] and reached == steps:
            return
        log = "\n".join(report["log"][-20:])
        if report["failure"]:
            raise BuildFailed(f"{page} reported an error: {report['failure']} The last lines of its log follow.", details=log)
        missed = next(step for step in steps if step not in reached)
        raise BuildFailed(f'{page} missed the step "{missed}". The last lines of its log follow.', details=log)

    def error_screen(self, api: str | None) -> None:
        """The browsers of this check draw through SwiftShader like the GPU tests of the web, so WebGPU starts on a machine without a GPU for it."""
        site = self.site(WebChecks.DATA / "web-error-screen")
        purpose = "the error screen check of the web"
        with ServedSite(site) as served:
            for each in [api] if api else WebChecks.GRAPHICS_APIS:
                for mode, steps in WebChecks.ERROR_SCREEN_STEPS.items():
                    browser = WebChecks.ERROR_SCREEN_BROWSERS[mode]
                    self.context.terminal.step(f'Running the error screen check of the web on "{each}" in a headless browser that {browser}')
                    report = self.drive("error-screen-check.mjs", HeadlessBrowser.SOFTWARE_GPU, purpose, served.page(WebChecks.query(each)), mode)
                    WebChecks.check_steps(report, steps, f'The page of the error screen check on "{each}" in a browser that {browser}')
            # A page that names an unknown graphics API draws its error screen through the first API that starts, so it opens with the checks of that API.
            if api in (None, WebChecks.GRAPHICS_APIS[0]):
                self.context.terminal.step('Running the error screen check of the web with the unknown graphics API "bogus" in a headless browser')
                report = self.drive("error-screen-check.mjs", HeadlessBrowser.SOFTWARE_GPU, purpose, served.page(WebChecks.query("bogus")), "unknownApi")
                WebChecks.check_steps(report, WebChecks.UNKNOWN_API_STEPS, 'The page of the error screen check with the unknown graphics API "bogus"')
        unknown = "" if api == WebChecks.GRAPHICS_APIS[1] else " A page that named an unknown graphics API showed the reason."
        self.context.terminal.success(
            f"The error screen put its report on the clipboard where the browser allowed it, warned where the browser refused it or lacked it, and restarted the app from the key R and from its button, which drew again.{unknown}"
        )

    def wake_lock(self, api: str | None) -> None:
        site = self.site(WebChecks.DATA / "web-wake-lock")
        with ServedSite(site) as served:
            for mode, steps in WebChecks.WAKE_LOCK_STEPS.items():
                browser = WebChecks.WAKE_LOCK_BROWSERS[mode]
                self.context.terminal.step(f"Running the wake lock check of the web in a headless browser that {browser}")
                report = self.drive("wake-lock-check.mjs", (), "the wake lock check of the web", served.page(WebChecks.query(api)), mode)
                WebChecks.check_steps(report, steps, f"The page of the wake lock check in a browser that {browser}")
        self.context.terminal.success(
            "The page took the wake lock while the app kept the screen on, gave it up when it hid, took it again when it showed and released it when the app let the screen sleep, and a browser that refused it or lacked it only warned."
        )

    def fullscreen(self, api: str | None) -> None:
        site = self.site(WebChecks.DATA / "web-fullscreen")
        with ServedSite(site) as served:
            self.context.terminal.step("Running the fullscreen check of the web in a headless browser")
            report = self.drive("fullscreen-check.mjs", (), "the fullscreen check of the web", served.page(WebChecks.query(api)))
            WebChecks.check_steps(report, WebChecks.FULLSCREEN_STEPS, "The page of the fullscreen check")
        self.context.terminal.success(
            'Each change of fullscreen settled once the window reported its mode, and a change that a newer one replaced failed with the code "denied" while the page ended in the mode of the newer one.'
        )

    def orientation(self, api: str | None) -> None:
        site = self.site(WebChecks.DATA / "web-orientation")
        with ServedSite(site) as served:
            self.context.terminal.step("Running the orientation check of the web in a headless browser that emulates a phone")
            report = self.drive("orientation-check.mjs", (), "the orientation check of the web", served.page(WebChecks.query(api)))
            WebChecks.check_steps(report, WebChecks.ORIENTATION_STEPS, "The page of the orientation check")
        self.context.terminal.success(
            "A phone held in portrait showed the turn screen of the page over an app that takes landscape while the app ran on under it, the screen went away once the phone turned, and the lock of the app asked for portrait until the app let the screen turn."
        )

    def plugins(self, api: str | None) -> None:
        site = self.site(WebChecks.DATA / "web-plugins")
        with ServedSite(site) as served:
            self.context.terminal.step("Running the plugin check of the web in a headless browser whose permissions policy governs one feature")
            report = self.drive("plugin-check.mjs", (), "the plugin check of the web", served.page(WebChecks.query(api)))
            WebChecks.check_steps(report, WebChecks.PLUGIN_STEPS, "The page of the plugin check")
        self.context.terminal.success(
            'The page lacked the feature that its permissions policy refused, left the feature that the policy does not govern to the error of its API, failed the error of the page and a result that JSON cannot carry with the code "exception", the use without a prompt with the code "denied" and the use of an unregistered permission with the code "unsupported".'
        )

    def editor(self, api: str | None) -> None:
        """Serves the page of the editor check as the page of a site next to the web player of the web SDK, and opens it on its own and in the Worker mode in browsers that draw through SwiftShader like the GPU tests of the web."""
        purpose = "the editor check of the web"
        with tempfile.TemporaryDirectory() as folder:
            site = Path(folder)
            shutil.copytree(self.web_player(), site, dirs_exist_ok=True)
            shutil.copyfile(WebChecks.DRIVERS / WebChecks.EDITOR_PAGE, site / "index.html")
            with ServedSite(site) as served:
                self.context.terminal.step("Running the editor check of the web in a headless browser")
                report = self.drive("editor-check.mjs", HeadlessBrowser.SOFTWARE_GPU, purpose, served.page(WebChecks.query(api)))
                WebChecks.check_steps(report, WebChecks.EDITOR_STEPS, "The page of the editor check")

                self.context.terminal.step("Running the editor check of the web in the Worker mode in a headless browser")
                report = self.drive("editor-check.mjs", HeadlessBrowser.SOFTWARE_GPU, purpose, served.page(WebChecks.query(api, WebChecks.WORKER_HOSTING)), "worker")
                WebChecks.check_steps(report, WebChecks.WORKER_STEPS, "The page of the editor check in the Worker mode")
        self.context.terminal.success(
            "The editor of the page built, ran, reloaded, rejected, restored, stopped, inspected and disposed of a project through the editor contract of the web player, and the execution budget stopped code that never ends while the page answered, both with the runtime on the page and in the Worker mode, where the app also played without a refused audio output, took input, read a gamepad, mirrored its interface, answered through a plugin and played a video, and the editor terminated a worker that stopped answering."
        )

    def memory(self, api: str | None) -> dict:
        """Builds the release site of a copy of the memory check app with a medium and a large file in its content, and opens it with the files of the browser and without them."""
        folder = Repository.BUILD / "tests" / "web-memory"
        Folders.remove(folder)
        shutil.copytree(WebChecks.DATA / "web-memory", folder)
        large = folder / "content" / "large"
        large.mkdir(parents=True)
        for name, size in (("medium.bin", WebChecks.MEMORY_MEDIUM_SIZE), ("blob.bin", WebChecks.MEMORY_FILE_SIZE)):
            with (large / name).open("wb") as file:
                for _ in range(size // 1024**2):
                    file.write(os.urandom(1024**2))
        site = self.site(folder)

        reports = {}
        with ServedSite(site) as served:
            for store, browser in (("files", ""), ("memory", " that keeps no files for the page"), ("missing", " whose worker of the content store does not load")):
                self.context.terminal.step(f"Running the memory check of the web in a headless browser{browser}")
                reports[store] = self.drive("memory-check.mjs", (), "the memory check of the web", served.page(WebChecks.query(api)), str(WebChecks.MEMORY_SECONDS), store)
        WebChecks.check_memory(reports["files"])
        WebChecks.check_memory_without_files(reports["memory"], "A page whose browser keeps no files")
        WebChecks.check_memory_without_files(reports["missing"], "A page whose worker of the content store does not load")
        report = reports["files"]
        self.context.terminal.success(
            f"The page downloaded {report['receivedBytes']} bytes with a peak of {report['peakWasmBytes']} bytes of WebAssembly memory and kept {report['buffersAfterCollection']} bytes of array buffers, and a page without files or without the worker of its content store kept the medium file in memory and refused the large one for want of room."
        )
        return report

    @staticmethod
    def check_memory(report: dict) -> None:
        """Fails on a memory check whose page failed, never finished, missed the download or held memory that grows with the file."""
        if report["failure"] or not report["finished"]:
            ended = f"failed: {report['failure']}" if report["failure"] else "did not finish"
            raise BuildFailed(f"The page of the memory check of the web {ended}. Its last log lines follow.", details="\n".join(report["log"]))
        problems = []
        if report["receivedBytes"] < WebChecks.MEMORY_FILE_SIZE:
            problems.append(f"The page received {report['receivedBytes']} bytes, fewer than the {WebChecks.MEMORY_FILE_SIZE} bytes of the large file.")
        if report["peakWasmBytes"] > WebChecks.MEMORY_WASM_LIMIT:
            problems.append(
                f"The memory of WebAssembly grew to {report['peakWasmBytes']} bytes, more than {WebChecks.MEMORY_WASM_LIMIT} bytes, so the page holds memory that grows with what it downloads."
            )
        if report["buffersAfterCollection"] > WebChecks.MEMORY_BUFFER_LIMIT:
            problems.append(f"The page and its workers keep {report['buffersAfterCollection']} bytes of array buffers after a collection, more than {WebChecks.MEMORY_BUFFER_LIMIT} bytes.")
        if problems:
            raise BuildFailed(" ".join(problems))

    @staticmethod
    def check_memory_without_files(report: dict, page: str) -> None:
        """Fails on a page without the files of the browser, which `page` describes, that did not keep the medium file in memory, or that took the large one past its room instead of refusing it."""
        if not report["downloadedMedium"]:
            reason = f": {report['failure']}" if report["failure"] else ""
            raise BuildFailed(f"{page} did not download the medium file{reason}.")
        if report["finished"] or not report["failure"] or "of room" not in report["failure"]:
            outcome = "downloaded it" if report["finished"] else f"reported: {report['failure']}"
            raise BuildFailed(f"{page} should refuse the large file for want of room, and it {outcome}.")
