import pytest

from haylen.core.errors.build_failed import BuildFailed
from haylen.engine.web_start_report import WebStartReport


class WebStartReportTest:
    """The report of the start driver of the web passes only a page that drew its first frames through the API of its address without an error after a splash that loaded in order, and names the first reason of a start or a loading that went wrong."""

    PAGE = 'The page of "Tests" on "webGpu"'
    SITE = "http://127.0.0.1:8000/"
    # The `config.json` of the site of a protected release with its logo.
    CONFIG = {"splash": {"logo": "splash.png"}, "release": {"files": ["manifest.hman", "domain.hpak"]}}

    @staticmethod
    def state(percent: str | None, starting: bool = False, ended: bool = False, logo: bool | None = True, images: int = 1, bar: bool = True) -> dict:
        return {"percent": percent, "starting": starting, "ended": ended, "logo": logo, "images": images, "bar": bar}

    @staticmethod
    def resource(path: str, start: float, end: float) -> dict:
        return {"url": f"{WebStartReportTest.SITE}{path}", "start": start, "end": end}

    @staticmethod
    def report(**changes: object) -> dict:
        """The report of the driver for an app that started on WebGPU after a splash with its logo that loaded in order, with the fields that `changes` replaces."""
        state = WebStartReportTest.state
        resource = WebStartReportTest.resource
        started = {
            "drawsThrough": "webGpu",
            "firstFrame": True,
            "splashFailure": None,
            "exceptions": [],
            "log": [{"level": "info", "text": '[2026-11-16 10:00:00.000] [info] The app draws through "webGpu".'}],
            "splash": [state("0%", starting=True, logo=False, images=0), state("0%"), state("41%"), state("100%"), state("100%", ended=True)],
            "resources": [
                resource("config.json", 1.0, 2.0),
                resource("splash.png", 3.0, 4.0),
                resource("haylen.wasm", 6.0, 90.0),
                resource("app/manifest.hman", 6.0, 7.0),
                resource("app/domain.hpak", 6.0, 8.0),
                resource("app/content.hpak?range=0", 120.0, 121.0),
            ],
            "expired": False,
            "stalled": False,
        }
        return {**started, **changes}

    @staticmethod
    def failure(report: dict) -> str:
        with pytest.raises(BuildFailed) as failed:
            WebStartReport.check(report, "webGpu", WebStartReportTest.PAGE)
        return failed.value.message

    @staticmethod
    def loading_failure(report: dict, config: dict = CONFIG) -> str:
        with pytest.raises(BuildFailed) as failed:
            WebStartReport.check_loading(report, config, WebStartReportTest.PAGE)
        return failed.value.message

    def test_an_app_that_drew_its_first_frames_through_the_api_of_its_address_after_a_splash_that_loaded_in_order_passes(self):
        warning = {"level": "warning", "text": "[2026-11-16 10:00:00.000] [warning] The browser keeps no files for the page."}
        report = WebStartReportTest.report(log=[*WebStartReportTest.report()["log"], warning])
        WebStartReport.check(report, "webGpu", WebStartReportTest.PAGE)
        WebStartReport.check_loading(report, WebStartReportTest.CONFIG, WebStartReportTest.PAGE)

    def test_the_error_screen_of_the_app_fails_the_check_with_its_report_in_the_details(self):
        screen = '[2026-11-16 10:00:00.000] [error] The app stopped with an error\n\nThe chunk "87eb" is not on the device yet, so its file must be downloaded before it is read.'
        report = WebStartReportTest.report(firstFrame=False, log=[*WebStartReportTest.report()["log"], {"level": "error", "text": screen}])
        with pytest.raises(BuildFailed, match="showed the error screen of the app") as failed:
            WebStartReport.check(report, "webGpu", WebStartReportTest.PAGE)
        assert "is not on the device yet" in failed.value.details

    def test_each_failure_of_the_start_names_its_reason(self):
        error = {"level": "error", "text": "Saving user data failed."}
        cases = {
            "threw an error: TypeError: x is undefined at app.js:1:2": WebStartReportTest.report(exceptions=["TypeError: x is undefined at app.js:1:2"]),
            "could not start the app: The app could not be loaded. The file was missing.": WebStartReportTest.report(
                drawsThrough=None, firstFrame=False, splashFailure={"status": "The app could not be loaded.", "details": "The file was missing."}
            ),
            "logged an error in its console: Saving user data failed.": WebStartReportTest.report(log=[error]),
            'did not log "The app draws through "webGpu"." within': WebStartReportTest.report(drawsThrough=None, firstFrame=False, expired=True, log=[]),
            'drew through "webGl2" instead of "webGpu"': WebStartReportTest.report(drawsThrough="webGl2"),
            "did not draw the first complete frame of the app": WebStartReportTest.report(firstFrame=False, expired=True),
            "stopped answering": WebStartReportTest.report(stalled=True),
        }
        for reason, report in cases.items():
            assert reason in WebStartReportTest.failure(report)

    def test_each_loading_out_of_order_names_its_reason(self):
        state = WebStartReportTest.state
        resource = WebStartReportTest.resource
        resources = WebStartReportTest.report()["resources"]
        cases = {
            "showed the percentage of its splash go down from 41% to 30%.": WebStartReportTest.report(splash=[state("0%"), state("41%"), state("30%"), state("100%", ended=True)]),
            "ended its splash at 87%, before every file of its start had downloaded.": WebStartReportTest.report(splash=[state("0%"), state("87%"), state("87%", ended=True)]),
            'showed "Loading" as the percentage of its splash': WebStartReportTest.report(splash=[state("Loading"), state("100%", ended=True)]),
            "showed its splash before its logo arrived.": WebStartReportTest.report(splash=[state("0%", logo=False, images=0), state("100%"), state("100%", ended=True)]),
            'started to download "haylen.wasm" before its logo "splash.png" arrived': WebStartReportTest.report(
                resources=[resource("haylen.wasm", 2.0, 90.0), resource("splash.png", 3.0, 4.0), *resources[3:]]
            ),
            'started to download "app/domain.hpak" before its logo "splash.png" arrived': WebStartReportTest.report(
                resources=[resource("splash.png", 3.0, 4.0), resource("haylen.wasm", 6.0, 90.0), resource("app/manifest.hman", 6.0, 7.0), resource("app/domain.hpak", 3.5, 8.0)]
            ),
            'showed a logo without downloading "splash.png".': WebStartReportTest.report(resources=resources[2:]),
            'started the app without downloading "app/domain.hpak".': WebStartReportTest.report(resources=resources[:4]),
            "reported no splash that showed and ended": WebStartReportTest.report(splash=[state("0%", starting=True), state("100%", starting=True)]),
        }
        for reason, report in cases.items():
            assert reason in WebStartReportTest.loading_failure(report)

    def test_a_page_without_a_logo_shows_the_bar_alone_and_downloads_the_package_of_development(self):
        state = WebStartReportTest.state
        resource = WebStartReportTest.resource
        config = {"splash": {"logo": "splash.svg"}}
        resources = [resource("haylen.wasm", 2.0, 90.0), resource("app.zip", 2.0, 5.0)]
        alone = [state("0%", starting=True, logo=None, images=0), state("0%", logo=None, images=0), state("100%", logo=None, images=0), state("100%", ended=True, logo=None, images=0)]
        WebStartReport.check_loading(WebStartReportTest.report(splash=alone, resources=resources), config, WebStartReportTest.PAGE)

        image = [alone[0], state("0%", logo=None, images=1), alone[3]]
        assert 'has no "#splash-logo" and still showed an image in its splash.' in WebStartReportTest.loading_failure(WebStartReportTest.report(splash=image, resources=resources), config)
        hidden = [alone[0], state("0%", logo=None, images=0, bar=False), alone[3]]
        assert 'has no "#splash-logo" and did not show the bar of its splash.' in WebStartReportTest.loading_failure(WebStartReportTest.report(splash=hidden, resources=resources), config)
        assert 'without downloading "app.zip".' in WebStartReportTest.loading_failure(WebStartReportTest.report(splash=alone, resources=resources[:1]), config)

    def test_a_page_without_a_percentage_skips_only_the_percentage(self):
        state = WebStartReportTest.state
        WebStartReport.check_loading(WebStartReportTest.report(splash=[state(None), state(None, ended=True)]), WebStartReportTest.CONFIG, WebStartReportTest.PAGE)
