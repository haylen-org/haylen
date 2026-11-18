import json
from pathlib import Path

from haylen.cli.report_printer import ReportPrinter
from haylen.core.app_manifest import AppManifest
from haylen.core.app_pipeline import AppPipeline
from haylen.core.configuration import Configuration
from haylen.core.errors.build_failed import BuildFailed
from haylen.core.errors.invalid_manifest import InvalidManifest
from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.core.product_request import ProductRequest
from haylen.core.repository import Repository
from haylen.core.tooling_context import ToolingContext
from haylen.engine.headless_browser import HeadlessBrowser
from haylen.engine.served_site import ServedSite
from haylen.engine.web_checks import WebChecks
from haylen.toolchains.emscripten import Emscripten


class WebAppCheck:
    """Checks that an app starts on the web: the web backend builds its site in the configuration it is asked for, a headless browser that draws through SwiftShader opens the served page with `?graphicsApi=` for WebGPU and WebGL2, or for the one API it is asked for, and the driver `engine/tests/web/app-start-check.mjs` follows the start. The check fails on an error that the page throws, an error in its console, such as one that the engine logs, the error screen of the app, a start that the splash reports as failed, or an app that does not log the API it draws through or does not draw its first complete frame within `START_SECONDS`."""

    DRIVER = "app-start-check.mjs"
    START_SECONDS = 180
    # The line that the engine logs once its GPU device started, with the name of its graphics API.
    DRAWS_THROUGH = 'The app draws through "{}".'
    # The title of the error screen, which starts the report that the engine logs when it shows the screen.
    ERROR_SCREEN = "The app stopped with an error"
    LOG_LINES = 40
    # The level of a line of the console that fails the check, which the driver takes from a line of the engine or from the type of any other console call.
    ERROR = "error"

    def __init__(self, context: ToolingContext, pipeline: AppPipeline) -> None:
        self.context = context
        self.pipeline = pipeline

    def run(self, app: str, configuration: str, api: str | None) -> dict[str, object]:
        if api is not None and api not in WebChecks.GRAPHICS_APIS:
            raise UnsupportedTarget(f'The pages of the web draw through "{WebChecks.GRAPHICS_APIS[0]}" or "{WebChecks.GRAPHICS_APIS[1]}", not "{api}".')
        folder = Path(app).expanduser().resolve()
        if not AppManifest.is_app(folder):
            raise InvalidManifest(f'The folder `{Repository.shown(folder)}` holds no "{AppManifest.FILE}". Name the folder of the app whose page the check opens.')
        request = ProductRequest(folder, WebChecks.TARGET, Configuration.parse(configuration), Configuration.RELEASE, None)

        product = self.pipeline.build(self.pipeline.prepare(self.pipeline.resolve(request, build_sdk=True)))
        ReportPrinter.verification(self.pipeline.verify(product), self.context.terminal, f"`{Repository.shown(product.path)}`")
        name = product.prepared.product.app.name
        apis = [api] if api else list(WebChecks.GRAPHICS_APIS)
        with ServedSite(product.path) as served:
            for each in apis:
                self.context.terminal.step(f'Opening the page of "{name}" in the "{configuration}" configuration on "{each}" in a headless browser')
                with HeadlessBrowser(self.context, HeadlessBrowser.SOFTWARE_GPU, f'the start check of "{name}"') as debugging:
                    command = [Emscripten.node(), WebChecks.DRIVERS / WebAppCheck.DRIVER, str(debugging), served.page(WebChecks.query(each)), str(WebAppCheck.START_SECONDS)]
                    output = self.context.process.capture(command)
                WebAppCheck.check_report(json.loads(output), each, f'The page of "{name}" on "{each}"')
                self.context.terminal.success(f'The page of "{name}" started the app, which drew its first frames through "{each}" without an error.')
        return {"app": str(folder), "configuration": configuration, "graphicsApis": apis, "site": str(product.path)}

    @staticmethod
    def check_report(report: dict, api: str, page: str) -> None:
        """Fails with the first reason of a start that went wrong, in the order a person reads them, with the last lines of the console of the page."""
        log = report["log"]
        details = "\n".join(entry["text"] for entry in log[-WebAppCheck.LOG_LINES :])
        errors = [entry["text"] for entry in log if entry["level"] == WebAppCheck.ERROR]
        screen = next((text for text in errors if WebAppCheck.ERROR_SCREEN in text), None)
        expected = WebAppCheck.DRAWS_THROUGH.format(api)
        seconds = WebAppCheck.START_SECONDS

        if report["stalled"]:
            raise BuildFailed(f"The debugging protocol of the browser stopped answering while {page} started. The last lines of its console follow.", details=details)
        if report["exceptions"]:
            raise BuildFailed(f"{page} threw an error: {report['exceptions'][0]} The last lines of its console follow.", details=details)
        if screen is not None:
            raise BuildFailed(f"{page} showed the error screen of the app. The last lines of its console follow.", details=details)
        if report["splashFailure"] is not None:
            failure = report["splashFailure"]
            raise BuildFailed(f"{page} could not start the app: {failure['status']} {failure['details']}".rstrip() + " The last lines of its console follow.", details=details)
        if errors:
            raise BuildFailed(f"{page} logged an error in its console: {errors[0]} The last lines of its console follow.", details=details)
        if report["drawsThrough"] is None:
            raise BuildFailed(f'{page} did not log "{expected}" within {seconds} seconds. The last lines of its console follow.', details=details)
        if report["drawsThrough"] != api:
            raise BuildFailed(f'{page} drew through "{report["drawsThrough"]}" instead of "{api}", which its address asks for. The last lines of its console follow.', details=details)
        if not report["firstFrame"]:
            raise BuildFailed(f"{page} did not draw the first complete frame of the app within {seconds} seconds, since its splash never ended. The last lines of its console follow.", details=details)
