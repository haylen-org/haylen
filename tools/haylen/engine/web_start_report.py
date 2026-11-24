import re
from collections.abc import Mapping
from urllib.parse import urlparse

from haylen.core.errors.build_failed import BuildFailed
from haylen.platforms.web.web_project import WebProject


class WebStartReport:
    """The report of the driver `engine/tests/web/app-start-check.mjs`, which follows the start of the page of an app in a headless browser. The method `check` fails a start that went wrong, and `check_loading` fails a page whose splash or downloads went out of order."""

    DRIVER = "app-start-check.mjs"
    START_SECONDS = 180
    # The line that the engine logs once its GPU device started, with the name of its graphics API.
    DRAWS_THROUGH = 'The app draws through "{}".'
    # The title of the error screen, which starts the report that the engine logs when it shows the screen.
    ERROR_SCREEN = "The app stopped with an error"
    LOG_LINES = 40
    # The level of a line of the console that fails the check, which the driver takes from a line of the engine or from the type of any other console call.
    ERROR = "error"
    RUNTIME = "haylen.wasm"
    PERCENT = re.compile(r"(\d+)%")

    @staticmethod
    def check(report: dict, api: str, page: str) -> None:
        """Fails with the first reason of a start that went wrong, in the order a person reads them, with the last lines of the console of the page."""
        log = report["log"]
        details = "\n".join(entry["text"] for entry in log[-WebStartReport.LOG_LINES :])
        errors = [entry["text"] for entry in log if entry["level"] == WebStartReport.ERROR]
        screen = next((text for text in errors if WebStartReport.ERROR_SCREEN in text), None)
        expected = WebStartReport.DRAWS_THROUGH.format(api)
        seconds = WebStartReport.START_SECONDS

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

    @staticmethod
    def check_loading(report: dict, config: Mapping[str, object], page: str) -> None:
        """Fails a page that started and loaded out of order, with `config` as the `config.json` of its site: a percentage that went down or that had not reached 100% when the splash ended, a splash whose content showed before its logo, a logo that arrived after the runtime or the package started to download, or a page without `#splash-logo` whose splash showed an image or no bar."""
        states = report["splash"]
        shown = next((index for index, state in enumerate(states) if not state["starting"]), None)
        ended = next((index for index, state in enumerate(states) if state["ended"]), None)
        if shown is None or ended is None:
            raise BuildFailed(f"{page} reported no splash that showed and ended, so the driver could not follow its loading.")

        visible = states[shown : ended + 1]
        WebStartReport.check_percentages(visible, page)
        if states[shown]["logo"] is None:
            WebStartReport.check_bar_alone(visible, page)
        elif not states[shown]["logo"]:
            raise BuildFailed(f"{page} showed its splash before its logo arrived.")

        downloads = WebStartReport.downloads(report["resources"])
        release = config.get("release")
        files = [WebStartReport.RUNTIME, *(f"app/{name}" for name in release["files"])] if isinstance(release, Mapping) else [WebStartReport.RUNTIME, WebProject.ARCHIVE]
        missing = next((name for name in files if name not in downloads), None)
        if missing is not None:
            raise BuildFailed(f'{page} started the app without downloading "{missing}".')
        if states[shown]["logo"] is None:
            return

        logo = str(config["splash"]["logo"])
        if logo not in downloads:
            raise BuildFailed(f'{page} showed a logo without downloading "{logo}".')
        early = next((name for name in files if downloads[name]["start"] < downloads[logo]["end"]), None)
        if early is not None:
            raise BuildFailed(f'{page} started to download "{early}" before its logo "{logo}" arrived, so the splash waited on the logo behind the larger files.')

    @staticmethod
    def check_percentages(states: list[dict], page: str) -> None:
        """Fails percentages that go down or that end below 100%. A page without `#splash-percent` shows none."""
        texts = [state["percent"] for state in states if state["percent"] is not None]
        if not texts:
            return
        values = []
        for text in texts:
            match = WebStartReport.PERCENT.fullmatch(text)
            if match is None:
                raise BuildFailed(f'{page} showed "{text}" as the percentage of its splash, which is no percentage.')
            values.append(int(match.group(1)))

        fall = next((index for index in range(1, len(values)) if values[index] < values[index - 1]), None)
        if fall is not None:
            raise BuildFailed(f"{page} showed the percentage of its splash go down from {values[fall - 1]}% to {values[fall]}%.")
        if values[-1] != 100:
            raise BuildFailed(f"{page} ended its splash at {values[-1]}%, before every file of its start had downloaded.")

    @staticmethod
    def check_bar_alone(states: list[dict], page: str) -> None:
        """Fails a splash without `#splash-logo` that shows an image or hides its bar."""
        if any(state["images"] for state in states):
            raise BuildFailed(f'{page} has no "#splash-logo" and still showed an image in its splash.')
        if not all(state["bar"] for state in states):
            raise BuildFailed(f'{page} has no "#splash-logo" and did not show the bar of its splash.')

    @staticmethod
    def downloads(resources: list[dict]) -> dict[str, dict]:
        """The first download of every file of the page by its path from the root of the site."""
        downloads: dict[str, dict] = {}
        for resource in resources:
            path = urlparse(resource["url"]).path.lstrip("/")
            downloads.setdefault(path, resource)
        return downloads
