import json
from pathlib import Path

from haylen.cli.report_printer import ReportPrinter
from haylen.core.app_manifest import AppManifest
from haylen.core.app_pipeline import AppPipeline
from haylen.core.configuration import Configuration
from haylen.core.errors.invalid_manifest import InvalidManifest
from haylen.core.errors.unsupported_target import UnsupportedTarget
from haylen.core.product_request import ProductRequest
from haylen.core.repository import Repository
from haylen.core.tooling_context import ToolingContext
from haylen.engine.headless_browser import HeadlessBrowser
from haylen.engine.served_site import ServedSite
from haylen.engine.web_checks import WebChecks
from haylen.engine.web_start_report import WebStartReport
from haylen.platforms.web.web_project import WebProject
from haylen.toolchains.emscripten import Emscripten


class WebAppCheck:
    """Checks that an app starts on the web: the web backend builds its site in the configuration it is asked for, a headless browser that draws through SwiftShader opens the served page with `?graphicsApi=` for WebGPU and WebGL2, or for the one API it is asked for, and the driver `engine/tests/web/app-start-check.mjs` follows the start. The check fails on an error that the page throws, an error in its console, such as one that the engine logs, the error screen of the app, a start that the splash reports as failed, or an app that does not log the API it draws through or does not draw its first complete frame within the seconds of `WebStartReport`, and on a page that loads out of order: a percentage of the splash that goes down or that ends below 100%, a logo that arrives after the runtime and the package start to download, or a splash without a logo that shows an image or no bar."""

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
        config = json.loads((product.path / WebProject.CONFIG).read_text(encoding="utf-8"))
        apis = [api] if api else list(WebChecks.GRAPHICS_APIS)
        with ServedSite(product.path) as served:
            for each in apis:
                self.context.terminal.step(f'Opening the page of "{name}" in the "{configuration}" configuration on "{each}" in a headless browser')
                with HeadlessBrowser(self.context, HeadlessBrowser.SOFTWARE_GPU, f'the start check of "{name}"') as debugging:
                    command = [Emscripten.node(), WebChecks.DRIVERS / WebStartReport.DRIVER, str(debugging), served.page(WebChecks.query(each)), str(WebStartReport.START_SECONDS)]
                    output = self.context.process.capture(command)
                report = json.loads(output)
                page = f'The page of "{name}" on "{each}"'
                WebStartReport.check(report, each, page)
                WebStartReport.check_loading(report, config, page)
                self.context.terminal.success(f'The page of "{name}" started the app, which drew its first frames through "{each}" without an error after a splash that loaded in order.')
        return {"app": str(folder), "configuration": configuration, "graphicsApis": apis, "site": str(product.path)}
